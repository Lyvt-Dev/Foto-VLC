#include "motion.hpp"
#include <iostream>
int main(){int failed=0,passed=0;auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';ok?++passed:++failed;};
    paf::ScrollMotion scroll;scroll.add(90,1000);scroll.advance(1.0/60);check(scroll.position>0&&scroll.position<30,"Wheel movement starts gently instead of jumping");
    bool monotonic=true;double previous=scroll.position;for(int i=0;i<120;++i){scroll.advance(1.0/60);monotonic&=scroll.position>=previous&&scroll.position<=90;previous=scroll.position;}check(monotonic&&scroll.position==90&&scroll.velocity==0,"Scroll settles without overshoot or endless animation");
    paf::ScrollMotion a,b;a.add(400,1000);b.add(400,1000);for(int i=0;i<10;++i)a.advance(1.0/30);for(int i=0;i<40;++i)b.advance(1.0/120);check(std::abs(a.position-b.position)<0.001,"Same movement at 30 and 120 frames per second");
    a.reset();a.add(11.25,1000);a.advance(0.1);check(a.target==11.25&&a.position>0&&a.position<11.25,"Small touchpad deltas remain fractional pixels");
    a.reset();a.add(90,1000);a.advance(0.04);double speed=a.velocity;a.add(90,1000);check(a.velocity==speed&&a.target==180,"Repeated wheel input preserves motion momentum");
    a.add(-10000,1000);for(int i=0;i<120;++i){a.advance(1.0/60);a.constrain(1000);}check(a.position==0&&a.velocity==0,"Direction reversal stops at top boundary");
    a.reset(800);a.add(500,1000);a.advance(0.1);a.constrain(100);check(a.position==100&&a.target==100&&a.velocity==0,"Resize or filtering clamps movement to new bounds");
    a.reset(42);check(a.target==42&&a.velocity==0&&!a.advance(0.016),"Reduced motion and dragging can snap without residual movement");
    check(!a.advance(0)&&!a.advance(-1),"Invalid frame intervals do not corrupt position");
    std::cout<<passed<<" checks passed\n";return failed?1:0;}
