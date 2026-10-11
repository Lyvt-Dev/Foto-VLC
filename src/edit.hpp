#pragma once
#include "core.hpp"
#include <cmath>
#include <stdexcept>
namespace paf {
struct EditOptions {unsigned cropX=0,cropY=0,cropW=0,cropH=0,width=0,height=0;int brightness=0;double contrast=1,saturation=1;bool sepia=false;};
inline void editBGRA(std::vector<std::uint8_t>& pixels,unsigned& w,unsigned& h,const EditOptions& e){
 if(!w||!h||pixels.size()!=size_t(w)*h*4||!std::isfinite(e.contrast)||!std::isfinite(e.saturation)||e.contrast<0||e.contrast>4||e.saturation<0||e.saturation>4||e.brightness<-100||e.brightness>100)throw std::runtime_error("Invalid edit input");
 if(e.cropW||e.cropH){if(!e.cropW||!e.cropH||e.cropX>=w||e.cropY>=h||e.cropW>w-e.cropX||e.cropH>h-e.cropY)throw std::runtime_error("Invalid crop");std::vector<std::uint8_t> out(size_t(e.cropW)*e.cropH*4);for(unsigned y=0;y<e.cropH;++y)std::copy_n(pixels.begin()+((size_t(y+e.cropY)*w+e.cropX)*4),size_t(e.cropW)*4,out.begin()+size_t(y)*e.cropW*4);pixels=std::move(out);w=e.cropW;h=e.cropH;}
 if(e.width||e.height){if(!e.width||!e.height||e.width>16384||e.height>16384||std::uint64_t(e.width)*e.height>32000000)throw std::runtime_error("Resize exceeds limits");std::vector<std::uint8_t> out(size_t(e.width)*e.height*4);
  for(unsigned y=0;y<e.height;++y){double fy=std::clamp((y+.5)*h/e.height-.5,0.0,double(h-1));unsigned y0=unsigned(fy),y1=std::min(y0+1,h-1);double ty=fy-y0;
   for(unsigned x=0;x<e.width;++x){double fx=std::clamp((x+.5)*w/e.width-.5,0.0,double(w-1));unsigned x0=unsigned(fx),x1=std::min(x0+1,w-1);double tx=fx-x0;
    for(unsigned c=0;c<4;++c){double a=pixels[(size_t(y0)*w+x0)*4+c]*(1-tx)+pixels[(size_t(y0)*w+x1)*4+c]*tx,b=pixels[(size_t(y1)*w+x0)*4+c]*(1-tx)+pixels[(size_t(y1)*w+x1)*4+c]*tx;out[(size_t(y)*e.width+x)*4+c]=std::uint8_t(std::clamp(std::round(a*(1-ty)+b*ty),0.0,255.0));}}
  }pixels=std::move(out);w=e.width;h=e.height;
 }
 if(e.brightness||e.contrast!=1||e.saturation!=1||e.sepia){for(size_t i=0;i<pixels.size();i+=4){unsigned alpha=pixels[i+3];if(!alpha){pixels[i]=pixels[i+1]=pixels[i+2]=0;continue;}double b=pixels[i]*255.0/alpha,g=pixels[i+1]*255.0/alpha,r=pixels[i+2]*255.0/alpha;
  auto adjust=[&](double c){return std::clamp((c-127.5)*e.contrast+127.5+e.brightness*2.55,0.0,255.0);};b=adjust(b);g=adjust(g);r=adjust(r);double gray=.2126*r+.7152*g+.0722*b;
  r=gray+(r-gray)*e.saturation;g=gray+(g-gray)*e.saturation;b=gray+(b-gray)*e.saturation;
  if(e.sepia){double nr=.393*r+.769*g+.189*b,ng=.349*r+.686*g+.168*b,nb=.272*r+.534*g+.131*b;r=nr;g=ng;b=nb;}
  pixels[i]=std::uint8_t(std::round(std::clamp(b,0.0,255.0)*alpha/255));pixels[i+1]=std::uint8_t(std::round(std::clamp(g,0.0,255.0)*alpha/255));pixels[i+2]=std::uint8_t(std::round(std::clamp(r,0.0,255.0)*alpha/255));}}
}
}
