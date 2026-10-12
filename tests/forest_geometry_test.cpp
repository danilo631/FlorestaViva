#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
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
int main(){
    try{
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
            size_t triangles=(tree.woodHigh.i.size()+tree.foliageHigh.i.size())/3;
            total+=triangles;
            std::cout<<"species="<<i<<" wood="<<tree.woodHigh.i.size()/3
                     <<" foliage="<<tree.foliageHigh.i.size()/3
                     <<" far="<<tree.foliageFar.i.size()/3<<"\n";
        }
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
