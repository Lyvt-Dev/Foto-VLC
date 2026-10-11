#include "image.hpp"
#include "core.hpp"
#include "decoders.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

std::wstring hrText(HRESULT hr) {
    std::wostringstream s; s<<L"0x"<<std::hex<<std::uppercase<<static_cast<unsigned long>(hr); return s.str();
}
Decode wicLoad(const fs::path& path, unsigned maxEdge) {
    Com<IWICImagingFactory> factory;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out()));
    if(FAILED(hr)) return {{},L"Windows Imaging Component: "+hrText(hr)};
    Com<IWICBitmapDecoder> decoder;
    hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,decoder.out());
    if(FAILED(hr)) return {{},L"Kein passender Windows-Decoder ("+hrText(hr)+L")."};
    Com<IWICBitmapFrameDecode> frame;
    hr=decoder->GetFrame(0,frame.out());
    if(FAILED(hr)) return {{},L"Das erste Bild konnte nicht gelesen werden."};
    auto pic=std::make_shared<Picture>();
    if(FAILED(frame->GetSize(&pic->originalW,&pic->originalH)) || !pic->originalW || !pic->originalH)
        return {{},L"Ungültige Bildabmessungen."};
    decoder->GetFrameCount(&pic->frames);
    double factor=std::min({1.0,16384.0/pic->originalW,16384.0/pic->originalH,
        std::sqrt(32000000.0/(double(pic->originalW)*pic->originalH))});
    if(maxEdge)factor=std::min({factor,double(maxEdge)/pic->originalW,double(maxEdge)/pic->originalH});
    pic->width=std::max(1u,UINT(pic->originalW*factor));
    pic->height=std::max(1u,UINT(pic->originalH*factor));
    Com<IWICBitmapScaler> scaler;
    IWICBitmapSource* source=frame;
    if(factor<1.0) {
        hr=factory->CreateBitmapScaler(scaler.out());
        if(SUCCEEDED(hr)) hr=scaler->Initialize(frame,pic->width,pic->height,WICBitmapInterpolationModeFant);
        if(FAILED(hr)) return {{},L"Bild ist zu groß zum sicheren Anzeigen."};
        source=scaler;
    }
    Com<IWICFormatConverter> converter;
    hr=factory->CreateFormatConverter(converter.out());
    if(SUCCEEDED(hr)) hr=converter->Initialize(source,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if(FAILED(hr)) return {{},L"Die Pixelfarben konnten nicht decodiert werden."};
    pic->pixels.resize(size_t(pic->width)*pic->height*4);
    hr=converter->CopyPixels(nullptr,pic->width*4,UINT(pic->pixels.size()),pic->pixels.data());
    if(FAILED(hr)) return {{},L"Die Datei ist beschädigt oder unvollständig ("+hrText(hr)+L")."};
    Com<IWICMetadataQueryReader> metadata;
    int orientation=1;
    if(SUCCEEDED(frame->GetMetadataQueryReader(metadata.out()))) {
        for(const auto* query:{L"/app1/ifd/{ushort=274}",L"/ifd/{ushort=274}"}) {
            PROPVARIANT value;PropVariantInit(&value);
            if(SUCCEEDED(metadata->GetMetadataByName(query,&value)) && value.vt==VT_UI2)orientation=value.uiVal;
            PropVariantClear(&value);
            if(orientation!=1)break;
        }
    }
    if(orientation>=2 && orientation<=8) {
        foto::orientBGRA(pic->pixels,pic->width,pic->height,orientation);
        if(orientation>=5)std::swap(pic->originalW,pic->originalH);
    }
    pic->engine=L"Windows WIC";
    return {pic,{}};
}
Decode embeddedLoad(const fs::path& path) {
    std::error_code ec;auto size=fs::file_size(path,ec);
    if(ec || size>128u*1024*1024)return {{},L"Datei überschreitet das Limit des eingebetteten Decoders."};
    std::ifstream input(path,std::ios::binary);
    std::vector<std::uint8_t> data(static_cast<size_t>(size));
    if(!input.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size())))return {{},L"Datei nicht lesbar."};
    auto suffix=path.extension().wstring();std::string ext;
    for(auto c:suffix)if(c!=L'.')ext+=c<128?char(std::tolower(int(c))):'?';
    auto raster=foto::embedded(data,ext);
    if(!raster)return {{},std::wstring(raster.error.begin(),raster.error.end())};
    auto pic=std::make_shared<Picture>();pic->width=pic->originalW=raster.width;pic->height=pic->originalH=raster.height;
    pic->pixels=std::move(raster.rgba);pic->engine=std::wstring(raster.engine.begin(),raster.engine.end());
    for(size_t i=0;i<pic->pixels.size();i+=4) {
        unsigned a=pic->pixels[i+3],r=pic->pixels[i],g=pic->pixels[i+1],b=pic->pixels[i+2];
        pic->pixels[i]=BYTE((b*a+127)/255);pic->pixels[i+1]=BYTE((g*a+127)/255);pic->pixels[i+2]=BYTE((r*a+127)/255);
    }
    return {pic,{}};
}
std::wstring magickPath() {
    wchar_t configured[32768]{};
    DWORD length=GetEnvironmentVariableW(L"FOTO_VLC_MAGICK",configured,32768);
    if(length && length<32768) {
        fs::path p(configured); std::error_code ec;
        if(p.is_absolute() && fs::is_regular_file(p,ec)) return p.wstring();
        return {};
    }
    // Search PATH explicitly, excluding the working directory from executable lookup.
    wchar_t paths[32768]{};
    length=GetEnvironmentVariableW(L"PATH",paths,32768);
    if(!length || length>=32768) return {};
    std::wstring list=paths;
    for(size_t i=0;i<list.size();) {
        auto end=list.find(L';',i); if(end==std::wstring::npos) end=list.size();
        auto part=list.substr(i,end-i); i=end+1;
        if(part.size()>1 && part.front()==L'"' && part.back()==L'"') part=part.substr(1,part.size()-2);
        fs::path folder(part); std::error_code ec;
        if(folder.is_absolute() && fs::is_regular_file(folder/L"magick.exe",ec)) return (folder/L"magick.exe").wstring();
    }
    return {};
}
TempDirectory::TempDirectory() {
    wchar_t base[MAX_PATH]{},name[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,base) || !GetTempFileNameW(base,L"fvc",0,name)) return;
    DeleteFileW(name);
    if(CreateDirectoryW(name,nullptr)) path=name;
}
TempDirectory::~TempDirectory(){if(!path.empty()){std::error_code ec;fs::remove_all(path,ec);}}

Decode load(const fs::path& path) {
    std::error_code ec;
    if(!fs::is_regular_file(path,ec)) return {{},L"Die Datei wurde verschoben oder ist nicht lesbar."};
    auto size=fs::file_size(path,ec);
    if(ec || size>512ull*1024*1024) return {{},L"Die Datei überschreitet das Eingabelimit von 512 MiB."};
    auto native=wicLoad(path);
    if(native.image) return native;
    auto bundled=embeddedLoad(path);
    if(bundled.image)return bundled;
    auto exe=magickPath();
    if(exe.empty()) return {{},native.error+L"\nFür weitere Formate: ImageMagick 7 installieren und FOTO_VLC_MAGICK auf magick.exe setzen.\nDie Endung allein garantiert keinen Decoder; proprietäre Projekte können nicht verfügbar sein."};
    TempDirectory temp;
    if(temp.path.empty()) return {{},L"Temporärer Ordner konnte nicht erstellt werden."};
    const auto output=temp.path/L"preview.bmp";
    // Input is an absolute filename, with an explicit first-frame selector; no shell.
    // WIC fallback is content based. Magick chooses an installed coder for the extension/content.
    std::vector<std::wstring> args={exe,L"-limit",L"memory",L"128MiB",L"-limit",L"map",L"256MiB",
        L"-limit",L"disk",L"256MiB",L"-limit",L"thread",L"2",L"-limit",L"time",L"15",
        L"-define",L"registry:temporary-path="+temp.path.wstring(),
        fs::absolute(path).wstring()+L"[0]",L"-auto-orient",L"-thumbnail",L"6000x6000>",L"BMP:"+output.wstring()};
    std::wstring cmd;
    for(const auto& arg:args){if(!cmd.empty())cmd+=L' ';cmd+=foto::quote(arg);}
    HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if(!job) return {{},L"Decoder-Isolation konnte nicht erstellt werden."};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit=768ull*1024*1024;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) {
        CloseHandle(job); return {{},L"Decoder-Ressourcenlimit konnte nicht aktiviert werden."};
    }
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION process{};
    BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,temp.path.c_str(),&si,&process);
    if(!ok){CloseHandle(job);return {{},L"ImageMagick konnte nicht gestartet werden."};}
    if(!AssignProcessToJobObject(job,process.hProcess)) {
        TerminateProcess(process.hProcess,1); CloseHandle(process.hThread);CloseHandle(process.hProcess);CloseHandle(job);
        return {{},L"Decoder-Isolation fehlgeschlagen. Bitte Windows 10/11 verwenden."};
    }
    ResumeThread(process.hThread);
    DWORD wait=WaitForSingleObject(process.hProcess,20000),exitCode=1;
    if(wait==WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess,&exitCode);
    else TerminateJobObject(job,1);
    CloseHandle(job);
    WaitForSingleObject(process.hProcess,2000);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    if(wait!=WAIT_OBJECT_0) return {{},L"Decoder nach 20 Sekunden gestoppt. Datei ist zu komplex oder beschädigt."};
    if(exitCode!=0) return {{},L"ImageMagick kann diese Datei nicht lesen.\nEin Codec/Delegate fehlt, das Format ist proprietär, oder die Datei ist beschädigt.\nPDF/EPS/AI benötigen ggf. Ghostscript und eine passende ImageMagick-Sicherheitsrichtlinie."};
    auto decoded=wicLoad(output);
    if(decoded.image) decoded.image->engine=L"ImageMagick · Vorschau";
    return decoded;
}

