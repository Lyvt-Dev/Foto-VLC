#include "decoders.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PSD
#define STBI_ONLY_TGA
#define STBI_ONLY_PNM
#define STBI_ONLY_PIC
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 16384
#include "../third_party/stb/stb_image.h"
#define NANOSVG_IMPLEMENTATION
#include "../third_party/nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../third_party/nanosvg/nanosvgrast.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
namespace foto {
namespace {
Raster fail(const char* text){Raster r;r.error=text;return r;}
Raster qoi(const std::vector<std::uint8_t>& data) {
    if(data.size()<22 || std::memcmp(data.data(),"qoif",4)!=0)return fail("Invalid QOI header");
    auto u32=[&](size_t i){return (unsigned(data[i])<<24)|(unsigned(data[i+1])<<16)|(unsigned(data[i+2])<<8)|data[i+3];};
    unsigned w=u32(4),h=u32(8);
    if(!w||!h||w>16384||h>16384||std::uint64_t(w)*h>32000000 || (data[12]!=3&&data[12]!=4)||data[13]>1)
        return fail("QOI dimensions or header exceed supported limits");
    const std::uint8_t end[]={0,0,0,0,0,0,0,1};
    if(std::memcmp(data.data()+data.size()-8,end,8))return fail("Missing QOI end marker");
    Raster r;r.width=w;r.height=h;r.engine="QOI";r.rgba.resize(size_t(w)*h*4);
    using Pixel=std::array<std::uint8_t,4>;std::array<Pixel,64> cache{};
    Pixel pixel={0,0,0,255};size_t pos=14,limit=data.size()-8,total=size_t(w)*h;
    unsigned run=0;
    for(size_t i=0;i<total;++i) {
        if(run) --run;
        else {
            if(pos>=limit)return fail("Truncated QOI pixel data");
            auto op=data[pos++];
            if(op==0xfe){if(pos+3>limit)return fail("Truncated QOI RGB");for(int c=0;c<3;++c)pixel[c]=data[pos++];}
            else if(op==0xff){if(pos+4>limit)return fail("Truncated QOI RGBA");for(int c=0;c<4;++c)pixel[c]=data[pos++];}
            else switch(op&0xc0) {
            case 0x00:pixel=cache[op&0x3f];break;
            case 0x40:pixel[0]=std::uint8_t(pixel[0]+((op>>4)&3)-2);pixel[1]=std::uint8_t(pixel[1]+((op>>2)&3)-2);pixel[2]=std::uint8_t(pixel[2]+(op&3)-2);break;
            case 0x80:{if(pos>=limit)return fail("Truncated QOI luma");int g=(op&63)-32;auto b=data[pos++];pixel[0]=std::uint8_t(pixel[0]+g+(b>>4)-8);pixel[1]=std::uint8_t(pixel[1]+g);pixel[2]=std::uint8_t(pixel[2]+g+(b&15)-8);break;}
            case 0xc0:run=op&63;if(i+run>=total)return fail("Invalid QOI run");break;
            }
            cache[(pixel[0]*3+pixel[1]*5+pixel[2]*7+pixel[3]*11)%64]=pixel;
        }
        std::copy(pixel.begin(),pixel.end(),r.rgba.begin()+i*4);
    }
    if(pos!=limit)return fail("Extra QOI pixel data");
    return r;
}
}
Raster embedded(const std::vector<std::uint8_t>& data,const std::string& ext) {
    if(data.empty() || data.size()>128u*1024*1024)return fail("Empty file or embedded decoder input limit exceeded");
    if(ext=="qoi" || (data.size()>=4 && !std::memcmp(data.data(),"qoif",4)))return qoi(data);
    if(ext=="svg") {
        if(data.size()>8u*1024*1024)return fail("SVG exceeds 8 MiB limit");
        std::vector<char> xml(data.begin(),data.end());xml.push_back(0);
        std::unique_ptr<NSVGimage,decltype(&nsvgDelete)> image(nsvgParse(xml.data(),"px",96),nsvgDelete);
        if(!image || !image->shapes || !std::isfinite(image->width)||!std::isfinite(image->height)||image->width<=0||image->height<=0)
            return fail("SVG has no supported vector shapes");
        float scale=std::min({1.0f,6000.0f/image->width,6000.0f/image->height,
            float(std::sqrt(32000000.0/(double(image->width)*image->height)))});
        Raster r;r.width=std::max(1u,unsigned(std::floor(image->width*scale)));r.height=std::max(1u,unsigned(std::floor(image->height*scale)));r.engine="NanoSVG";
        r.rgba.resize(size_t(r.width)*r.height*4);
        std::unique_ptr<NSVGrasterizer,decltype(&nsvgDeleteRasterizer)> raster(nsvgCreateRasterizer(),nsvgDeleteRasterizer);
        if(!raster)return fail("SVG rasterizer allocation failed");
        nsvgRasterize(raster.get(),image.get(),0,0,scale,r.rgba.data(),r.width,r.height,r.width*4);return r;
    }
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(data.data(),int(data.size()),&width,&height,&channels))return fail("No embedded decoder for this file");
    if(width<=0||height<=0||std::uint64_t(width)*height>32000000)return fail("Raster exceeds 32 megapixel limit");
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(stbi_load_from_memory(data.data(),int(data.size()),&width,&height,&channels,4),stbi_image_free);
    if(!pixels)return fail("Invalid or unsupported raster variant");
    Raster r;r.width=unsigned(width);r.height=unsigned(height);r.engine="stb_image";
    r.rgba.assign(pixels.get(),pixels.get()+size_t(width)*height*4);return r;
}
}
