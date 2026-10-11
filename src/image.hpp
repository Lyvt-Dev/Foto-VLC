#pragma once
#include <windows.h>
#include <wincodec.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace fs=std::filesystem;
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
struct TempDirectory {
    fs::path path;
    TempDirectory();
    ~TempDirectory();
};
Decode wicLoad(const fs::path& path,unsigned maxEdge=0);
Decode embeddedLoad(const fs::path& path);
Decode load(const fs::path& path);
std::wstring magickPath();
