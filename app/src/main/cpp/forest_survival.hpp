#pragma once
#include <algorithm>
#include <cmath>
// Survival mechanics are independent of JNI/GL; unit-testable on desktop.
struct ForestSurvival {
    int wood=0,stone=0,herbs=0;
    float stamina=100.f,hydration=100.f;
    float gatherCooldown=0.f;
    void tick(float dt,bool sprinting,bool nearRiver){
        dt=std::max(0.f,std::min(dt,.08f));
        gatherCooldown=std::max(0.f,gatherCooldown-dt);
        stamina=std::clamp(stamina+(sprinting?-14.f:9.f)*dt,0.f,100.f);
        hydration=std::clamp(hydration+((nearRiver?8.f:-.26f)-(sprinting?-.0f+1.2f:0.f))*dt,0.f,100.f);
    }
    bool canSprint()const{return stamina>8.f&&hydration>5.f;}
    bool harvest(int type){
        if(gatherCooldown>0.f)return false;
        if(type==0)wood++;
        else if(type==1)stone++;
        else if(type==2)herbs++;
        else return false;
        gatherCooldown=.8f;
        return true;
    }
    bool firstSurvivalGoal()const{return wood>=2&&stone>=2&&herbs>=1;}
};
