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
static float riverDistance(float x,float z){
    // Signed x-distance to smooth river spline, corrected for winding banks.
    float gradient=78.f*.0067f*std::cos(z*.0067f)+
                   24.f*.020f*std::cos(z*.020f+.47f);
    return std::fabs(x-riverCenter(z))/std::sqrt(1.f+gradient*gradient);
}
static float waterFraction(float x,float z){
    const float w=riverWidth(z);
    const float t=(riverDistance(x,z)-w)/(7.5f);
    return clampf(t,0.f,1.f);
}
static float forestTerrainHeight(float x,float z,float base){
    const float river=riverLevel(z);
    const float width=riverWidth(z);
    const float d=riverDistance(x,z);
    if(d>width+10.0f)return base;
    // Ensure water is above its carved bed; shores remain traversable and dry.
    const float shore=std::max(base,river+.65f);
    const float nearBank=clampf((d-width)/10.f,0.f,1.f);
    const float eased=nearBank*nearBank*(3.f-2.f*nearBank);
    const float channel=clampf(d/(width),0.f,1.f);
    const float bottom=river-1.35f+.17f*channel*channel;
    const float bank=bottom+(shore-bottom)*eased;
    return base*eased+bank*(1.f-eased);
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
