#include "edit.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);++checks;}while(false)
int main(){int checks=0;std::vector<std::uint8_t> original{0,0,255,255,0,255,0,255,255,0,0,255,0,0,0,0};unsigned w=2,h=2;auto pixels=original;paf::EditOptions e;e.cropX=1;e.cropW=1;e.cropH=2;paf::editBGRA(pixels,w,h,e);CHECK(w==1&&h==2);CHECK(pixels==std::vector<std::uint8_t>({0,255,0,255,0,0,0,0}));
 pixels=original;w=h=2;e={};e.width=e.height=1;paf::editBGRA(pixels,w,h,e);CHECK(w==1&&h==1);CHECK(pixels==std::vector<std::uint8_t>({64,64,64,191}));
 pixels=original;w=h=2;e={};e.saturation=0;paf::editBGRA(pixels,w,h,e);CHECK(pixels[0]==pixels[1]&&pixels[1]==pixels[2]);CHECK(pixels[15]==0&&pixels[12]==0);
 pixels={0,0,128,128};w=h=1;e={};e.brightness=10;paf::editBGRA(pixels,w,h,e);CHECK(pixels[3]==128&&pixels[2]==128&&pixels[0]>0&&pixels[0]<=128);
 bool rejected=false;e={};e.cropW=2;e.cropH=1;try{paf::editBGRA(pixels,w,h,e);}catch(...){rejected=true;}CHECK(rejected);rejected=false;e={};e.width=20000;e.height=1;try{paf::editBGRA(pixels,w,h,e);}catch(...){rejected=true;}CHECK(rejected);
 pixels=original;w=h=2;e={};paf::editBGRA(pixels,w,h,e);CHECK(pixels==original);
 std::cout<<checks<<" edit checks passed\n";}
