#include <cassert>
#include <cstring>
#include <iostream>
#include "../app/src/main/cpp/renderer/quality.hpp"
int main(){
    for(int i=0;i<5;i++){
        const auto &p=forestQuality(i);
        assert(p.nearMeters<=p.midMeters && p.midMeters<=p.maxMeters);
        assert(p.microMeters>0 && p.targetFps>=30);
    }
    assert(forestQuality(0).targetFps==60);
    assert(!forestQuality(0).postFX);
    assert(forestQuality(1).targetFps==30);
    assert(forestQuality(2).postFX);
    assert(forestQuality(3).godRays && forestQuality(3).shadows);
    assert(forestQuality(4).legacy && !forestQuality(4).shadows &&
           !forestQuality(4).postFX);
    assert(!std::strcmp(forestQuality(4).name,"LEGACY"));
    assert(forestQuality(99).legacy);
    std::cout<<"PASS five quality policy presets & Legacy rollback lighting\n";
}
