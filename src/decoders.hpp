#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace foto {
struct Raster {
    unsigned width=0,height=0,frames=1;
    std::vector<std::uint8_t> rgba;
    std::string engine,error;
    explicit operator bool()const{return !rgba.empty();}
};
Raster embedded(const std::vector<std::uint8_t>& data,const std::string& extension);
}
