#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>
constexpr float PI=3.14159265358979323846f;
constexpr int CHUNK=64;
static float clampf(float x,float low,float high){return std::max(low,std::min(high,x));}
static float noise(float x,float z){return .5f+.28f*std::sin(x*.21f+z*.23f);}
struct Vertex{float x,y,z,nx,ny,nz,u,v;};
struct ModelData{
    std::vector<Vertex> v;
    std::vector<uint32_t> i;
    void triangle(uint32_t a,uint32_t b,uint32_t c){
        i.push_back(a);i.push_back(b);i.push_back(c);
    }
};
#include "../app/src/main/cpp/forest_ecosystem.hpp"
#include "../app/src/main/cpp/forest_river.hpp"
int main(){
    for(float z=-900;z<=900;z+=29.7f){
        const float center=riverCenter(z);
        const float w=riverWidth(z);
        const float surface=riverLevel(z);
        float bed=forestTerrainHeight(center,z,1.7f);
        if(bed>=surface-.30f)throw std::runtime_error("River bed above water");
        if(w<4.f||w>18.f)throw std::runtime_error("Invalid river width");
        if(riverDistance(center+200.f,z)<w+30.f)
            throw std::runtime_error("River distance field incorrect");
        float above=forestTerrainHeight(center+w+50.f,z,1.7f);
        if(std::fabs(above-1.7f)>.0001f)
            throw std::runtime_error("River changes distant terrain");
        const auto biome=forestBiome(center,z);
        if(biome!=ForestBiome::Wetland)
            throw std::runtime_error("River center must be wetland");
    }
    // The corrected water edges match the normal-space channel carving,
    // including high-curvature river bends.
    for(float z=-900.f;z<900.f;z+=11.23f){
        const float halfWidth=riverWidth(z)+.85f;
        const float left=riverMeshX(z,0.f),right=riverMeshX(z,1.f);
        if(std::fabs(riverDistance(left,z)-halfWidth)>.003f)
            throw std::runtime_error("Left bank is not aligned with riverbed");
        if(std::fabs(riverDistance(right,z)-halfWidth)>.003f)
            throw std::runtime_error("Right bank is not aligned with riverbed");
        const float water=riverLevel(z)+.035f;
        const float atBank=forestTerrainHeight(right,z,1.7f);
        if(std::fabs(water-atBank)>.12f)
            throw std::runtime_error("Visible waterline gap / cliff at right bank");
    }
    // No duplicate entire river surface per x-chunk; triangle ranges clipped.
    int nonempty=0;
    for(int cx=-4;cx<=4;cx++)for(int cz=-4;cz<=4;cz++){
        const auto m=forestRiverSurface(cx,cz);
        if(m.i.empty())continue;
        nonempty++;
        for(uint32_t index:m.i)if(index>=m.v.size())
            throw std::runtime_error("Water mesh invalid index");
        for(size_t k=0;k<m.i.size();k+=3){
            const auto& a=m.v[m.i[k]];
            const auto& b=m.v[m.i[k+1]];
            const auto& c=m.v[m.i[k+2]];
            const float signedY=(b.z-a.z)*(c.x-a.x)-
                                 (b.x-a.x)*(c.z-a.z);
            if(signedY<=.0000005f)
                throw std::runtime_error("River has inverted or degenerate triangle");
        }
        for(const auto& v:m.v){
            if(v.x<float(cx*CHUNK)-.0001f||
               v.x>float((cx+1)*CHUNK)+.0001f)
                throw std::runtime_error("River mesh outside x-chunk");
            if(!std::isfinite(v.y)||!std::isfinite(v.u))
                throw std::runtime_error("Nonfinite river vertex");
        }
    }
    if(nonempty<2)throw std::runtime_error("River did not cross chunk grid");
    std::printf("PASS river carving, spline, wetland biome, 2km domain, %d water chunks\n",nonempty);
    return 0;
}
