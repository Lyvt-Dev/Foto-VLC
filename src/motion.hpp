#pragma once
#include <algorithm>
#include <cmath>
namespace paf {
// Exact critically damped spring: independent of timer frequency, with no bounce.
struct ScrollMotion {
    double position=0,target=0,velocity=0;
    void reset(double value=0){position=target=value;velocity=0;}
    void constrain(double maximum){maximum=std::max(0.0,maximum);target=std::clamp(target,0.0,maximum);position=std::clamp(position,0.0,maximum);if((position==0&&velocity<0)||(position==maximum&&velocity>0))velocity=0;}
    void add(double delta,double maximum){constrain(maximum);target=std::clamp(target+delta,0.0,std::max(0.0,maximum));}
    bool advance(double seconds){
        if(!std::isfinite(seconds)||seconds<=0)return position!=target||velocity!=0;
        constexpr double omega=24;double change=position-target,decay=std::exp(-omega*seconds),step=(velocity+omega*change)*seconds;
        position=target+(change+step)*decay;velocity=(velocity-omega*step)*decay;
        if(std::abs(position-target)<0.1&&std::abs(velocity)<1){reset(target);return false;}return true;
    }
};
}
