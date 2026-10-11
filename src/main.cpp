#include <windows.h>
#include <windowsx.h>
#include <wincodec.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cmath>
#include "core.hpp"
#include "decoders.hpp"

using namespace Gdiplus;
namespace fs=std::filesystem;
constexpr UINT Done=WM_APP+1;
constexpr wchar_t AppClass[]=L"FotoVLC.Main";

template<class T> struct Com {
    T* p=nullptr;
    ~Com(){if(p)p->Release();}
    T** out(){if(p)p->Release();p=nullptr;return &p;}
    T* operator->()const{return p;}
    operator T*()const{return p;}
    Com()=default;
    Com(const Com&)=delete;
};
struct Picture {
    UINT width=0,height=0,originalW=0,originalH=0,frames=1;
    std::vector<BYTE> pixels;
    std::wstring engine;
};
struct Decode { std::shared_ptr<Picture> image; std::wstring error; };
std::wstring hrText(HRESULT hr) {
    std::wostringstream s; s<<L"0x"<<std::hex<<std::uppercase<<static_cast<unsigned long>(hr); return s.str();
}
Decode wicLoad(const fs::path& path) {
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
struct TempDirectory {
    fs::path path;
    TempDirectory() {
        wchar_t base[MAX_PATH]{},name[MAX_PATH]{};
        if(!GetTempPathW(MAX_PATH,base) || !GetTempFileNameW(base,L"fvc",0,name)) return;
        DeleteFileW(name);
        if(CreateDirectoryW(name,nullptr)) path=name;
    }
    ~TempDirectory(){if(!path.empty()){std::error_code ec;fs::remove_all(path,ec);}}
};
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

struct Hit { RECT box; int command; };
enum {Open=1,Folder,Prev,Next,Fit,Actual,ZoomOut,ZoomIn,Rotate,Full,Info,Formats,Slide};
struct Job { fs::path path; unsigned serial; };
struct Completion { unsigned serial; Decode decoded; };
struct App {
    HWND window=nullptr;
    std::vector<fs::path> files;
    size_t index=0;
    std::shared_ptr<Picture> picture;
    std::wstring error;
    bool busy=false,info=true,fullscreen=false,slideshow=false,fit=true,dragging=false,keyboardFocus=false;
    int focusCommand=Open;
    double zoom=1,panX=0,panY=0;
    int rotation=0;
    POINT drag{};
    UINT dpi=96;
    RECT restore{};
    DWORD restoreStyle=0;
    std::vector<Hit> hits;
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<Job> queued;
    std::optional<Completion> completed;
    std::thread worker;
    bool stopping=false;
    unsigned serial=0;
    void begin() {
        worker=std::thread([this]{
            HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            for(;;) {
                Job task;
                {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[this]{return stopping||queued.has_value();});
                 if(stopping)break;
                 task=*queued;queued.reset();}
                Decode result;
                try {result=load(task.path);} catch(const std::exception&) {result.error=L"Datei konnte nicht sicher geladen werden.";}
                {std::lock_guard<std::mutex> lock(mutex);if(stopping)break;completed=Completion{task.serial,std::move(result)};}
                PostMessageW(window,Done,0,0);
            }
            if(SUCCEEDED(initialized))CoUninitialize();
        });
    }
    void shutdown() {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;queued.reset();}
        cv.notify_one(); if(worker.joinable())worker.join();
    }
    void request() {
        if(files.empty())return;
        ++serial; busy=true;error.clear();picture.reset();fit=true;zoom=1;rotation=0;panX=panY=0;
        SetWindowTextW(window,(files[index].filename().wstring()+L" — Foto VLC").c_str());
        {std::lock_guard<std::mutex> lock(mutex);queued=Job{files[index],serial};}
        cv.notify_one();InvalidateRect(window,nullptr,FALSE);
    }
    bool known(const fs::path& p) {
        auto ext=p.extension().wstring();
        std::string ascii;for(auto c:ext) ascii+=c<128?char(c):'?';
        return foto::recognized("file"+ascii);
    }
    void openPath(const fs::path& incoming) {
        std::error_code ec;
        auto path=fs::absolute(incoming,ec); if(ec)return;
        bool directory=fs::is_directory(path,ec);
        auto root=directory?path:path.parent_path();
        std::vector<fs::path> found;
        fs::directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
        for(;!ec && it!=end;it.increment(ec)){
            std::error_code e;
            if(it->is_regular_file(e) && known(it->path()))found.push_back(it->path());
        }
        // A user-selected file can be content-decoded even with an unfamiliar extension.
        if(!directory && fs::is_regular_file(path,ec) && std::find(found.begin(),found.end(),path)==found.end())found.push_back(path);
        std::sort(found.begin(),found.end(),[](const auto& a,const auto& b){return _wcsicmp(a.filename().c_str(),b.filename().c_str())<0;});
        if(found.empty()){
            ++serial;files.clear();picture.reset();busy=false;error=L"Dieser Ordner enthält keine erkannten Bilddateien.";
            InvalidateRect(window,nullptr,FALSE);return;
        }
        files=std::move(found);
        auto chosen=std::find(files.begin(),files.end(),path);
        index=chosen==files.end()?0:size_t(chosen-files.begin());request();
    }
};
App app;

Color bg(255,15,18,24),panel(255,23,27,35),border(255,43,49,61),accent(255,128,220,174),text(255,236,240,246),muted(255,147,159,177);
void rectangle(Graphics& g,Color c,float x,float y,float w,float h) {SolidBrush b(c);g.FillRectangle(&b,x,y,w,h);}
void rounded(Graphics& g,Color c,float x,float y,float w,float h,float radius=10) {
    GraphicsPath p;float d=radius*2;
    p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();
    SolidBrush b(c);g.FillPath(&b,&p);
}
void label(Graphics& g,const std::wstring& s,float x,float y,float w,float h,float size=12,Color color=text,bool bold=false,bool center=false) {
    FontFamily family(L"Segoe UI");
    Font font(family.GetLastStatus()==Ok?&family:FontFamily::GenericSansSerif(),size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush brush(color);
    StringFormat format;format.SetTrimming(StringTrimmingEllipsisCharacter);format.SetLineAlignment(StringAlignmentCenter);
    if(center)format.SetAlignment(StringAlignmentCenter);
    g.DrawString(s.c_str(),-1,&font,RectF(x,y,w,h),&format,&brush);
}
void button(Graphics& g,const std::wstring& caption,float x,float y,float w,int command,bool active=false) {
    rounded(g,active?Color(255,38,67,57):Color(255,33,39,49),x,y,w,34,8);
    label(g,caption,x,y,w,34,12,active?accent:text,false,true);
    if(app.keyboardFocus && app.focusCommand==command){Pen outline(accent,1);g.DrawRectangle(&outline,x+2,y+2,w-4,30.0f);}
    app.hits.push_back({{LONG(x),LONG(y),LONG(x+w),LONG(y+34)},command});
}
RectF canvas(float w,float h) {
    float sidebar=app.info && w>=860?218.0f:0.0f;
    return RectF(20,132,std::max(1.0f,w-40-sidebar),std::max(1.0f,h-212));
}
void paint(HWND window) {
    PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT r;GetClientRect(window,&r);
    int pw=r.right,ph=r.bottom;if(pw<1||ph<1){EndPaint(window,&ps);return;}
    HDC mem=CreateCompatibleDC(dc);HBITMAP buffer=CreateCompatibleBitmap(dc,pw,ph);auto old=SelectObject(mem,buffer);
    {
        Graphics g(mem);float scale=app.dpi/96.0f;g.ScaleTransform(scale,scale);
        float w=pw/scale,h=ph/scale;app.hits.clear();g.SetSmoothingMode(SmoothingModeAntiAlias);
        rectangle(g,bg,0,0,w,h);
        // Brand mark: a compact photo tile.
        rounded(g,accent,20,20,34,34,9);
        Point triangle[]={{25,47},{36,31},{49,47}};SolidBrush dark(bg);g.FillPolygon(&dark,triangle,3);
        label(g,L"Foto VLC",65,17,180,27,21,text,true);
        label(g,L"Deine Bilder. Direkt im Fokus.",65,43,250,20,11,muted);
        button(g,L"Datei öffnen",w-230,25,112,Open,true);button(g,L"Ordner",w-108,25,88,Folder);
        rectangle(g,border,20,76,w-40,1);
        float x=20;
        button(g,L"←",x,88,36,Prev);x+=42;button(g,L"→",x,88,36,Next);x+=50;
        button(g,L"Einpassen",x,88,88,Fit,app.fit);x+=94;button(g,L"1:1",x,88,44,Actual);x+=50;
        button(g,L"−",x,88,34,ZoomOut);x+=40;button(g,L"+",x,88,34,ZoomIn);x+=45;
        button(g,L"Drehen",x,88,66,Rotate);x+=72;button(g,L"Vollbild",x,88,76,Full);x+=82;
        if(w>800){button(g,app.slideshow?L"Stopp":L"Diashow",x,88,76,Slide,app.slideshow);x+=82;}
        if(w>940)button(g,L"Info",x,88,48,Info,app.info);
        RectF c=canvas(w,h);
        rounded(g,Color(255,19,23,30),c.X,c.Y,c.Width,c.Height,12);
        if(app.picture) {
            auto& p=*app.picture;
            double rw=app.rotation%2?p.height:p.width,rh=app.rotation%2?p.width:p.height;
            if(app.fit)app.zoom=std::min((c.Width-32)/rw,(c.Height-32)/rh);
            double dw=rw*app.zoom,dh=rh*app.zoom;
            auto saved=g.Save();g.SetClip(c);
            float left=c.X+(c.Width-float(dw))/2+float(app.panX),top=c.Y+(c.Height-float(dh))/2+float(app.panY);
            // Transparency pattern stays inside the visible image bounds.
            RectF imageRect(left,top,float(dw),float(dh));g.SetClip(imageRect,CombineModeIntersect);
            rectangle(g,Color(255,32,37,46),left,top,float(dw),float(dh));
            float sx=std::max(c.X,left),sy=std::max(c.Y,top),ex=std::min(c.GetRight(),left+float(dw)),ey=std::min(c.GetBottom(),top+float(dh));
            for(int yy=int(sy)/16*16;yy<ey;yy+=16)for(int xx=int(sx)/16*16;xx<ex;xx+=16)
                if(((xx/16)+(yy/16))%2)rectangle(g,Color(255,40,46,55),float(xx),float(yy),16,16);
            g.TranslateTransform(c.X+c.Width/2+float(app.panX),c.Y+c.Height/2+float(app.panY));
            g.RotateTransform(float(app.rotation*90));
            g.SetInterpolationMode(app.zoom>3?InterpolationModeNearestNeighbor:InterpolationModeHighQualityBicubic);
            Bitmap bitmap(p.width,p.height,p.width*4,PixelFormat32bppPARGB,p.pixels.data());
            g.DrawImage(&bitmap,RectF(float(-double(p.width)*app.zoom/2),float(-double(p.height)*app.zoom/2),float(p.width*app.zoom),float(p.height*app.zoom)));
            g.Restore(saved);
        } else if(app.busy) {
            label(g,L"Bild wird geladen …",c.X,c.Y+c.Height/2-35,c.Width,40,22,text,true,true);
            label(g,L"Decoder arbeitet im Hintergrund",c.X,c.Y+c.Height/2+7,c.Width,25,12,muted,false,true);
        } else if(!app.error.empty()) {
            label(g,L"Keine Vorschau verfügbar",c.X+25,c.Y+c.Height/2-100,c.Width-50,40,22,text,true,true);
            label(g,app.error,c.X+40,c.Y+c.Height/2-50,c.Width-80,140,13,muted,false,true);
        } else {
            rounded(g,Color(255,33,49,43),c.X+c.Width/2-32,c.Y+c.Height/2-105,64,64,18);
            label(g,L"+",c.X+c.Width/2-32,c.Y+c.Height/2-105,64,64,36,accent,false,true);
            label(g,L"Platz für deine Perspektive.",c.X,c.Y+c.Height/2-26,c.Width,42,26,text,true,true);
            label(g,L"Ziehe ein Foto oder einen Ordner hierher.",c.X,c.Y+c.Height/2+22,c.Width,24,13,muted,false,true);
            button(g,L"Fotos entdecken",c.X+c.Width/2-73,c.Y+c.Height/2+62,146,Open,true);
        }
        if(app.info && w>=860) {
            float side=w-220;
            rounded(g,panel,side,c.Y,200,c.Height,12);
            label(g,L"BILDINFORMATION",side+16,c.Y+15,168,26,10,muted,true);
            if(!app.files.empty()) {
                label(g,app.files[app.index].filename().wstring(),side+16,c.Y+49,168,62,16,text,true);
                auto extension=app.files[app.index].extension().wstring();
                label(g,L"FORMAT",side+16,c.Y+125,168,20,10,muted,true);
                label(g,extension,side+16,c.Y+147,168,28,15);
                std::error_code ec;auto bytes=fs::file_size(app.files[app.index],ec);
                std::wostringstream size;size<<std::fixed<<std::setprecision(2)<<bytes/1048576.0<<L" MiB";
                label(g,L"DATEIGRÖSSE",side+16,c.Y+189,168,20,10,muted,true);
                label(g,ec?L"—":size.str(),side+16,c.Y+211,168,28,15);
            }
            if(app.picture) {
                auto& p=*app.picture;
                label(g,L"AUFLÖSUNG",side+16,c.Y+254,168,20,10,muted,true);
                label(g,std::to_wstring(p.originalW)+L" × "+std::to_wstring(p.originalH),side+16,c.Y+276,168,28,15);
                label(g,p.engine,side+16,c.Y+316,168,28,11,accent);
                if(p.frames>1)label(g,L"Erstes Bild / erste Seite",side+16,c.Y+348,168,36,11,muted);
                if(p.width!=p.originalW || p.height!=p.originalH)label(g,L"Speicherschonende Vorschau",side+16,c.Y+387,168,36,11,muted);
            } else label(g,L"Nur betrachten.\nOriginale bleiben unverändert.",side+16,c.Y+258,168,70,12,muted);
        }
        rectangle(g,border,20,h-64,w-40,1);
        std::wstring footer=L"Bereit · Drag-and-drop oder Strg+O";
        if(!app.files.empty())footer=std::to_wstring(app.index+1)+L" / "+std::to_wstring(app.files.size())+L"   ·   "+app.files[app.index].filename().wstring();
        label(g,footer,20,h-52,w-310,36,12,muted);
        if(app.picture)label(g,std::to_wstring(int(app.zoom*100))+L" %",w-265,h-52,65,36,12,accent,false,true);
        button(g,L"Formate & Hilfe",w-170,h-52,150,Formats);
    }
    BitBlt(dc,0,0,pw,ph,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(buffer);DeleteDC(mem);EndPaint(window,&ps);
}
void choose(bool folder) {
    if(folder) {
        Com<IFileOpenDialog> dialog;
        if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.out())))) {
            DWORD flags=0;dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);
            if(SUCCEEDED(dialog->Show(app.window))) {
                Com<IShellItem> item;if(SUCCEEDED(dialog->GetResult(item.out()))) {
                    PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){app.openPath(path);CoTaskMemFree(path);}
                }
            }
        }
    } else {
        wchar_t path[32768]{};
        std::wstring patterns;
        for(const auto& ext:foto::extensions()) {if(!patterns.empty())patterns+=L';';patterns+=L"*."+std::wstring(ext.begin(),ext.end());}
        std::wstring filter=L"Bild- und Projektdateien";filter+=L'\0';filter+=patterns;filter+=L'\0';filter+=L"Alle Dateien";filter+=L'\0';filter+=L"*.*";filter+=L'\0';filter+=L'\0';
        OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=app.window;ofn.lpstrFilter=filter.c_str();ofn.lpstrFile=path;ofn.nMaxFile=32768;
        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&ofn))app.openPath(path);
    }
}
void command(int id) {
    switch(id) {
    case Open:choose(false);break;case Folder:choose(true);break;
    case Prev:case Next:if(!app.files.empty()){app.index=foto::neighbor(app.index,id==Next?1:-1,app.files.size());app.request();}break;
    case Fit:app.fit=true;app.panX=app.panY=0;break;
    case Actual:app.fit=false;app.zoom=1;app.panX=app.panY=0;break;
    case ZoomIn:case ZoomOut:app.fit=false;app.zoom=foto::clampZoom(app.zoom*(id==ZoomIn?1.25:0.8));break;
    case Rotate:app.rotation=(app.rotation+1)%4;app.panX=app.panY=0;break;
    case Info:app.info=!app.info;break;
    case Full:
        if(!app.fullscreen){app.restoreStyle=GetWindowLongW(app.window,GWL_STYLE);GetWindowRect(app.window,&app.restore);
            MONITORINFO mi{};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(app.window,MONITOR_DEFAULTTONEAREST),&mi);
            SetWindowLongW(app.window,GWL_STYLE,app.restoreStyle&~WS_OVERLAPPEDWINDOW);
            SetWindowPos(app.window,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);
        }else{SetWindowLongW(app.window,GWL_STYLE,app.restoreStyle);auto r=app.restore;SetWindowPos(app.window,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED|SWP_NOZORDER);}
        app.fullscreen=!app.fullscreen;break;
    case Slide:app.slideshow=!app.slideshow;if(app.slideshow)SetTimer(app.window,1,4000,nullptr);else KillTimer(app.window,1);break;
    case Formats: {
        std::wstring msg=L"Foto VLC 0.1.0 · C++ · Windows 10/11 x64\n\n"
            L"Windows WIC: PNG, JPEG/JFIF, BMP/DIB, GIF, TIFF und ICO. Zusätzliche Windows-Codecs können weitere Formate öffnen.\n\n"
            L"Eingebettet: SVG (Formen/Verläufe, kein Text), PSD (8/16-Bit-Komposit), TGA, PGM/PPM, PIC und QOI. Varianten können abweichen.\n\n"
            L"Optional: ImageMagick 7 für viele weitere Raster-, RAW- und Vektorformate. FOTO_VLC_MAGICK muss auf den vollständigen Pfad zu magick.exe zeigen, alternativ Installation im PATH.\n\n"
            L"Alle angefragten Endungen sind im Dateifilter erfasst. Das ist keine Garantie für einen Decoder. Proprietäre Projektdateien, Cloud-Dokumente und viele Spielcontainer sind in v0.1 nicht unterstützt.\n\n"
            L"GIF/APNG und mehrseitige Dateien: erste decodierbare Seite; keine Animation. ImageMagick-Vorschauen bis 6000 × 6000. Kein Ebenen-Editor oder Farbproofing.\n\n"
            L"Strg+O öffnen · Strg+Umschalt+O Ordner\n←/→ blättern · Mausrad zoomen · Ziehen verschieben\nF einpassen · 1 Originalgröße · R drehen · I Info\nF11 Vollbild · Leertaste Diashow · Esc beenden\n\n";
        msg+=magickPath().empty()?L"ImageMagick: nicht gefunden":L"ImageMagick: verfügbar";
        MessageBoxW(app.window,msg.c_str(),L"Formate & Hilfe",MB_OK|MB_ICONINFORMATION);break;
    }}
    InvalidateRect(app.window,nullptr,FALSE);
}
LRESULT CALLBACK wndProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
    case WM_CREATE:app.window=window;app.dpi=GetDpiForWindow(window);DragAcceptFiles(window,TRUE);app.begin();return 0;
    case WM_PAINT:paint(window);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_GETMINMAXINFO:{auto m=reinterpret_cast<MINMAXINFO*>(lp);m->ptMinTrackSize={LONG(720*app.dpi/96),LONG(540*app.dpi/96)};return 0;}
    case WM_DPICHANGED:{app.dpi=HIWORD(wp);auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(window,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
    case Done:{
        std::optional<Completion> result;
        {std::lock_guard<std::mutex> lock(app.mutex);result=std::move(app.completed);app.completed.reset();}
        if(result && result->serial==app.serial){app.picture=std::move(result->decoded.image);app.error=std::move(result->decoded.error);app.busy=false;InvalidateRect(window,nullptr,FALSE);}return 0;
    }
    case WM_DROPFILES:{auto drop=reinterpret_cast<HDROP>(wp);UINT length=DragQueryFileW(drop,0,nullptr,0);std::vector<wchar_t> path(length+1);
        if(length && DragQueryFileW(drop,0,path.data(),length+1))app.openPath(path.data());
        DragFinish(drop);return 0;}
    case WM_LBUTTONDOWN:{
        app.keyboardFocus=false;SetFocus(window);
        POINT pos={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};float scale=app.dpi/96.0f;POINT logical={LONG(pos.x/scale),LONG(pos.y/scale)};
        for(auto hit:app.hits)if(PtInRect(&hit.box,logical)){command(hit.command);return 0;}
        RECT r;GetClientRect(window,&r);auto c=canvas(r.right/scale,r.bottom/scale);
        if(app.picture && c.Contains(float(logical.x),float(logical.y))){app.dragging=true;app.drag=pos;SetCapture(window);}return 0;
    }
    case WM_MOUSEMOVE:if(app.dragging){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};double scale=app.dpi/96.0;app.panX+=(p.x-app.drag.x)/scale;app.panY+=(p.y-app.drag.y)/scale;app.drag=p;InvalidateRect(window,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP:app.dragging=false;ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:app.dragging=false;return 0;
    case WM_MOUSEWHEEL:if(app.picture)command(GET_WHEEL_DELTA_WPARAM(wp)>0?ZoomIn:ZoomOut);return 0;
    case WM_KEYDOWN:
        if(wp==VK_TAB && !app.hits.empty()) {
            app.keyboardFocus=true;size_t current=0;
            for(size_t i=0;i<app.hits.size();++i)if(app.hits[i].command==app.focusCommand)current=i;
            app.focusCommand=app.hits[foto::neighbor(current,GetKeyState(VK_SHIFT)<0?-1:1,app.hits.size())].command;
            InvalidateRect(window,nullptr,FALSE);
        }else if(wp==VK_RETURN && app.keyboardFocus)command(app.focusCommand);
        else if(wp=='O' && GetKeyState(VK_CONTROL)<0)command(GetKeyState(VK_SHIFT)<0?Folder:Open);
        else if(wp==VK_LEFT)command(Prev);else if(wp==VK_RIGHT)command(Next);
        else if(wp=='F')command(Fit);else if(wp=='1')command(Actual);else if(wp=='R')command(Rotate);else if(wp=='I')command(Info);
        else if(wp==VK_F11)command(Full);else if(wp==VK_SPACE)command(Slide);
        else if(wp==VK_ADD||wp==VK_OEM_PLUS)command(ZoomIn);else if(wp==VK_SUBTRACT||wp==VK_OEM_MINUS)command(ZoomOut);
        else if(wp==VK_ESCAPE){if(app.fullscreen)command(Full);else if(app.slideshow)command(Slide);}
        return 0;
    case WM_TIMER:if(wp==1 && !app.busy && app.files.size()>1)command(Next);return 0;
    case WM_DESTROY:KillTimer(window,1);app.shutdown();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}

// Exercise the real Windows decoder, not just a file extension table.
int selfTest(const fs::path& report) {
    std::ofstream log(report);int passed=0;
    auto check=[&](bool ok,const char* name){log<<(ok?"PASS ":"FAIL ")<<name<<"\n";if(ok)++passed;return ok;};
    TempDirectory temp;if(temp.path.empty())return 2;
    Com<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out()))))return 3;
    bool success=true;
    const std::pair<const wchar_t*,GUID> formats[]={{L"png",GUID_ContainerFormatPng},{L"jpg",GUID_ContainerFormatJpeg},{L"bmp",GUID_ContainerFormatBmp},{L"tiff",GUID_ContainerFormatTiff}};
    for(const auto& format:formats) {
        auto path=temp.path/(L"Bilder ü 日本語."+std::wstring(format.first));
        Com<IWICStream> stream;Com<IWICBitmapEncoder> encoder;Com<IWICBitmapFrameEncode> frame;Com<IPropertyBag2> properties;
        HRESULT hr=factory->CreateStream(stream.out());
        if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
        if(SUCCEEDED(hr))hr=factory->CreateEncoder(format.second,nullptr,encoder.out());
        if(SUCCEEDED(hr))hr=encoder->Initialize(stream,WICBitmapEncoderNoCache);
        if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(frame.out(),properties.out());
        if(SUCCEEDED(hr))hr=frame->Initialize(properties);
        if(SUCCEEDED(hr))hr=frame->SetSize(2,2);
        WICPixelFormatGUID pixels=GUID_WICPixelFormat24bppBGR;
        if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&pixels);
        BYTE data[]={0,0,255, 0,0,255, 0,0,255, 0,0,255};
        if(SUCCEEDED(hr))hr=frame->WritePixels(2,6,sizeof(data),data);
        if(SUCCEEDED(hr))hr=frame->Commit();
        if(SUCCEEDED(hr))hr=encoder->Commit();
        // Close the stream and encoder before opening the result.
        frame.out();encoder.out();stream.out();
        auto decoded=wicLoad(path);
        success &=check(SUCCEEDED(hr)&&decoded.image && decoded.image->width==2 && decoded.image->height==2,"WIC fixture + Unicode path + dimensions");
        success &=check(decoded.image && decoded.image->pixels[2]>240 && decoded.image->pixels[0]<15,"Decoded red pixels");
        if(decoded.image) {
            auto& p=*decoded.image;Bitmap source(p.width,p.height,p.width*4,PixelFormat32bppPARGB,p.pixels.data());
            Bitmap destination(16,16,PixelFormat32bppARGB);Graphics g(&destination);
            g.Clear(Color(255,0,0,0));g.DrawImage(&source,Rect(0,0,16,16));Color pixel;destination.GetPixel(8,8,&pixel);
            success &=check(pixel.GetR()>240 && pixel.GetB()<15,"GDI+ render");
        }
    }
    auto invalid=temp.path/L"broken.png";{std::ofstream f(invalid);f<<"not an image";}
    success &=check(!wicLoad(invalid).image,"Reject corrupt image");
    success &=check(!wicLoad(temp.path/L"missing.png").image,"Reject missing file");
    success &=check(foto::recognized("UPPER.PNG") && !foto::recognized("image.png.exe"),"Extension registry");
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=wndProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=AppClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(RegisterClassExW(&wc)) {
        HWND window=CreateWindowExW(0,AppClass,L"Foto VLC · UI-Test",WS_OVERLAPPEDWINDOW,0,0,1180,800,nullptr,nullptr,wc.hInstance,nullptr);
        success &=check(window!=nullptr,"Create desktop window");
        if(window) {
            ShowWindow(window,SW_SHOW);
            app.openPath(temp.path/L"Bilder ü 日本語.png");
            auto pump=[&]{
                DWORD start=GetTickCount();
                while(app.busy && GetTickCount()-start<5000) {
                    MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
                    Sleep(10);
                }
                UpdateWindow(window);
            };
            pump();
            success &=check(app.picture && !app.busy && app.error.empty(),"Asynchronous desktop image load");
            success &=check(app.files.size()>=5,"Folder image discovery");
            auto selected=app.index;command(Next);pump();command(Prev);pump();
            success &=check(app.index==selected && app.picture,"Next and previous navigation");
            command(Actual);command(ZoomIn);command(Rotate);
            success &=check(!app.fit && app.zoom==1.25 && app.rotation==1,"Zoom and rotation controls");
            command(Fit);success &=check(app.fit && app.panX==0 && app.panY==0,"Fit view");
            command(Full);success &=check(app.fullscreen,"Enter fullscreen");command(Full);
            success &=check(!app.fullscreen,"Exit fullscreen");
            command(Slide);success &=check(app.slideshow,"Slideshow timer start");command(Slide);
            success &=check(!app.slideshow,"Slideshow timer stop");
            UpdateWindow(window);success &=check(!app.hits.empty(),"Paint desktop toolbar");
            DestroyWindow(window);MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){}
        }
        UnregisterClassW(AppClass,wc.hInstance);
    }else success &=check(false,"Register desktop window");
    log<<passed<<" checks passed\n";return success?0:1;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    GdiplusStartupInput startup;ULONG_PTR token=0;
    if(GdiplusStartup(&token,&startup,nullptr)!=Ok){if(SUCCEEDED(co))CoUninitialize();return 1;}
    int argc=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc>1 && std::wstring(args[1])==L"--self-test") {
        int result=selfTest(argc>2?fs::path(args[2]):fs::path(L"self-test.txt"));LocalFree(args);GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return result;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=wndProc;wc.hInstance=instance;wc.lpszClassName=AppClass;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(2));wc.hIconSm=wc.hIcon;
    if(!RegisterClassExW(&wc)){LocalFree(args);GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return 1;}
    HWND window=CreateWindowExW(0,AppClass,L"Foto VLC",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1180,800,nullptr,nullptr,instance,nullptr);
    if(!window){LocalFree(args);GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return 1;}
    BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    ShowWindow(window,show);UpdateWindow(window);
    if(argc>1)app.openPath(args[1]);
    LocalFree(args);
    MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return int(message.wParam);
}
