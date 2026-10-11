#include "decoders.hpp"
#include <algorithm>
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
 auto ascii=foto::embedded(bytes("P3\n# comment\n2 1\n15\n15 0 0 0 15 0\n"),"ppm");CHECK(ascii);CHECK(ascii.rgba==p.rgba);
 auto pbm=foto::embedded(bytes("P1\n2 1\n1 0\n"),"pbm");CHECK(pbm);CHECK(pbm.rgba==std::vector<std::uint8_t>({0,0,0,255,255,255,255,255}));
 auto packed=bytes("P4\n2 1\n");packed.push_back(128);auto packedImage=foto::embedded(packed,"pbm");CHECK(packedImage);CHECK(packedImage.rgba==pbm.rgba);packed.pop_back();CHECK(!foto::embedded(packed,"pbm"));
 auto pam=bytes("P7\nWIDTH 1\nHEIGHT 1\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n");pam.insert(pam.end(),{255,0,0,128});auto alphaPam=foto::embedded(pam,"pam");CHECK(alphaPam);CHECK(alphaPam.rgba==std::vector<std::uint8_t>({255,0,0,128}));pam.pop_back();CHECK(!foto::embedded(pam,"pam"));
 CHECK(!foto::embedded(bytes("P3\n1 1\n255\n256 0 0"),"ppm"));
 auto mask=foto::embedded(bytes("#define icon_width 2\n#define icon_height 1\nstatic unsigned char icon_bits[]={0x01};"),"xbm");CHECK(mask);CHECK(mask.rgba==pbm.rgba);
 CHECK(!foto::embedded(bytes("#define icon_width 100000\n#define icon_height 1\nstatic char bits[]={0x01};"),"xbm"));
 std::vector<std::uint8_t> pcx(128,0);pcx[0]=10;pcx[2]=1;pcx[3]=8;pcx[8]=1;pcx[65]=3;pcx[66]=2;pcx.insert(pcx.end(),{193,255,0,0,193,255,0,0});auto colorPcx=foto::embedded(pcx,"pcx");CHECK(colorPcx);CHECK(colorPcx.rgba==p.rgba);pcx[128]=199;CHECK(!foto::embedded(pcx,"pcx"));
 std::vector<std::uint8_t> sgi(512,0);sgi[0]=1;sgi[1]=218;sgi[3]=1;sgi[5]=3;sgi[7]=2;sgi[9]=1;sgi[11]=3;sgi.insert(sgi.end(),{255,0,0,255,0,0});auto colorSgi=foto::embedded(sgi,"sgi");CHECK(colorSgi);CHECK(colorSgi.rgba==p.rgba);sgi.resize(512);CHECK(!foto::embedded(sgi,"sgi"));
 auto set32=[](std::vector<std::uint8_t>& data,size_t pos,unsigned value){for(unsigned c=0;c<4;++c)data[pos+c]=std::uint8_t(value>>(c*8));};
 std::vector<std::uint8_t> dds(136,0);dds[0]='D';dds[1]='D';dds[2]='S';dds[3]=' ';set32(dds,4,124);set32(dds,12,4);set32(dds,16,4);set32(dds,76,32);set32(dds,80,4);set32(dds,84,0x31545844);dds[128]=0;dds[129]=248;auto dxt=foto::embedded(dds,"dds");CHECK(dxt);CHECK(dxt.width==4&&dxt.height==4);CHECK(dxt.rgba[0]==255&&dxt.rgba[1]==0&&dxt.rgba[3]==255);
 dds.resize(135);CHECK(!foto::embedded(dds,"dds"));dds.resize(144);set32(dds,84,0x35545844);dds[128]=128;dds[129]=64;dds[136]=0;dds[137]=248;auto dxtAlpha=foto::embedded(dds,"dds");CHECK(dxtAlpha);CHECK(dxtAlpha.rgba[0]==255&&dxtAlpha.rgba[3]==128);set32(dds,84,0x30315844);CHECK(!foto::embedded(dds,"dds"));
 auto hdr=bytes("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n");hdr.insert(hdr.end(),{128,0,0,129});auto radiance=foto::embedded(hdr,"hdr");CHECK(radiance);CHECK(radiance.rgba[0]==255&&radiance.rgba[1]==0);
 std::vector<std::uint8_t> indexed(128,0);indexed[0]=10;indexed[2]=1;indexed[3]=8;indexed[65]=1;indexed[66]=1;indexed.push_back(1);indexed.push_back(12);indexed.resize(indexed.size()+768);indexed[133]=255;auto indexImage=foto::embedded(indexed,"pcx");CHECK(indexImage);CHECK(indexImage.rgba[0]==255&&indexImage.rgba[1]==0);
 std::vector<std::uint8_t> rleSgi(512,0);rleSgi[0]=1;rleSgi[1]=218;rleSgi[2]=1;rleSgi[3]=1;rleSgi[5]=2;rleSgi[7]=2;rleSgi[9]=1;rleSgi[11]=1;
 auto setBE=[](std::vector<std::uint8_t>& data,size_t pos,unsigned value){for(unsigned c=0;c<4;++c)data[pos+c]=std::uint8_t(value>>((3-c)*8));};rleSgi.resize(520);setBE(rleSgi,512,520);setBE(rleSgi,516,4);rleSgi.insert(rleSgi.end(),{130,0,255,0});auto graySgi=foto::embedded(rleSgi,"sgi");CHECK(graySgi);CHECK(graySgi.rgba[0]==0&&graySgi.rgba[4]==255);rleSgi.back()=2;CHECK(!foto::embedded(rleSgi,"sgi"));
 std::vector<std::uint8_t> rawDds(132,0);std::copy_n(dds.begin(),128,rawDds.begin());set32(rawDds,12,1);set32(rawDds,16,1);set32(rawDds,80,65);set32(rawDds,84,0);set32(rawDds,88,32);set32(rawDds,92,0xff0000);set32(rawDds,96,0xff00);set32(rawDds,100,0xff);set32(rawDds,104,0xff000000);rawDds[130]=255;rawDds[131]=128;auto rawTexture=foto::embedded(rawDds,"dds");CHECK(rawTexture);CHECK(rawTexture.rgba==std::vector<std::uint8_t>({255,0,0,128}));
 std::cout<<checks<<" decoder checks passed\n";
}
