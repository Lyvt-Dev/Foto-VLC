#pragma once
#include <array>
#include <string>
#include <optional>
#include <regex>
#include <cstdint>
namespace paf {
inline std::optional<std::array<unsigned,4>> version(const std::string& text){
    std::array<unsigned,4> result{};size_t cursor=0;
    for(size_t component=0;component<3;++component){
        size_t start=cursor;unsigned value=0;
        while(cursor<text.size()&&text[cursor]>='0'&&text[cursor]<='9'){
            if(cursor-start>=5)return {};value=value*10+unsigned(text[cursor++]-'0');if(value>65535)return {};
        }
        if(cursor==start||(cursor-start>1&&text[start]=='0'))return {};
        result[component]=value;
        if(component<2){if(cursor>=text.size()||text[cursor++]!='.')return {};}
    }
    if(text.substr(cursor)=="-fix"){result[3]=1;cursor=text.size();}
    if(cursor!=text.size())return {};return result;
}
struct Manifest {std::string version,sha256;uint64_t size=0;};
inline std::optional<Manifest> manifest(const std::string& text){
    if(text.size()>8192)return {};
    static const std::regex schema(R"JSON(^\s*\{\s*"version"\s*:\s*"([0-9.]{5,17}(?:-fix)?)"\s*,\s*"sha256"\s*:\s*"([0-9a-f]{64})"\s*,\s*"size"\s*:\s*([0-9]{1,9})\s*\}\s*$)JSON");
    std::smatch match;if(!std::regex_match(text,match,schema)||!version(match[1]))return {};
    Manifest value{match[1],match[2],std::stoull(match[3])};
    if(value.size<1024||value.size>128ull*1024*1024)return {};return value;
}
inline bool newer(const std::string& candidate,const std::string& current){auto a=version(candidate),b=version(current);return a&&b&&*a>*b;}
}
