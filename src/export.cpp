#include "export.hpp"
#include <gdiplus.h>
#include <algorithm>
#include <fstream>
#include "webp/encode.h"
using namespace Gdiplus;
const wchar_t* exportExtension(ExportFormat format) {
    switch(format){case ExportFormat::WebP:return L".webp";case ExportFormat::JPEG:return L".jpg";case ExportFormat::BMP:return L".bmp";case ExportFormat::TIFF:return L".tiff";default:return L".png";}
}
static std::unique_ptr<Bitmap> transformed(const Picture& picture,int rotation,bool mirrored,bool opaque) {
    if(!picture.width||!picture.height||picture.pixels.size()!=size_t(picture.width)*picture.height*4)return {};
    Bitmap source(picture.width,picture.height,picture.width*4,PixelFormat32bppPARGB,const_cast<BYTE*>(picture.pixels.data()));
    auto output=std::make_unique<Bitmap>(picture.width,picture.height,PixelFormat32bppARGB);
    {Graphics graphics(output.get());graphics.Clear(opaque?Color(255,255,255,255):Color(0,0,0,0));graphics.DrawImage(&source,0,0,picture.width,picture.height);}
    if(mirrored)output->RotateFlip(RotateNoneFlipX);
    if(output->RotateFlip(static_cast<RotateFlipType>((rotation%4+4)%4))!=Ok)return {};
    return output;
}
bool exportPicture(const Picture& picture,const fs::path& original,const fs::path& target,ExportFormat format,int rotation,bool mirrored,unsigned quality) {
    std::error_code error;
    if(fs::equivalent(original,target,error))return false;
    if(CompareStringOrdinal(fs::absolute(original).lexically_normal().c_str(),-1,fs::absolute(target).lexically_normal().c_str(),-1,TRUE)==CSTR_EQUAL)return false;
    auto image=transformed(picture,rotation,mirrored,format==ExportFormat::JPEG||format==ExportFormat::BMP);if(!image)return false;
    UINT count=0,bytes=0;if(GetImageEncodersSize(&count,&bytes)!=Ok||!bytes)return false;
    std::vector<BYTE> buffer(bytes);auto encoders=reinterpret_cast<ImageCodecInfo*>(buffer.data());
    if(GetImageEncoders(count,bytes,encoders)!=Ok)return false;
    const wchar_t* mime=format==ExportFormat::JPEG?L"image/jpeg":format==ExportFormat::BMP?L"image/bmp":format==ExportFormat::TIFF?L"image/tiff":L"image/png";
    CLSID encoder{};bool found=false;for(UINT i=0;i<count;++i)if(wcscmp(encoders[i].MimeType,mime)==0){encoder=encoders[i].Clsid;found=true;break;}if(!found&&format!=ExportFormat::WebP)return false;
    auto temporary=fs::path(target.wstring()+L".photovlc-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;CloseHandle(file);
    ULONG jpegQuality=std::clamp(quality,1u,100u);EncoderParameters parameters{};parameters.Count=1;
    parameters.Parameter[0].Guid=EncoderQuality;parameters.Parameter[0].Type=EncoderParameterValueTypeLong;
    parameters.Parameter[0].NumberOfValues=1;parameters.Parameter[0].Value=&jpegQuality;
    bool saved=false;
    if(format==ExportFormat::WebP){
        BitmapData data{};Rect area(0,0,image->GetWidth(),image->GetHeight());
        if(image->LockBits(&area,ImageLockModeRead,PixelFormat32bppARGB,&data)==Ok){
            uint8_t* encoded=nullptr;size_t size=WebPEncodeLosslessBGRA(static_cast<const uint8_t*>(data.Scan0),area.Width,area.Height,data.Stride,&encoded);
            image->UnlockBits(&data);
            if(size){std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(encoded),std::streamsize(size));output.flush();saved=bool(output);}
            WebPFree(encoded);
        }
    }else saved=image->Save(temporary.c_str(),&encoder,format==ExportFormat::JPEG?&parameters:nullptr)==Ok;
    if(saved)saved=MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!saved)fs::remove(temporary,error);return saved;
}
bool copyPicture(const Picture& picture,int rotation,bool mirrored) {
    auto image=transformed(picture,rotation,mirrored,true);if(!image)return false;
    HBITMAP bitmap=nullptr;if(image->GetHBITMAP(Color(255,255,255,255),&bitmap)!=Ok)return false;
    if(!OpenClipboard(nullptr)){DeleteObject(bitmap);return false;}
    bool copied=EmptyClipboard()&&SetClipboardData(CF_BITMAP,bitmap)!=nullptr;CloseClipboard();if(!copied)DeleteObject(bitmap);return copied;
}
