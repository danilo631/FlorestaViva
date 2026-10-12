#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <cstdlib>
#include <string>
#include <random>
#include <vector>
constexpr float PI=3.14159265358979323846f;
struct Vec3 {
    float x=0,y=0,z=0;
    Vec3()=default;
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    Vec3 operator+(Vec3 b)const{return {x+b.x,y+b.y,z+b.z};}
    Vec3 operator-(Vec3 b)const{return {x-b.x,y-b.y,z-b.z};}
    Vec3 operator*(float v)const{return {x*v,y*v,z*v};}
};
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 normalize(Vec3 a){float l=std::sqrt(std::max(dot(a,a),1e-12f));return a*(1.f/l);}
struct Vertex {float x,y,z,nx,ny,nz,u,v;};
struct ModelData {
    std::vector<Vertex> v;
    std::vector<uint32_t> i;
    void triangle(uint32_t a,uint32_t b,uint32_t c){i.push_back(a);i.push_back(b);i.push_back(c);}
};
void ellipsoid(ModelData& m,Vec3 center,Vec3 scale,int rings=8,int sectors=12){
    uint32_t base=uint32_t(m.v.size());
    for(int r=0;r<=rings;r++)for(int i=0;i<=sectors;i++){
        float a=PI*float(r)/rings,b=2*PI*float(i)/sectors;
        Vec3 n{std::sin(a)*std::cos(b),std::cos(a),std::sin(a)*std::sin(b)};
        Vec3 p=center+Vec3{n.x*scale.x,n.y*scale.y,n.z*scale.z};
        m.v.push_back({p.x,p.y,p.z,n.x,n.y,n.z,float(i)/sectors,float(r)/rings});
    }
    for(int r=0;r<rings;r++)for(int i=0;i<sectors;i++){
        uint32_t a=base+uint32_t(r*(sectors+1)+i),b=a+uint32_t(sectors+1);
        m.triangle(a,b,a+1);m.triangle(a+1,b,b+1);
    }
}
#include "../app/src/main/cpp/forest_tree_mesh.hpp"
#include "../app/src/main/cpp/forest_undergrowth.hpp"
#include "../app/src/main/cpp/forest_leaf_litter.hpp"
#include "../app/src/main/cpp/forest_fauna.hpp"
#include "../app/src/main/cpp/forest_biome_props.hpp"
#include "../app/src/main/cpp/forest_floor_details.hpp"
void check(const ModelData& m,const char* name){
    if(m.i.empty()||m.v.empty())throw std::runtime_error(std::string(name)+" empty");
    if(m.i.size()%3)throw std::runtime_error(std::string(name)+" nontriangles");
    for(uint32_t i:m.i)if(i>=m.v.size())throw std::runtime_error(std::string(name)+" invalid index");
    for(auto v:m.v){
        for(float q:{v.x,v.y,v.z,v.nx,v.ny,v.nz,v.u,v.v})
            if(!std::isfinite(q))throw std::runtime_error(std::string(name)+" nonfinite");
        if(v.u<-.0001f || v.u>8.f || v.v<-.0001f || v.v>80.f)
            throw std::runtime_error(std::string(name)+" UV out of expected bounds");
    }
}

namespace fs=std::filesystem;
static void exportPart(std::ofstream& out,const ModelData& model,
                       const char* group,const char* mat,size_t& base){
    out<<"g "<<group<<"\nusemtl "<<mat<<"\n";
    for(auto v:model.v)out<<"v "<<v.x<<" "<<v.y<<" "<<v.z<<"\n";
    for(auto v:model.v)out<<"vt "<<v.u<<" "<<v.v<<"\n";
    for(auto v:model.v)out<<"vn "<<v.nx<<" "<<v.ny<<" "<<v.nz<<"\n";
    for(size_t i=0;i<model.i.size();i+=3){
        out<<"f ";
        for(int j=0;j<3;j++){
            auto index=base+model.i[i+j]+1;
            out<<index<<"/"<<index<<"/"<<index<<(j==2?"\n":" ");
        }
    }
    base+=model.v.size();
}
static void exportLod(const fs::path& directory,const ForestTreeMesh& model,
                      int species,int lod){
    bool pine=species>=3;
    const char* family=pine?"conifer":"broadleaf";
    fs::path file=directory/(std::string(family)+"_"+std::to_string(species)+
                              "_LOD"+std::to_string(lod)+".obj");
    std::ofstream o(file);
    o<<std::fixed<<std::setprecision(6);
    o<<"# Floresta Viva: polygon mesh generated at build time.\n";
    o<<"# LOD0: branch skeleton + individual cards; LOD1: lower detail;"
        " LOD2: distant canopy.\n";
    o<<"mtllib forest_materials.mtl\n";
    size_t base=0;
    exportPart(o,lod==0?model.woodHigh:model.woodMid,"woody_skeleton",
               pine?"pine_bark":"broadleaf_bark",base);
    exportPart(o,lod==0?model.foliageHigh:(lod==1?model.foliageMid:model.foliageFar),
               "separate_leaf_geometry",
               lod==2?"distant_canopy":(pine?"conifer_needles":"broadleaf_leaves"),
               base);
    if(!o)throw std::runtime_error("OBJ export failed");
}
static void exportMaterials(const fs::path& dir){
    std::ofstream m(dir/"forest_materials.mtl");
    auto material=[&](const char* name,const char* color,const char* normal,bool alpha){
        m<<"newmtl "<<name<<"\nKd 1 1 1\nKs .04 .04 .04\nNs 20\n";
        m<<"map_Kd "<<color<<"\n";
        if(alpha)m<<"map_d "<<color<<"\n";
        if(normal[0])m<<"bump "<<normal<<"\n";
        m<<"\n";
    };
    material("pine_bark","pine_bark.png","pine_bark_normal.png",false);
    material("broadleaf_bark","bark.png","bark_normal.png",false);
    material("conifer_needles","pine_atlas.png","pine_normal.png",true);
    material("broadleaf_leaves","leaves_atlas.png","leaves_normal.png",true);
    material("distant_canopy","canopy.png","",false);
}

int main(){
    try{
        const char* exportDir=std::getenv("FOREST_EXPORT_DIR");
        const fs::path output=exportDir?fs::path(exportDir):fs::path();
        if(exportDir){fs::create_directories(output);exportMaterials(output);}
        size_t total=0;
        for(int i=0;i<5;i++){
            auto tree=forestBuildTree(i);
            check(tree.woodHigh,"woodHi");
            check(tree.woodMid,"woodMid");
            check(tree.foliageHigh,"foliageHi");
            check(tree.foliageMid,"foliageMid");
            check(tree.foliageFar,"foliageFar");
            if(tree.woodHigh.i.size()<=tree.woodMid.i.size())
                throw std::runtime_error("wood LOD not simplified");
            if(tree.foliageHigh.i.size()<=tree.foliageMid.i.size())
                throw std::runtime_error("leaf LOD not simplified");
            if(exportDir){
                for(int lod=0;lod<3;lod++)exportLod(output,tree,i,lod);
            }
            size_t triangles=(tree.woodHigh.i.size()+tree.foliageHigh.i.size())/3;
            total+=triangles;
            std::cout<<"species="<<i<<" wood="<<tree.woodHigh.i.size()/3
                     <<" foliage="<<tree.foliageHigh.i.size()/3
                     <<" far="<<tree.foliageFar.i.size()/3<<"\n";
        }
        auto details=buildForestUnderstory();
        check(details.twig,"undergrowth twig branches");
        check(details.shrub,"detailed shrub leaves");
        check(details.fern,"pinnate fern leaflets");
        check(details.broadGroundPlant,"groundcover seedlings");
        if(details.shrub.i.size()/3<300 || details.fern.i.size()/3<150)
            throw std::runtime_error("Undergrowth geometries are insufficient");
        std::cout<<"undergrowth mesh triangles: woody="<<details.twig.i.size()/3
                 <<" shrub="<<details.shrub.i.size()/3
                 <<" fern="<<details.fern.i.size()/3
                 <<" seedlings="<<details.broadGroundPlant.i.size()/3<<"\n";
        auto litter=forestLeafLitterPatch();
        check(litter,"fallen leaves");
        if(litter.i.size()/3<75)throw std::runtime_error("Leaf litter poly count too low");
        std::cout<<"scanned atlas litter triangles="<<litter.i.size()/3<<"\n";
        ModelData deer=forestBuildDeer(),boar=forestBuildBoar(),fox=forestBuildFox();
        check(deer,"deer");check(boar,"boar");check(fox,"fox");
        for(const auto& animal:{&deer,&boar,&fox})
            if(animal->i.size()/3<300)throw std::runtime_error("Wildlife mesh too small");
        check(makeRiverReeds(),"river reeds");
        check(makeMeadowFlowers(),"meadow flowers");
        std::cout<<"wildlife triangles deer="<<deer.i.size()/3
                 <<" boar="<<boar.i.size()/3
                 <<" fox="<<fox.i.size()/3<<"\n";
        auto details=forestBuildFloorDetails();
        check(details.dryTwigs,"3D dry ground twigs");
        check(details.fungiStems,"3D mushroom stalks");
        check(details.fungiCaps,"3D mushroom caps");
        if(details.dryTwigs.i.size()/3<100 || details.fungiCaps.i.size()/3<100)
            throw std::runtime_error("insufficient groundcover detail");
        std::cout<<"layer four groundcover triangles: "<<
            (details.dryTwigs.i.size()+details.fungiStems.i.size()+
             details.fungiCaps.i.size())/3<<"\n";
        auto rock=forestRockMesh();check(rock,"rock");
        auto log=forestFallenLog();check(log,"fallenLog");
        if(total<12000)throw std::runtime_error("tree meshes unexpectedly low-poly");
        std::cout<<"PASS: 5 tree species, 3 LODs, triangles="<<total<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
