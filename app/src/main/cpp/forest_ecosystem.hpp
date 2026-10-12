#pragma once
// Deterministic procedural river and biome queries used by height, wildlife,
// terrain/river mesh and spawning. No global mutable state.
static float riverCenter(float z){
    return 38.f+78.f*std::sin(z*.0067f)+24.f*std::sin(z*.020f+.47f);
}
static float riverWidth(float z){
    return 9.2f+2.7f*std::sin(z*.013f+1.1f)+1.1f*std::sin(z*.043f);
}
static float riverLevel(float z){
    return -.52f+.51f*std::sin(z*.0023f+.8f)+.23f*std::sin(z*.0113f);
}
// All river systems use the same slope and normal-space metric.
// Previously the water used raw X width while the banks used projected
// distance, leaving broad dry trenches where the river bends.
static float riverSlope(float z){
    return 78.f*.0067f*std::cos(z*.0067f)+
           24.f*.020f*std::cos(z*.020f+.47f);
}
static float riverScaleX(float z){
    return std::sqrt(1.f+riverSlope(z)*riverSlope(z));
}
static float riverDistance(float x,float z){
    return std::fabs(x-riverCenter(z))/riverScaleX(z);
}
static float waterFraction(float x,float z){
    const float w=riverWidth(z);
    const float t=(riverDistance(x,z)-w)/(7.5f);
    return clampf(t,0.f,1.f);
}
// Channel carving and surface geometry agree at the waterline.
// The bank's first 3 meters form a beach instead of a vertical black cliff.
// Smooth transitions prevent cracks at neighboring chunk borders.
static float forestTerrainHeight(float x,float z,float base){
    const float level=riverLevel(z);
    const float d=riverDistance(x,z),w=riverWidth(z);
    if(d>=w+20.f)return base;
    if(d<=w){
        const float t=clampf(d/w,0.f,1.f);
        const float eased=t*t*(3.f-2.f*t);
        const float underwater=1.42f*(1.f-eased);
        return level-.038f-underwater;
    }
    if(d<=w+3.0f){
        const float t=clampf((d-w)/3.f,0.f,1.f);
        const float eased=t*t*(3.f-2.f*t);
        return level-.038f+eased*.36f;
    }
    const float raised=std::max(base,level+.32f);
    if(d<w+10.f){
        const float t=clampf((d-w-3.f)/7.f,0.f,1.f);
        return (level+.322f)*(1.f-t)+raised*t;
    }
    const float t=clampf((d-w-10.f)/10.f,0.f,1.f);
    const float eased=t*t*(3.f-2.f*t);
    return raised*(1.f-eased)+base*eased;
}
enum class ForestBiome : unsigned char{Broadleaf=0,Conifer=1,Wetland=2,Meadow=3};
static ForestBiome forestBiome(float x,float z){
    const float dist=riverDistance(x,z);
    if(dist<riverWidth(z)+24.f)return ForestBiome::Wetland;
    const float n=noise(x*.0029f+99.f,z*.0029f-45.f);
    if(n>.69f)return ForestBiome::Conifer;
    if(n<.27f)return ForestBiome::Meadow;
    return ForestBiome::Broadleaf;
}
