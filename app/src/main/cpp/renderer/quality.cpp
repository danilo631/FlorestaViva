#include "renderer/quality.hpp"
#include <algorithm>
namespace {
constexpr ForestQuality levels[5]={
    // BAIXO: 60fps target. Modest vegetation, no heavy post or CSM.
    {"BAIXO",32,88,42,176,20,22,12,19,47,40,75,false,false,false,false,0,0,60},
    // MEDIO: 30fps target Mali-G57 / Adreno 610.
    {"MEDIO",44,104,65,205,26,30,17,26,47,40,115,true,true,false,false,1,1,30},
    // ALTO: wider grass, shadow refinement, normal PBR photos.
    {"ALTO",52,118,75,242,30,35,22,32,47,40,145,true,true,false,false,1,1,45},
    // ULTRA: dense foliage, 3x3 shadow quality and atmospheric shafts.
    {"ULTRA",74,155,107,250,45,50,30,49,73,60,145,true,true,true,false,2,2,30},
    // LEGACY is old-style Lambertian lighting, *not* original source textures.
    {"LEGACY",32,88,42,176,20,22,12,19,47,40,75,false,false,false,true,0,0,30}
};
}
const ForestQuality& forestQuality(int preset){
    return levels[std::clamp(preset,0,4)];
}
