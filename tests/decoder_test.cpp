#include "decoders.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
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
 std::vector<std::uint8_t> psd={'8','B','P','S',0,1,0,0,0,0,0,0,0,3,0,0,0,1,0,0,0,1,0,8,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0};
 auto composite=foto::embedded(psd,"psd");CHECK(composite);CHECK(composite.width==1&&composite.height==1);CHECK(composite.rgba==std::vector<std::uint8_t>({255,0,0,255}));
 auto pgm=bytes("P5\n1 1\n255\n");pgm.push_back(100);auto gray=foto::embedded(pgm,"pgm");CHECK(gray);CHECK(gray.rgba==std::vector<std::uint8_t>({100,100,100,255}));
 std::vector<std::uint8_t> rle(18,0);rle[2]=10;rle[12]=2;rle[14]=1;rle[16]=24;rle.insert(rle.end(),{129,0,255,0});
 auto repeated=foto::embedded(rle,"tga");CHECK(repeated);CHECK(repeated.rgba==std::vector<std::uint8_t>({0,255,0,255,0,255,0,255}));
 CHECK(!foto::embedded(bytes("invalid"),"psd"));
 CHECK(!foto::embedded({},"tga"));
 CHECK(!foto::embedded(bytes("P6\n100000 100000\n255\n"),"ppm"));
 for(const auto* name:{"alpha.webp","lossy.webp","animated.webp"}){
   auto path=std::filesystem::path(__FILE__).parent_path()/"fixtures"/name;std::ifstream input(path,std::ios::binary);
   std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)),{});CHECK(!data.empty());
   auto image=foto::embedded(data,"webp");CHECK(image);CHECK(image.width==3&&image.height==2);
   CHECK(image.rgba[0]>240&&image.rgba[1]<10&&image.rgba[2]<10);
   if(std::string(name)!="lossy.webp")CHECK(image.rgba[3]==128);
   data.resize(10);CHECK(!foto::embedded(data,"webp"));
 }
 std::cout<<checks<<" decoder checks passed\n";
}
