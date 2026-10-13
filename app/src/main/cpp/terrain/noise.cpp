#include "terrain/noise.hpp"
#include <cmath>
uint32_t hash32(uint32_t x){
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;
}
float randCell(int x,int z){
    uint32_t a=hash32(uint32_t(x)*73856093u ^ uint32_t(z)*19349663u ^ 913547u);
    return float(a&0xffffffu)/float(0xffffffu);
}
float noise(float x,float z){
    int ix=int(std::floor(x)),iz=int(std::floor(z)); float fx=x-ix,fz=z-iz;
    fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
    float a=randCell(ix,iz),b=randCell(ix+1,iz);
    float c=randCell(ix,iz+1),d=randCell(ix+1,iz+1);
    return (a+(b-a)*fx)*(1-fz)+(c+(d-c)*fx)*fz;
}
