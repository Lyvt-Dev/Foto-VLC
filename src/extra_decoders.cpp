#include "decoders.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <regex>
#include <sstream>
namespace foto { namespace {
using Bytes=std::vector<std::uint8_t>;
Raster bad(const char* message){Raster r;r.error=message;return r;}
bool dimensions(unsigned w,unsigned h){return w&&h&&w<=16384&&h<=16384&&std::uint64_t(w)*h<=32000000;}
Raster raster(unsigned w,unsigned h,const char* engine){Raster r;r.width=w;r.height=h;r.engine=engine;r.rgba.assign(size_t(w)*h*4,255);return r;}
unsigned le16(const Bytes& d,size_t p){return unsigned(d[p])|(unsigned(d[p+1])<<8);}
unsigned le32(const Bytes& d,size_t p){return le16(d,p)|(le16(d,p+2)<<16);}
unsigned be16(const Bytes& d,size_t p){return (unsigned(d[p])<<8)|d[p+1];}
unsigned be32(const Bytes& d,size_t p){return (be16(d,p)<<16)|be16(d,p+2);}
Raster pcx(const Bytes& d){
 if(d.size()<128||d[0]!=10||d[2]>1||d[3]!=8)return bad("PCX supports 8-bit indexed and RGB variants");
 unsigned xmin=le16(d,4),ymin=le16(d,6),xmax=le16(d,8),ymax=le16(d,10);if(xmax<xmin||ymax<ymin)return bad("Invalid PCX bounds");
 unsigned w=xmax-xmin+1,h=ymax-ymin+1,planes=d[65],stride=le16(d,66);
 if(!dimensions(w,h)||(planes!=1&&planes!=3)||stride<w||stride>32768)return bad("Invalid PCX dimensions or planes");
 size_t limit=d.size(),palette=0;if(planes==1){if(d.size()<897||d[d.size()-769]!=12)return bad("Missing PCX palette");palette=d.size()-768;limit=d.size()-769;}
 auto r=raster(w,h,"PAF PCX");size_t pos=128;Bytes row(size_t(stride)*planes);
 for(unsigned y=0;y<h;++y){size_t n=0;while(n<row.size()){if(pos>=limit)return bad("Truncated PCX row");unsigned value=d[pos++],count=1;
  if(d[2]&&(value&192)==192){count=value&63;if(!count||pos>=limit)return bad("Invalid PCX run");value=d[pos++];}
  if(count>row.size()-n)return bad("PCX run crosses row boundary");std::fill_n(row.begin()+n,count,std::uint8_t(value));n+=count;}
  for(unsigned x=0;x<w;++x){auto out=(size_t(y)*w+x)*4;for(unsigned c=0;c<3;++c)r.rgba[out+c]=planes==1?d[palette+size_t(row[x])*3+c]:row[size_t(c)*stride+x];}
 }return r;
}
struct Tokens{
 const Bytes& d;size_t p=0;
 std::string next(){while(p<d.size()){if(std::isspace(d[p])){++p;continue;}if(d[p]=='#'){while(p<d.size()&&d[p]!='\n')++p;continue;}break;}size_t start=p;while(p<d.size()&&!std::isspace(d[p])&&d[p]!='#')++p;return std::string(d.begin()+start,d.begin()+p);}
 bool number(unsigned& value){auto t=next();if(t.empty()||t.size()>10)return false;std::uint64_t n=0;for(unsigned char c:t){if(!std::isdigit(c))return false;n=n*10+c-'0';if(n>std::numeric_limits<unsigned>::max())return false;}value=unsigned(n);return true;}
 bool separator(){if(p>=d.size()||!std::isspace(d[p]))return false;auto c=d[p++];if(c=='\r'&&p<d.size()&&d[p]=='\n')++p;return true;}
};
Raster netpbm(const Bytes& d){
 Tokens t{d};auto magic=t.next();unsigned w=0,h=0,depth=0,max=1;
 if(magic=="P7"){max=0;
  bool ended=false;std::string tuple;for(int i=0;i<64;++i){auto key=t.next();if(key=="ENDHDR"){ended=true;break;}if(key=="TUPLTYPE")tuple=t.next();else if(key=="WIDTH"){if(!t.number(w))return bad("Invalid PAM width");}else if(key=="HEIGHT"){if(!t.number(h))return bad("Invalid PAM height");}else if(key=="DEPTH"){if(!t.number(depth))return bad("Invalid PAM depth");}else if(key=="MAXVAL"){if(!t.number(max))return bad("Invalid PAM maximum");}else return bad("Unsupported PAM header");}
  if(!ended||depth<1||depth>4||(!tuple.empty()&&tuple!="RGB"&&tuple!="RGB_ALPHA"&&tuple!="GRAYSCALE"&&tuple!="GRAYSCALE_ALPHA"&&tuple!="BLACKANDWHITE"&&tuple!="BLACKANDWHITE_ALPHA"))return bad("Unsupported PAM tuple");
 }else{if(magic!="P1"&&magic!="P2"&&magic!="P3"&&magic!="P4"&&magic!="P5"&&magic!="P6")return bad("Unsupported Netpbm magic");if(!t.number(w)||!t.number(h))return bad("Invalid Netpbm dimensions");depth=(magic=="P3"||magic=="P6")?3:1;if(magic!="P1"&&magic!="P4"&&!t.number(max))return bad("Invalid Netpbm maximum");}
 if(!dimensions(w,h)||!max||max>65535)return bad("Netpbm dimensions or sample range exceed limits");auto r=raster(w,h,"PAF Netpbm");
 bool ascii=magic=="P1"||magic=="P2"||magic=="P3";if(!ascii&&!t.separator())return bad("Missing Netpbm raster separator");
 if(magic=="P4"){size_t stride=(size_t(w)+7)/8;if(stride*h>d.size()-t.p)return bad("Truncated PBM raster");for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){auto color=(d[t.p+y*stride+x/8]&(128>>(x%8)))?0:255;for(unsigned c=0;c<3;++c)r.rgba[(size_t(y)*w+x)*4+c]=std::uint8_t(color);}return r;}
 size_t samples=size_t(w)*h*depth;unsigned bytes=max>255?2:1;if(!ascii&&samples*bytes>d.size()-t.p)return bad("Truncated Netpbm raster");
 for(size_t pixel=0;pixel<size_t(w)*h;++pixel){std::array<unsigned,4> values{0,0,0,255};for(unsigned c=0;c<depth;++c){unsigned v=0;if(ascii){if(!t.number(v))return bad("Truncated ASCII Netpbm");}else{v=bytes==1?d[t.p]:be16(d,t.p);t.p+=bytes;}if(v>max)return bad("Netpbm sample exceeds maximum");values[c]=(v*255+max/2)/max;}
  if(magic=="P1")values[0]=values[0]?0:255;auto out=pixel*4;
  for(unsigned c=0;c<3;++c)r.rgba[out+c]=std::uint8_t(depth<3?values[0]:values[c]);if(depth==2||depth==4)r.rgba[out+3]=std::uint8_t(values[depth-1]);}
 return r;
}
Raster sgi(const Bytes& d){
 if(d.size()<512||be16(d,0)!=474||d[2]>1||d[3]!=1)return bad("SGI supports 8-bit raw and RLE variants");unsigned w=be16(d,6),h=be16(d,8),channels=be16(d,10);
 if(!dimensions(w,h)||channels<1||channels>4||be32(d,104)!=0)return bad("Invalid SGI dimensions or colormap");size_t rows=size_t(h)*channels;
 if((!d[2]&&size_t(w)*rows>d.size()-512)||(d[2]&&rows*8>d.size()-512))return bad("Truncated SGI tables or raster");auto r=raster(w,h,"PAF SGI");Bytes row(w);
 for(unsigned c=0;c<channels;++c)for(unsigned y=0;y<h;++y){size_t table=size_t(c)*h+y;
  if(!d[2])std::copy_n(d.begin()+512+table*w,w,row.begin());else{size_t pos=be32(d,512+table*4),length=be32(d,512+rows*4+table*4);if(pos<512+rows*8||pos>d.size()||length>d.size()-pos)return bad("Invalid SGI RLE offset");size_t end=pos+length,n=0;bool stopped=false;
   while(pos<end){unsigned op=d[pos++],count=op&127;if(!count){stopped=true;break;}if(count>w-n)return bad("SGI run exceeds row");if(op&128){if(count>end-pos)return bad("Truncated SGI literal");std::copy_n(d.begin()+pos,count,row.begin()+n);pos+=count;}else{if(pos>=end)return bad("Truncated SGI run");std::fill_n(row.begin()+n,count,d[pos++]);}n+=count;}if(!stopped||n!=w)return bad("Incomplete SGI row");}
  for(unsigned x=0;x<w;++x){size_t out=(size_t(h-1-y)*w+x)*4;if(channels<3&&c==0)for(unsigned k=0;k<3;++k)r.rgba[out+k]=row[x];else r.rgba[out+((channels==2&&c==1)?3:c)]=row[x];}
 }return r;
}
std::array<unsigned,4> rgb565(unsigned n){return {((n>>11)&31)*255/31,((n>>5)&63)*255/63,(n&31)*255/31,255};}
Raster dds(const Bytes& d){
 if(d.size()<128||std::memcmp(d.data(),"DDS ",4)||le32(d,4)!=124||le32(d,76)!=32)return bad("Invalid DDS header");unsigned h=le32(d,12),w=le32(d,16),flags=le32(d,80),fourcc=le32(d,84),bits=le32(d,88);
 if(!dimensions(w,h))return bad("DDS dimensions exceed limits");auto r=raster(w,h,"PAF DDS");
 if(flags&4){unsigned type=fourcc==0x31545844?1:fourcc==0x33545844?3:fourcc==0x35545844?5:0;if(!type)return bad("DDS supports DXT1/DXT3/DXT5; DX10/BC7 require an optional decoder");size_t stride=type==1?8:16,columns=(w+3)/4,rows=(h+3)/4;if(columns*rows*stride>d.size()-128)return bad("Truncated DDS blocks");
  for(size_t by=0;by<rows;++by)for(size_t bx=0;bx<columns;++bx){size_t p=128+(by*columns+bx)*stride,color=p+(type==1?0:8);unsigned a=le16(d,color),b=le16(d,color+2);std::array<std::array<unsigned,4>,4> colors{rgb565(a),rgb565(b)};
   for(unsigned c=0;c<3;++c){colors[2][c]=type!=1||a>b?(2*colors[0][c]+colors[1][c])/3:(colors[0][c]+colors[1][c])/2;colors[3][c]=type!=1||a>b?(colors[0][c]+2*colors[1][c])/3:0;}colors[2][3]=255;colors[3][3]=type==1&&a<=b?0:255;
   std::array<unsigned,8> alpha{};std::uint64_t alphaCodes=0;if(type==5){alpha[0]=d[p];alpha[1]=d[p+1];for(unsigned k=2;k<8;++k)alpha[k]=alpha[0]>alpha[1]?((8-k)*alpha[0]+(k-1)*alpha[1])/7:k<6?((6-k)*alpha[0]+(k-1)*alpha[1])/5:k==6?0:255;for(unsigned k=0;k<6;++k)alphaCodes|=std::uint64_t(d[p+2+k])<<(k*8);}
   unsigned codes=le32(d,color+4);for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){unsigned i=y*4+x;if(bx*4+x>=w||by*4+y>=h)continue;auto value=colors[(codes>>(2*i))&3];if(type==3)value[3]=((d[p+i/2]>>((i%2)*4))&15)*17;if(type==5)value[3]=alpha[(alphaCodes>>(i*3))&7];size_t out=((by*4+y)*w+bx*4+x)*4;for(unsigned c=0;c<4;++c)r.rgba[out+c]=std::uint8_t(value[c]);}
  }return r;
 }
 if(!(flags&64)||(bits!=24&&bits!=32))return bad("DDS supports RGB24/RGBA32 and DXT1/3/5");unsigned bytes=bits/8;size_t stride=size_t(w)*bytes;if(le32(d,8)&8)stride=le32(d,20);if(stride<size_t(w)*bytes||stride> d.size()||stride*h>d.size()-128)return bad("Truncated DDS rows");
 std::array<unsigned,4> masks{le32(d,92),le32(d,96),le32(d,100),le32(d,104)};for(unsigned c=0;c<3;++c)if(!masks[c])return bad("Missing DDS color mask");
 for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){size_t p=128+y*stride+x*bytes;unsigned value=unsigned(d[p])|(unsigned(d[p+1])<<8)|(unsigned(d[p+2])<<16);if(bytes==4)value|=unsigned(d[p+3])<<24;for(unsigned c=0;c<4;++c){unsigned mask=masks[c],shift=0;if(!mask){r.rgba[(size_t(y)*w+x)*4+c]=255;continue;}while(!(mask&1)){mask>>=1;++shift;}if((mask&(mask+1))||mask>65535)return bad("Unsupported DDS channel mask");r.rgba[(size_t(y)*w+x)*4+c]=std::uint8_t((((value>>shift)&mask)*255+mask/2)/mask);}}
 return r;
}
Raster xbm(const Bytes& d){
 if(d.size()>8u*1024*1024)return bad("XBM text exceeds limit");std::string text(d.begin(),d.end());std::smatch match;unsigned w=0,h=0;
 try{if(!std::regex_search(text,match,std::regex("#define[ \\t]+[A-Za-z0-9_]+_width[ \\t]+([0-9]+)")))return bad("Missing XBM width");if(std::stoul(match[1])>16384)return bad("XBM width exceeds limit");w=unsigned(std::stoul(match[1]));if(!std::regex_search(text,match,std::regex("#define[ \\t]+[A-Za-z0-9_]+_height[ \\t]+([0-9]+)")))return bad("Missing XBM height");if(std::stoul(match[1])>16384)return bad("XBM height exceeds limit");h=unsigned(std::stoul(match[1]));}catch(...){return bad("Invalid XBM dimensions");}
 if(!dimensions(w,h)||text.find("short")!=std::string::npos)return bad("XBM supports 8-bit arrays");size_t start=text.find('{'),end=text.find('}',start);if(start==std::string::npos||end==std::string::npos)return bad("Missing XBM bytes");auto body=text.substr(start+1,end-start-1);std::regex hex("0[xX]([0-9a-fA-F]{1,2})(?![0-9a-fA-F])");Bytes values;for(auto i=std::sregex_iterator(body.begin(),body.end(),hex);i!=std::sregex_iterator();++i)values.push_back(std::uint8_t(std::stoul((*i)[1],nullptr,16)));
 size_t stride=(size_t(w)+7)/8;if(values.size()!=stride*h)return bad("Incomplete XBM raster");auto r=raster(w,h,"PAF XBM");for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){auto color=values[y*stride+x/8]&(1<<(x%8))?0:255;for(unsigned c=0;c<3;++c)r.rgba[(size_t(y)*w+x)*4+c]=std::uint8_t(color);}return r;
}
} // namespace
Raster extraEmbedded(const std::vector<std::uint8_t>& data,const std::string& ext){
 if(ext=="pcx"||ext=="pcc")return pcx(data);
 if(ext=="pbm"||ext=="pam"||ext=="pgm"||ext=="ppm"||ext=="pnm")return netpbm(data);
 if(ext=="sgi"||ext=="rgb"||ext=="rgba"||ext=="bw")return sgi(data);
 if(ext=="dds")return dds(data);
 if(ext=="xbm")return xbm(data);
 return bad("No additional embedded decoder");
}
}
