#include "decoders.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); ++checks; } while(false)
std::vector<std::uint8_t> bytes(const std::string& s){return {s.begin(),s.end()};}
int main(){
 int checks=0;
 auto ppm=bytes("P6\n2 1\n255\n");ppm.insert(ppm.end(),{255,0,0,0,255,0});
 auto p=foto::embedded(ppm,"ppm");CHECK(p);CHECK(p.width==2&&p.height==1);CHECK(p.rgba==std::vector<std::uint8_t>({255,0,0,255,0,255,0,255}));
 auto svg=foto::embedded(bytes("<svg xmlns='http://www.w3.org/2000/svg' width='4' height='4'><rect width='4' height='4' fill='#ff0000'/></svg>"),"svg");
 CHECK(svg);CHECK(svg.width==4&&svg.height==4);CHECK(svg.rgba[0]==255&&svg.rgba[3]==255);
 CHECK(!foto::embedded(bytes("broken"),"svg"));
 CHECK(!foto::embedded(bytes("<svg width='0' height='4'></svg>"),"svg"));
 auto huge=foto::embedded(bytes("<svg width='100000' height='100000'><rect width='100000' height='100000'/></svg>"),"svg");
 CHECK(huge);CHECK(huge.width<=6000&&std::uint64_t(huge.width)*huge.height<=32000000);
 std::vector<std::uint8_t> tga(18,0);tga[2]=2;tga[12]=1;tga[14]=1;tga[16]=24;tga.insert(tga.end(),{0,0,255});
 auto t=foto::embedded(tga,"tga");CHECK(t);CHECK(t.rgba[0]==255&&t.rgba[3]==255);
 std::vector<std::uint8_t> qoi={'q','o','i','f',0,0,0,2,0,0,0,1,4,0,0xff,255,0,0,128,0xc0,0,0,0,0,0,0,0,1};
 auto q=foto::embedded(qoi,"qoi");CHECK(q);CHECK(q.rgba==std::vector<std::uint8_t>({255,0,0,128,255,0,0,128}));
 qoi[19]=0xc1;CHECK(!foto::embedded(qoi,"qoi"));
 qoi.pop_back();CHECK(!foto::embedded(qoi,"qoi"));
 CHECK(!foto::embedded(bytes("invalid"),"psd"));
 CHECK(!foto::embedded({},"tga"));
 CHECK(!foto::embedded(bytes("P6\n100000 100000\n255\n"),"ppm"));
 std::cout<<checks<<" decoder checks passed\n";
}
