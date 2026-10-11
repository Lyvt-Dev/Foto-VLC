#pragma once
#include <algorithm>
#include <cstddef>
#include <utility>
namespace foto {
struct Box {float x=0,y=0,w=0,h=0;};
struct ViewerLayout {
    Box canvas, image, inspector, filmstrip;
    float width=0,height=0;
    ViewerLayout(float w,float h,bool file,bool details,bool strip):width(w),height(h) {
        float side=file&&details?256.0f:0.0f;
        float bottom=file&&strip?110.0f:0.0f;
        canvas={0,112,std::max(1.0f,w-side),std::max(1.0f,h-140-bottom)};
        image={44,128,std::max(1.0f,canvas.w-88),std::max(1.0f,canvas.h-96)};
        inspector={w-side,112,side,std::max(1.0f,h-140)};
        filmstrip={0,h-28-bottom,canvas.w,bottom};
    }
};
inline std::pair<size_t,size_t> filmRange(size_t selected,size_t total,float width) {
    if(!total)return {0,0};
    size_t visible=std::min(total,size_t(std::max(1.0f,std::min(11.0f,(width-48)/88))));
    size_t first=selected>=visible/2?selected-visible/2:0;
    first=std::min(first,total-visible);
    return {first,visible};
}
}
