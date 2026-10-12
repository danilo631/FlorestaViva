#include <cassert>
#include <cmath>
#include <iostream>
#include "../app/src/main/cpp/forest_survival.hpp"
int main(){
    ForestSurvival s;
    assert(s.canSprint());
    for(int i=0;i<100;i++)s.tick(.05f,true,false);
    assert(s.stamina<100.f && s.hydration<100.f);
    assert(s.harvest(0));
    assert(!s.harvest(1));
    s.tick(.85f,false,true);
    assert(s.harvest(0));
    s.tick(.9f,false,true);assert(s.harvest(1));
    s.tick(.9f,false,true);assert(s.harvest(1));
    s.tick(.9f,false,true);assert(s.harvest(2));
    assert(s.firstSurvivalGoal());
    for(int i=0;i<500;i++)s.tick(.05f,true,false);
    assert(s.stamina==0.f&&!s.canSprint());
    for(int i=0;i<500;i++)s.tick(.05f,false,true);
    assert(s.stamina>8.f&&s.hydration<=100.f);
    std::cout<<"PASS survival sprint/regen/hydration/harvest/inventory goal\n";
}
