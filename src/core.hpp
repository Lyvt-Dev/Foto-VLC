#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace foto {
inline std::vector<std::string> extensions() {
    // Recognition is not a claim that a decoder exists for every format.
    const std::string list =
        "png jpg jpeg jpe jfif pjpeg pjp bmp dib gif webp avif heic heif heics heifs tif tiff ico cur apng jxl jxr wdp hdp svg svgz "
        "raw dng cr2 cr3 crw nef nrw arw srf sr2 raf orf rw2 rw1 pef ptx 3fr fff iiq k25 kdc dcr mos mrw mef erf cap eip bay rwl srw x3f qtk rd sti obm mdc "
        "psd psb psdc ai eps epsf epsi pdf indd idml xcf kra ora ase aseprite afphoto afdesign cdr cdt cmx cpt pdn procreate sketch fig fh fh3 fh4 fh5 fh7 fh8 fh9 fh10 fh11 "
        "ps emf emf+ wmf cgm dxf dwg odg sxd vsd vsdx wpg hpgl plt "
        "pcx pcc tga targa iff ilbm lbm pbm pgm ppm pnm pam pfm xbm xpm xwd ras sun rgb rgba bw sgi pic pict pct mac msp cut dcx pcd fpx pxr ota wbmp art viff vif miff mpc mng jng jp2 j2k j2c jpc jpf jpx jpm mj2 bpg flif qoi "
        "dds ktx ktx2 pvr astc atf vtf vmt wal wad iwi blp bli bti bnr tpl tim tim2 gim gxt gtf gnf tex texture tex0 bntx bflim bclim gtx swf";
    std::vector<std::string> result;
    for (size_t i=0; i<list.size();) {
        auto end=list.find(' ',i);
        if(end==std::string::npos) end=list.size();
        result.push_back(list.substr(i,end-i)); i=end+1;
    }
    std::sort(result.begin(),result.end());
    result.erase(std::unique(result.begin(),result.end()),result.end());
    return result;
}
inline std::string extension(const std::string& path) {
    auto dot=path.find_last_of('.'), slash=path.find_last_of("/\\");
    if(dot==std::string::npos || (slash!=std::string::npos && dot<slash)) return {};
    std::string s=path.substr(dot+1);
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});
    return s;
}
inline bool recognized(const std::string& path) {
    static const auto all=extensions();
    return std::binary_search(all.begin(),all.end(),extension(path));
}
inline bool nativeFormat(const std::string& ext) {
    static const std::vector<std::string> native={"bmp","dib","gif","ico","jfif","jpe","jpeg","jpg","pjpeg","pjp","png","tif","tiff"};
    return std::binary_search(native.begin(),native.end(),ext);
}
// CommandLineToArgvW / MS CRT quoting. Never invoke a command shell.
inline std::wstring quote(const std::wstring& arg) {
    std::wstring out=L"\""; size_t slashes=0;
    for(wchar_t c:arg) {
        if(c==L'\\') { ++slashes; continue; }
        if(c==L'\"') out.append(slashes*2+1,L'\\');
        else out.append(slashes,L'\\');
        slashes=0; out+=c;
    }
    out.append(slashes*2,L'\\'); out+=L'\"'; return out;
}
inline bool orientBGRA(std::vector<std::uint8_t>& pixels,unsigned& w,unsigned& h,int orientation) {
    if(!w||!h||pixels.size()!=std::uint64_t(w)*h*4||orientation<1||orientation>8)return false;
    if(orientation==1)return true;
    unsigned nw=orientation>=5?h:w,nh=orientation>=5?w:h;
    std::vector<std::uint8_t> transformed(pixels.size());
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
        unsigned dx=x,dy=y;
        switch(orientation) {
        case 2:dx=w-1-x;break;
        case 3:dx=w-1-x;dy=h-1-y;break;
        case 4:dy=h-1-y;break;
        case 5:dx=y;dy=x;break;
        case 6:dx=h-1-y;dy=x;break;
        case 7:dx=h-1-y;dy=w-1-x;break;
        case 8:dx=y;dy=w-1-x;break;
        }
        std::copy_n(pixels.begin()+(size_t(y)*w+x)*4,4,transformed.begin()+(size_t(dy)*nw+dx)*4);
    }
    pixels=std::move(transformed);w=nw;h=nh;return true;
}
inline double clampZoom(double z) { return std::max(0.01,std::min(32.0,z)); }
inline size_t neighbor(size_t index,int delta,size_t count) {
    if(!count) return 0;
    return static_cast<size_t>((static_cast<long long>(index)+delta%static_cast<long long>(count)+count)%count);
}
}
