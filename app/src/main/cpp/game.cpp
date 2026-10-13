#include "assets/ktx2.hpp"
#include "renderer/shaders.hpp"
#include "renderer/metrics.hpp"
#include "terrain/noise.hpp"
#include <jni.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <memory>
#include <limits>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FlorestaViva", __VA_ARGS__)
static constexpr float PI = 3.14159265358979323846f;
static constexpr int CHUNK = 64;
static constexpr int GRID = 32;
static constexpr int RADIUS = 3;
static constexpr float WORLD_HALF = 1024.f;
static constexpr int RELIC_COUNT = 6;

struct Vec3 {
    float x=0,y=0,z=0;
    Vec3()=default;
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    Vec3 operator+(Vec3 b) const {return {x+b.x,y+b.y,z+b.z};}
    Vec3 operator-(Vec3 b) const {return {x-b.x,y-b.y,z-b.z};}
    Vec3 operator*(float s) const {return {x*s,y*s,z*s};}
};
static float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static Vec3 normalize(Vec3 a){float l=std::sqrt(std::max(dot(a,a),1e-12f));return a*(1.f/l);}
static float clampf(float a,float lo,float hi){return std::max(lo,std::min(hi,a));}
struct Mat4 { float m[16]{}; };
static Mat4 perspective(float fov,float aspect,float zn,float zf){
    Mat4 o; float f=1.f/std::tan(fov*.5f);
    o.m[0]=f/aspect; o.m[5]=f; o.m[10]=(zf+zn)/(zn-zf);
    o.m[11]=-1.f; o.m[14]=2.f*zf*zn/(zn-zf); return o;
}
static Mat4 orthographic(float left,float right,float bottom,float top,float nearPlane,float farPlane){
    Mat4 o;
    o.m[0]=2.f/(right-left);
    o.m[5]=2.f/(top-bottom);
    o.m[10]=-2.f/(farPlane-nearPlane);
    o.m[12]=-(right+left)/(right-left);
    o.m[13]=-(top+bottom)/(top-bottom);
    o.m[14]=-(farPlane+nearPlane)/(farPlane-nearPlane);
    o.m[15]=1.f;
    return o;
}
static Mat4 lookAt(Vec3 eye,Vec3 at){
    Vec3 f=normalize(at-eye),s=normalize(cross(f,{0,1,0})),u=cross(s,f);
    Mat4 o;
    o.m[0]=s.x;o.m[4]=s.y;o.m[8]=s.z;o.m[12]=-dot(s,eye);
    o.m[1]=u.x;o.m[5]=u.y;o.m[9]=u.z;o.m[13]=-dot(u,eye);
    o.m[2]=-f.x;o.m[6]=-f.y;o.m[10]=-f.z;o.m[14]=dot(f,eye);
    o.m[15]=1.f;return o;
}
static Mat4 multiply(const Mat4&a,const Mat4&b){
    Mat4 r;
    for(int c=0;c<4;c++)for(int row=0;row<4;row++)
        for(int k=0;k<4;k++)r.m[c*4+row]+=a.m[k*4+row]*b.m[c*4+k];
    return r;
}
#include "forest_ecosystem.hpp"
static float height(float x,float z){
    const float base=5.2f*(noise(x*.0048f,z*.0048f)-.5f)+
           1.8f*(noise(x*.020f,z*.020f)-.5f)+
           .55f*(noise(x*.065f,z*.065f)-.5f)+
           .7f*std::sin(x*.012f)*std::cos(z*.014f);
    return forestTerrainHeight(x,z,base);
}
static Vec3 normalAt(float x,float z){
    return normalize({height(x-1,z)-height(x+1,z),2.f,height(x,z-1)-height(x,z+1)});
}
struct Vertex {float x,y,z,nx,ny,nz,u,v;
    // Pivot Painter-inspired packed data: local origin and tip bend weight.
    float pivotX=0,pivotY=0,pivotZ=0,bend=0;
};
struct Instance {float x,y,z,s,rotation,tint,type,pad;};
struct Mesh {
    GLuint vao=0,vbo=0,ebo=0;
    GLsizei count=0;
};
struct ModelData {
    std::vector<Vertex> v;
    std::vector<uint32_t> i;
    void triangle(uint32_t a,uint32_t b,uint32_t c){i.push_back(a);i.push_back(b);i.push_back(c);}
    void quad(Vec3 a,Vec3 b,Vec3 c,Vec3 d, Vec3 n){
        uint32_t k=uint32_t(v.size());
        v.push_back({a.x,a.y,a.z,n.x,n.y,n.z,0,1});
        v.push_back({b.x,b.y,b.z,n.x,n.y,n.z,1,1});
        v.push_back({c.x,c.y,c.z,n.x,n.y,n.z,1,0});
        v.push_back({d.x,d.y,d.z,n.x,n.y,n.z,0,0});
        triangle(k,k+1,k+2);triangle(k,k+2,k+3);
    }
};
static void ellipsoid(ModelData& m,Vec3 c,Vec3 scale,int rings=8,int segments=12){
    uint32_t k=uint32_t(m.v.size());
    for(int r=0;r<=rings;r++){
        float th=PI*float(r)/rings;
        for(int j=0;j<=segments;j++){
            float ph=2*PI*float(j)/segments;
            Vec3 n={std::sin(th)*std::cos(ph),std::cos(th),std::sin(th)*std::sin(ph)};
            Vec3 p={c.x+n.x*scale.x,c.y+n.y*scale.y,c.z+n.z*scale.z};
            Vec3 nn=normalize({n.x/scale.x,n.y/scale.y,n.z/scale.z});
            m.v.push_back({p.x,p.y,p.z,nn.x,nn.y,nn.z,float(j)/segments*3.f,float(r)/rings*2.f});
        }
    }
    for(int r=0;r<rings;r++)for(int j=0;j<segments;j++){
        uint32_t a=k+r*(segments+1)+j,b=a+segments+1;
        m.triangle(a,b,a+1);m.triangle(a+1,b,b+1);
    }
}
#include "forest_tree_mesh.hpp"
#include "forest_undergrowth.hpp"
#include "forest_leaf_litter.hpp"
#include "forest_fauna.hpp"
#include "forest_river.hpp"
#include "forest_frustum.hpp"
#include "forest_biome_props.hpp"
#include "forest_floor_details.hpp"
#include "forest_scanned_loader.hpp"
#include "forest_survival.hpp"

static ModelData makeCards(float width,float h,int crosses){
    ModelData m;
    for(int i=0;i<crosses;i++){
        float ang=float(i)*PI/float(crosses),ca=std::cos(ang),sa=std::sin(ang);
        Vec3 left{-ca*width*.5f,0,-sa*width*.5f};
        Vec3 right{ca*width*.5f,0,sa*width*.5f};
        m.quad(left,right,right+Vec3{0,h,0},left+Vec3{0,h,0},{-sa,0,ca});
    }
    return m;
}
static ModelData makeShadow(){
    ModelData m; m.quad({-2,0,-2},{2,0,-2},{2,0,2},{-2,0,2},{0,1,0});return m;
}
static ModelData makeCrystal(){
    ModelData m;
    const Vec3 p[6]={{0,2.0f,0},{-.7f,.65f,0},{0,.65f,-.7f},{.7f,.65f,0},{0,.65f,.7f},{0,0,0}};
    const int tri[8][3]={{0,1,2},{0,2,3},{0,3,4},{0,4,1},{5,2,1},{5,3,2},{5,4,3},{5,1,4}};
    for(const auto& t:tri){
        Vec3 a=p[t[0]],b=p[t[1]],c=p[t[2]],n=normalize(cross(b-a,c-a));
        uint32_t k=uint32_t(m.v.size());
        m.v.push_back({a.x,a.y,a.z,n.x,n.y,n.z,0,.5f});
        m.v.push_back({b.x,b.y,b.z,n.x,n.y,n.z,1,0});
        m.v.push_back({c.x,c.y,c.z,n.x,n.y,n.z,1,1});m.triangle(k,k+1,k+2);
    }
    return m;
}












static GLuint compile(GLenum stage,const char* source){
    GLuint s=glCreateShader(stage);glShaderSource(s,1,&source,nullptr);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char msg[2000]{};glGetShaderInfoLog(s,sizeof(msg),nullptr,msg);LOGE("shader error: %s",msg);glDeleteShader(s);return 0;}
    return s;
}
static GLuint program(const char*vs,const char*fs){
    GLuint a=compile(GL_VERTEX_SHADER,vs),b=compile(GL_FRAGMENT_SHADER,fs);
    if(!a||!b){if(a)glDeleteShader(a);if(b)glDeleteShader(b);return 0;}
    GLuint p=glCreateProgram();glAttachShader(p,a);glAttachShader(p,b);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char msg[2000]{};glGetProgramInfoLog(p,sizeof(msg),nullptr,msg);LOGE("link error: %s",msg);glDeleteProgram(p);p=0;}
    glDeleteShader(a);glDeleteShader(b);return p;
}
static GLuint createTexture(){
    GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    uint8_t px[]={128,128,255,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);return t;
}
static GLuint instanceBuffer=0;
static Mesh makeMesh(const ModelData& data){
    Mesh m;glGenVertexArrays(1,&m.vao);glGenBuffers(1,&m.vbo);glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(data.v.size()*sizeof(Vertex)),data.v.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(data.i.size()*sizeof(uint32_t)),data.i.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(5);glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(8*sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER,instanceBuffer);
    glEnableVertexAttribArray(3);glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Instance),(void*)0);
    glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(Instance),(void*)(4*sizeof(float)));
    glVertexAttribDivisor(3,1);glVertexAttribDivisor(4,1);
    glBindVertexArray(0);m.count=GLsizei(data.i.size());return m;
}
struct ChunkData {
    Mesh terrain,river;
    std::vector<Instance> broad,pine,grass,shrub,fern,seedling,litter,twigs,fungi,reeds,flowers,stones,logs,shadow;
};
struct Batches {
    std::vector<Instance> broad,pine,grass,shrub,fern,seedling,litter,twigs,fungi,reeds,flowers,stones,logs,shadow;
    void clear(){broad.clear();pine.clear();grass.clear();shrub.clear();fern.clear();seedling.clear();litter.clear();twigs.clear();fungi.clear();reeds.clear();flowers.clear();stones.clear();logs.clear();shadow.clear();}
};
struct Relic {float x,z;bool picked=false;};
struct State {
    int width=1280,height=720;
    GLuint terrainProg=0,instProg=0,shadowProg=0,skyProg=0,skyVao=0,treeDepthProg=0;
    GLuint postProg=0,postFbo=0,postColor=0,postDepth=0;
    int postWidth=0,postHeight=0;
    bool postReady=false;
    GLuint treeDepthFbo=0,treeDepthTex=0;
    int depthMapSize=0;
    bool treeDepthReady=false;
    Mat4 sunShadowMatrix{};
    GLuint tex[39]{};
    uint64_t texBytes[39]{};
    GLuint riverProg=0;
    Mesh wildlifeMeshes[3],reedsMesh,flowersMesh;
    Mesh scannedStump[3],scannedDeadlog[3];
    bool scannedStumpReady[3]{},scannedDeadlogReady[3]{};
    std::vector<ForestAnimal> wildlife;
    float animalRespawnT=0.f;
    float feetOffset=0.f,verticalVelocity=0.f;
    bool jumpQueued=false;
    int jumps=0;
    unsigned seenSpecies=0;
    bool reachedRiver=false;
    ForestSurvival survival{};
    GLuint windTex=0;
    bool windVulkan=false;
    Mesh woodHi[5],woodMid[5],leavesHi[5],leavesMid[5],leavesFar[5];
    Mesh grass,shrub,rock,fallenLog,shadow,crystal;
    Mesh shrubBranches,shrubLeaflets,fernFronds,groundSeedlings,leafLitter,dryTwigs,fungiStems,fungiCaps;
    std::unordered_map<int64_t,ChunkData> chunks;
    Batches batches;
    bool batchesDirty=true;
    float x=0,z=0,yaw=.35f,pitch=-.06f,t=0;
    float moveX=0,moveZ=0,lookX=0,lookY=0;
    bool running=false,collect=false;
    int graphicsQuality=1;  // 0 ECO, 1 ALTA, 2 ULTRA
    std::array<Relic,RELIC_COUNT> relics{{{38,36},{-96,72},{149,-119},{-200,-176},{299,193},{-370,308}}};
    int collected=0;
    bool ready=false;
};
static State game;

static int64_t key(int x,int z){return int64_t((uint64_t(uint32_t(x))<<32)|uint32_t(z));}
static void freeMesh(Mesh &m){
    if(m.ebo)glDeleteBuffers(1,&m.ebo);
    if(m.vbo)glDeleteBuffers(1,&m.vbo);
    if(m.vao)glDeleteVertexArrays(1,&m.vao);m={};
}
static void freeChunk(ChunkData &c){freeMesh(c.terrain);freeMesh(c.river);}
static ModelData makeTerrain(int cx,int cz){
    ModelData m; m.v.reserve((GRID+1)*(GRID+1));m.i.reserve(GRID*GRID*6);
    for(int j=0;j<=GRID;j++)for(int i=0;i<=GRID;i++){
        float x=cx*CHUNK+float(i)*CHUNK/GRID;
        float z=cz*CHUNK+float(j)*CHUNK/GRID;
        float y=height(x,z);Vec3 n=normalAt(x,z);
        m.v.push_back({x,y,z,n.x,n.y,n.z,x,z});
    }
    for(int j=0;j<GRID;j++)for(int i=0;i<GRID;i++){
        uint32_t a=j*(GRID+1)+i,b=a+GRID+1;
        m.triangle(a,a+1,b);m.triangle(a+1,b+1,b);
    }
    return m;
}
static ChunkData makeChunk(int cx,int cz){
    ChunkData out;out.terrain=makeMesh(makeTerrain(cx,cz));
    ModelData water=forestRiverSurface(cx,cz);
    if(!water.i.empty())out.river=makeMesh(water);
    std::mt19937 rng(hash32(uint32_t(cx)*12582917u ^ uint32_t(cz)*4256249u ^ 0x8af553u));
    std::uniform_real_distribution<float> unit(0,1);
    auto samplePos=[&]()->Vec3{
        float x=cx*CHUNK+3.f+unit(rng)*(CHUNK-6.f);
        float z=cz*CHUNK+3.f+unit(rng)*(CHUNK-6.f);
        return {x,height(x,z),z};
    };
    const float forestDensity=.65f+noise((cx+.5f)*.57f,(cz+.5f)*.57f)*.72f;
    const Vec3 biomeCenter{cx*CHUNK+CHUNK*.5f,0,cz*CHUNK+CHUNK*.5f};
    const ForestBiome biome=forestBiome(biomeCenter.x,biomeCenter.z);
    const int treeCount=int((biome==ForestBiome::Meadow?5.f:
            (biome==ForestBiome::Wetland?8.f:12.f))*forestDensity);
    for(int i=0;i<treeCount;i++){
        Vec3 p=samplePos();
        if(std::hypot(p.x,p.z)<8.f ||
           riverDistance(p.x,p.z)<riverWidth(p.z)+2.8f)continue;
        float scale=.67f+unit(rng)*.79f,angle=unit(rng)*6.283f,tint=unit(rng);
        Instance ins{p.x,p.y,p.z,scale,angle,tint,0,0};
        const float pineChance=biome==ForestBiome::Conifer?.82f:
            (biome==ForestBiome::Wetland?.12f:.33f);
        if(unit(rng)<pineChance){ins.type=1;out.pine.push_back(ins);}
        else out.broad.push_back(ins);
        out.shadow.push_back({p.x,p.y+.06f,p.z,scale*1.65f,0,tint,0,0});
    }
    // Clustered rather than uniform undergrowth: wetter pockets grow denser ferns.
    const int undergrowthCount=int((biome==ForestBiome::Meadow?450.f:
         biome==ForestBiome::Wetland?470.f:340.f)*forestDensity);
    for(int i=0;i<undergrowthCount;i++){
        Vec3 p=samplePos();float scale=.55f+unit(rng)*1.10f;
        Instance ins{p.x,p.y+.025f,p.z,scale,unit(rng)*6.283f,unit(rng),0,0};
        const float moisture=noise(p.x*.021f+49.f,p.z*.021f-17.f);
        // Local canopy shade is derived from *actual* generated trees:
        // understory growth is now tied to the upper canopy rather than
        // being scattered equally in an empty field and a dark forest.
        float canopyShade=0.f;
        for(const Instance& tree:out.broad){
            float dx=p.x-tree.x,dz=p.z-tree.z;
            canopyShade=std::max(canopyShade,(1.f-clampf(std::sqrt(dx*dx+dz*dz)/13.f,0.f,1.f)));
        }
        for(const Instance& tree:out.pine){
            float dx=p.x-tree.x,dz=p.z-tree.z;
            canopyShade=std::max(canopyShade,(1.f-clampf(std::sqrt(dx*dx+dz*dz)/10.f,0.f,1.f))*.73f);
        }
        const float shade=clampf(.2f+canopyShade*.78f+
                         noise(p.x*.042f-2.f,p.z*.042f+8.f)*.14f,0.f,1.f);
        float slope=normalAt(p.x,p.z).y;
        if(slope<.80f || riverDistance(p.x,p.z)<riverWidth(p.z)-1.f)continue;
        const float waterEdge=riverDistance(p.x,p.z)-riverWidth(p.z);
        if(waterEdge<10.f && waterEdge>0.f && unit(rng)<.45f){
            out.reeds.push_back({p.x,p.y,p.z,scale*.84f,ins.rotation,ins.tint,0,0});
            continue;
        }
        float dice=unit(rng);
        if(dice < .080f && shade>.39f){
            out.shrub.push_back({p.x,p.y,p.z,scale*1.25f,ins.rotation,ins.tint,0,0});
        }else if(dice<.19f+canopyShade*.10f && moisture>.38f){
            out.fern.push_back({p.x,p.y,p.z,scale*.86f,ins.rotation,ins.tint,0,0});
        }else if(dice<.35f && biome==ForestBiome::Meadow && canopyShade<.48f){
            out.flowers.push_back({p.x,p.y,p.z,scale*.76f,ins.rotation,ins.tint,0,0});
        }else if(dice<.32f+canopyShade*.08f){
            out.seedling.push_back({p.x,p.y+.015f,p.z,scale*.63f,ins.rotation,ins.tint,0,0});
        }else{
            out.grass.push_back(ins);
        }
    }
    // Only microclimates suitable for leaf litter receive instanced leaf piles.
    for(int i=0;i<int(82.f*forestDensity);i++){
        Vec3 p=samplePos();
        if(riverDistance(p.x,p.z)<riverWidth(p.z)+1.f)continue;
        float density=noise(p.x*.036f+19.f,p.z*.036f-2.f);
        if(density<.31f || normalAt(p.x,p.z).y<.91f)continue;
        if(std::hypot(p.x,p.z)<4.f)continue;
        out.litter.push_back({p.x,p.y+.024f,p.z,.68f+unit(rng)*.60f,
                              unit(rng)*2.f*PI,unit(rng),0,0});
    }
    // Forest floor patches: mushrooms favor dark moist under-canopy micro-sites,
    // while dry twigs collect around (but not directly inside) trunks.
    for(int i=0;i<int(33.f*forestDensity);i++){
        Vec3 p=samplePos();
        if(riverDistance(p.x,p.z)<riverWidth(p.z)+1.0f)continue;
        if(normalAt(p.x,p.z).y<.84f)continue;
        float shade=0.f;
        for(const Instance &t:out.broad){
            float d=std::hypot(p.x-t.x,p.z-t.z);
            shade=std::max(shade,1.f-clampf(d/12.f,0.f,1.f));
        }
        for(const Instance &t:out.pine){
            float d=std::hypot(p.x-t.x,p.z-t.z);
            shade=std::max(shade,(1.f-clampf(d/10.f,0.f,1.f))*.7f);
        }
        float moisture=noise(p.x*.019f+2.f,p.z*.019f-13.f);
        const float coin=unit(rng);
        if(shade>.28f && moisture>.51f && coin<.38f){
            out.fungi.push_back({p.x,p.y+.022f,p.z,.78f+unit(rng)*.54f,
                                 unit(rng)*2.f*PI,unit(rng),0,0});
        }else if(shade>.16f && coin<.70f){
            out.twigs.push_back({p.x,p.y+.035f,p.z,.65f+unit(rng)*.66f,
                                 unit(rng)*2.f*PI,unit(rng),0,0});
        }
    }
    for(int i=0;i<3;i++){
        Vec3 p=samplePos();
        if(std::hypot(p.x,p.z)>12.f){
            out.logs.push_back({p.x,p.y+.08f,p.z,
                .70f+unit(rng)*.62f,unit(rng)*6.283f,unit(rng),0.f,0.f});
        }
    }
    for(int i=0;i<8;i++){
        Vec3 p=samplePos();out.stones.push_back({p.x,p.y-.2f,p.z,.35f+unit(rng)*.7f,unit(rng)*6.283f,unit(rng),0,0});
    }
    return out;
}
static void append(std::vector<Instance>& a,const std::vector<Instance>& b){a.insert(a.end(),b.begin(),b.end());}
static void rebuildBatches(){
    Batches& b=game.batches;b.clear();
    for(auto& pair:game.chunks){const ChunkData& c=pair.second;
        append(b.broad,c.broad);append(b.pine,c.pine);append(b.grass,c.grass);
        append(b.shrub,c.shrub);append(b.fern,c.fern);append(b.seedling,c.seedling);append(b.litter,c.litter);append(b.twigs,c.twigs);append(b.fungi,c.fungi);append(b.reeds,c.reeds);append(b.flowers,c.flowers);
        append(b.stones,c.stones);append(b.logs,c.logs);append(b.shadow,c.shadow);
    }
    game.batchesDirty=false;
}
static void streamWorld(){
    int cx=int(std::floor(game.x/CHUNK)),cz=int(std::floor(game.z/CHUNK));
    for(auto it=game.chunks.begin();it!=game.chunks.end();){
        int tx=int(int32_t(uint32_t(uint64_t(it->first)>>32)));
        int tz=int(int32_t(uint32_t(uint64_t(it->first))));
        if(std::abs(tx-cx)>RADIUS || std::abs(tz-cz)>RADIUS){
            freeChunk(it->second);it=game.chunks.erase(it);game.batchesDirty=true;
        }else ++it;
    }
    int built=0;
    // Closest blocks load first, to avoid empty terrain near the player.
    for(int r=0;r<=RADIUS && built<8;r++)for(int dz=-r;dz<=r && built<8;dz++)
        for(int dx=-r;dx<=r && built<8;dx++){
            if(std::max(std::abs(dx),std::abs(dz))!=r)continue;
            int a=cx+dx,b=cz+dz;
            if(a < -16 || a > 15 || b < -16 || b > 15)continue;
            auto k=key(a,b);
            if(game.chunks.find(k)==game.chunks.end()){
                game.chunks.emplace(k,makeChunk(a,b));game.batchesDirty=true;built++;
            }
        }
    if(game.batchesDirty)rebuildBatches();
}
static void set3(GLuint prog,const char* key,Vec3 p){
    glUniform3f(glGetUniformLocation(prog,key),p.x,p.y,p.z);
}
static void set1(GLuint prog,const char* key,float v){glUniform1f(glGetUniformLocation(prog,key),v);}
static void common(GLuint prog,const Mat4& vp,Vec3 eye,Vec3 fog,float light){
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uVP"),1,GL_FALSE,vp.m);
    set3(prog,"uEye",eye);set3(prog,"uFog",fog);
    set3(prog,"uSun",normalize({.62f,.87f,.33f}));set1(prog,"uLight",light);
}
static void instDraw(const Mesh& mesh,const std::vector<Instance>& instances){
    if(instances.empty())return;
    glBindBuffer(GL_ARRAY_BUFFER,instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(instances.size()*sizeof(Instance)),instances.data(),GL_STREAM_DRAW);
    glBindVertexArray(mesh.vao);
    glDrawElementsInstanced(GL_TRIANGLES,mesh.count,GL_UNSIGNED_INT,nullptr,GLsizei(instances.size()));
    countDraw(mesh.count,uint32_t(instances.size()));
    glBindVertexArray(0);
}
static void material(int index,float cutout,float wind,float emissive=0.f,float spin=0.f){
    int layer=-1;
    if(index==5)layer=0;      // grass
    else if(index==6)layer=1; // bush
    else if(index==11)layer=2;// broadleaf
    else if(index==13)layer=3;// pine needles
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,game.tex[layer>=0?19:index]);
    glUniform1i(glGetUniformLocation(game.instProg,"uTex"),0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,game.tex[layer>=0?20:
        (index==33?34:(index==36?37:(index==27?28:(index==15?16:(index==17?18:10)))))]);
    glUniform1i(glGetUniformLocation(game.instProg,"uMaterialNormal"),1);
    if(layer>=0){
        float x=(layer%2)*.5f,y=(layer/2)*.5f;
        // Half-pixel inset prevents cross-material mipmap/anisotropic bleeding.
        glUniform4f(glGetUniformLocation(game.instProg,"uAtlasRect"),
                    x+.00065f,y+.00065f,.4987f,.4987f);
    }else{
        glUniform4f(glGetUniformLocation(game.instProg,"uAtlasRect"),0.f,0.f,1.f,1.f);
    }
    const int roughSlot=index==3?23:(index==15?24:(index==17?31:(index==33?35:(index==36?38:-1))));
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D,game.tex[roughSlot>=0?roughSlot:23]);
    glUniform1i(glGetUniformLocation(game.instProg,"uMaterialRough"),5);
    set1(game.instProg,"uUseRoughnessMap",roughSlot>=0?1.f:0.f);
    set1(game.instProg,"uCutout",cutout);
    set1(game.instProg,"uWind",wind);
    set1(game.instProg,"uEmissive",emissive);
    set1(game.instProg,"uSpin",spin);
    set1(game.instProg,"uRoughness",(index==3||index==15||index==33||index==36)?.83f:(index==17?.72f:.94f));
    set1(game.instProg,"uBump",(index==3||index==15||index==33||index==36)?.63f:
          (index==17?.42f:((index==11||index==13||index==5||index==6)?.64f:
                            (index==27?.30f:0.f))));
    set1(game.instProg,"uFoliage",(index==4||index==5||index==6||index==11||index==13)?1.f:0.f);
    set1(game.instProg,"uLeafFlutter",(index==11||index==13)?0.04f:0.f);
    set1(game.instProg,"uLeafLitter",0.f);
    set1(game.instProg,"uInteractiveVegetation",0.f);
    set1(game.instProg,"uAnimalGait",index==27?1.f:0.f);
    set1(game.instProg,"uPivotWind",(index==11||index==13)?.105f:
                                      ((index==3||index==15||index==33||index==36)?.040f:0.f));
}
static void simulateWildlife(float dt){
    // Deterministic population around the moving player; persistent agents
    // keep their paths until they stream out. Capped for predictable mobile cost.
    if(game.wildlife.empty()){
        std::mt19937 rng(0x5EED851u);
        std::uniform_real_distribution<float> random(0.f,1.f);
        for(int i=0;i<30;i++){
            float angle=random(rng)*2.f*PI;
            float radius=24.f+random(rng)*105.f;
            ForestAnimal a;
            a.x=clampf(game.x+std::cos(angle)*radius,-WORLD_HALF+15,WORLD_HALF-15);
            a.z=clampf(game.z+std::sin(angle)*radius,-WORLD_HALF+15,WORLD_HALF-15);
            a.homeX=a.x;a.homeZ=a.z;
            a.species=i%3;a.angle=random(rng)*2.f*PI;
            a.phase=random(rng)*6.f;
            game.wildlife.push_back(a);
        }
    }
    for(auto &a:game.wildlife){
        float dx=a.x-game.x,dz=a.z-game.z;
        float ds=std::sqrt(dx*dx+dz*dz);
        if(ds>175.f){
            // Reuse agents instead of allocating and destroying actors.
            float phase=a.phase*9.1f+float(a.species)*2.1f;
            a.x=clampf(game.x+std::cos(phase)*125.f,-WORLD_HALF+12,WORLD_HALF-12);
            a.z=clampf(game.z+std::sin(phase)*125.f,-WORLD_HALF+12,WORLD_HALF-12);
            a.homeX=a.x;a.homeZ=a.z;
            ds=125.f;
        }
        a.phase+=dt*(a.species==2?5.f:3.5f);
        // Bounded finite-state ecosystem AI; species react differently:
        // resting, foraging, routine roaming, warning and escaping.
        if(ds<17.f)a.behavior=ForestAnimalBehavior::Flee;
        else if(ds<27.f)a.behavior=ForestAnimalBehavior::Alert;
        else{
            float cycle=std::fmod(a.phase*.075f+float(a.species)*.21f,4.f);
            a.behavior=cycle<.45f?ForestAnimalBehavior::Rest:
                       (cycle<1.80f?ForestAnimalBehavior::Graze:
                        ForestAnimalBehavior::Roam);
        }
        a.fleeing=a.behavior==ForestAnimalBehavior::Flee;
        if(a.fleeing){
            a.angle=std::atan2(dx,dz);
        }else{
            a.angle+=dt*(.17f*std::sin(a.phase*.12f+a.species*3.f));
            float homeDX=a.homeX-a.x,homeDZ=a.homeZ-a.z;
            if(homeDX*homeDX+homeDZ*homeDZ>58.f*58.f){
                a.angle=std::atan2(homeDX,homeDZ);
            }
        }
        float stepSpeed=0.f;
        switch(a.behavior){
            case ForestAnimalBehavior::Flee:
                stepSpeed=a.species==0?5.0f:(a.species==2?4.2f:3.2f);break;
            case ForestAnimalBehavior::Alert:stepSpeed=.08f;break;
            case ForestAnimalBehavior::Graze:stepSpeed=.11f;break;
            case ForestAnimalBehavior::Rest:stepSpeed=0.f;break;
            case ForestAnimalBehavior::Roam:
                stepSpeed=.56f+.23f*std::sin(a.phase*.38f);break;
        }
        float movement=stepSpeed*dt;
        const float nx=clampf(a.x+std::sin(a.angle)*movement,-WORLD_HALF+11,WORLD_HALF-11);
        const float nz=clampf(a.z+std::cos(a.angle)*movement,-WORLD_HALF+11,WORLD_HALF-11);
        if(riverDistance(nx,nz)>riverWidth(nz)+2.2f){
            a.x=nx;a.z=nz;
        }else{
            a.angle+=PI*.43f*dt+dt*2.f;
        }
        if(std::hypot(a.x-game.x,a.z-game.z)<19.f){
            game.seenSpecies|=1u<<unsigned(a.species);
        }
    }
}

static void update(float dt){
    dt=clampf(dt,0.f,.05f);game.t+=dt;
    game.yaw-=game.lookX*.0034f;game.pitch=clampf(game.pitch-game.lookY*.003f,-1.35f,1.35f);
    game.lookX=game.lookY=0;
    simulateWildlife(dt);
    const float distanceToRiver=riverDistance(game.x,game.z);
    if(distanceToRiver<riverWidth(game.z)+5.f)game.reachedRiver=true;
    if(game.jumpQueued && game.feetOffset<.002f){
        game.verticalVelocity=6.4f;
        game.jumps++;
    }
    game.jumpQueued=false;
    if(game.feetOffset>0.f || game.verticalVelocity>0.f){
        game.feetOffset+=game.verticalVelocity*dt;
        game.verticalVelocity-=17.5f*dt;
        if(game.feetOffset<0.f){game.feetOffset=0;game.verticalVelocity=0;}
    }
    Vec3 f{std::sin(game.yaw),0,std::cos(game.yaw)},rt{-f.z,0,f.x};
    Vec3 delta=f*game.moveZ+rt*game.moveX;
    if(dot(delta,delta)>1.f)delta=normalize(delta);
    const bool immersed=riverDistance(game.x,game.z)<riverWidth(game.z)-1.f;
    const bool sprinting=game.running && game.survival.canSprint();
    game.survival.tick(dt,sprinting,immersed);
    float speed=(sprinting?7.5f:4.2f)*(immersed?.55f:1.f)*dt;
    float nx=clampf(game.x+delta.x*speed,-WORLD_HALF+8,WORLD_HALF-8);
    float nz=clampf(game.z+delta.z*speed,-WORLD_HALF+8,WORLD_HALF-8);
    auto blocked=[&](float px,float pz){
        for(const auto& pair:game.chunks){const auto& c=pair.second;
            for(const auto& group:{&c.broad,&c.pine}){
                for(const auto& t:*group){
                    float dx=px-t.x,dz=pz-t.z,r=.31f*t.s+.43f;
                    if(dx*dx+dz*dz<r*r)return true;
                }
            }
        }
        return false;
    };
    if(!blocked(nx,game.z))game.x=nx;
    if(!blocked(game.x,nz))game.z=nz;
    if(game.collect){
        const int before=game.collected;
        for(auto &r:game.relics){
            if(r.picked)continue;
            if(std::hypot(game.x-r.x,game.z-r.z)<3.6f){r.picked=true;game.collected++;break;}
        }
        if(game.collected==before && game.survival.gatherCooldown<=.0f){
            // A single gather action searches nearby pre-generated resources.
            // No new meshes, allocations, or network activity are required.
            float best=2.8f*2.8f;
            int resource=-1;
            auto scan=[&](const std::vector<Instance>& group,int kind){
                for(const Instance& obj:group){
                    float dx=game.x-obj.x,dz=game.z-obj.z;
                    float ds=dx*dx+dz*dz;
                    if(ds<best){best=ds;resource=kind;}
                }
            };
            scan(game.batches.logs,0);
            scan(game.batches.stones,1);
            scan(game.batches.fern,2);
            scan(game.batches.flowers,2);
            if(resource>=0)game.survival.harvest(resource);
        }
    }
    game.collect=false;
}
static bool ensureSunShadowFbo(int size){
    if(game.treeDepthReady&&size==game.depthMapSize)return true;
    if(game.treeDepthFbo)glDeleteFramebuffers(1,&game.treeDepthFbo);
    if(game.treeDepthTex)glDeleteTextures(1,&game.treeDepthTex);
    game.treeDepthFbo=game.treeDepthTex=0;
    game.treeDepthReady=false;
    game.depthMapSize=size;
    glGenTextures(1,&game.treeDepthTex);
    glBindTexture(GL_TEXTURE_2D,game.treeDepthTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,size,size,0,
                 GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
    glGenFramebuffers(1,&game.treeDepthFbo);
    glBindFramebuffer(GL_FRAMEBUFFER,game.treeDepthFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,game.treeDepthTex,0);
    GLenum status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status==GL_FRAMEBUFFER_COMPLETE){
        const GLenum attachment=GL_NONE;
        glDrawBuffers(1,&attachment);
        game.treeDepthReady=true;
    }else{
        LOGE("Shadow FBO unsupported: 0x%x",unsigned(status));
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return game.treeDepthReady;
}
static void treeShadowMaterial(int textureIndex,bool alpha){
    set1(game.treeDepthProg,"uPivotWind",alpha?.105f:.040f);
    if(alpha){
        int layer=textureIndex==13?3:2;
        float u=float(layer%2)*.5f,v=float(layer/2)*.5f;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,game.tex[19]);
        glUniform4f(glGetUniformLocation(game.treeDepthProg,"uRect"),
                    u+.00065f,v+.00065f,.4987f,.4987f);
        glUniform1f(glGetUniformLocation(game.treeDepthProg,"uThreshold"),.44f);
    }else{
        glUniform1f(glGetUniformLocation(game.treeDepthProg,"uThreshold"),0.f);
        glUniform4f(glGetUniformLocation(game.treeDepthProg,"uRect"),0,0,1,1);
    }
}
static void castForestShadows(const std::vector<Instance>& broad,
                              const std::vector<Instance>& pine,int quality){
    if(!game.treeDepthProg||!ensureSunShadowFbo(quality==2?2048:(quality==0?768:1536)))return;
    Vec3 center{game.x,height(game.x,game.z)+2.5f,game.z};
    Vec3 sunlight=normalize({.62f,.87f,.33f});
    Vec3 eye=center+sunlight*118.f;
    float coverage=quality==2?100.f:(quality==0?65.f:82.f);
    game.sunShadowMatrix=multiply(orthographic(-coverage,coverage,-coverage,coverage,
                                             1.f,260.f),lookAt(eye,center));
    glBindFramebuffer(GL_FRAMEBUFFER,game.treeDepthFbo);
    glViewport(0,0,game.depthMapSize,game.depthMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUseProgram(game.treeDepthProg);
    glUniformMatrix4fv(glGetUniformLocation(game.treeDepthProg,"uLightVP"),
                       1,GL_FALSE,game.sunShadowMatrix.m);
    glUniform1i(glGetUniformLocation(game.treeDepthProg,"uMask"),0);
    set1(game.treeDepthProg,"uTime",game.t);
    // Keep shadows dense near player without drawing distant, invisible trees.
    std::array<std::vector<Instance>,5> plants;
    auto add=[&](const std::vector<Instance>& group,bool conifer){
        for(const auto& t:group){
            float dx=t.x-game.x,dz=t.z-game.z;
            if(dx*dx+dz*dz>coverage*coverage)continue;
            int species=conifer?3+std::min(1,int(t.tint*2.f)):std::min(2,int(t.tint*3.f));
            plants[species].push_back(t);
        }
    };
    add(broad,false);add(pine,true);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.4f,2.0f);
    for(int i=0;i<5;i++){
        treeShadowMaterial(3,false);
        instDraw(game.woodMid[i],plants[i]);
        treeShadowMaterial(i>=3?13:11,true);
        instDraw(game.leavesMid[i],plants[i]);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
static void bindSunShadow(GLuint programId,int slot){
    const bool active=game.treeDepthReady;
    glUseProgram(programId);
    glUniform1f(glGetUniformLocation(programId,"uShadowsEnabled"),active?1.f:0.f);
    if(!active)return;
    glUniformMatrix4fv(glGetUniformLocation(programId,"uSunShadowVP"),
                       1,GL_FALSE,game.sunShadowMatrix.m);
    glActiveTexture(GL_TEXTURE0+slot);
    glBindTexture(GL_TEXTURE_2D,game.treeDepthTex);
    glUniform1i(glGetUniformLocation(programId,"uSunShadow"),slot);
    glUniform1f(glGetUniformLocation(programId,"uShadowTexel"),1.f/float(game.depthMapSize));
    glUniform1f(glGetUniformLocation(programId,"uShadowQuality"),float(game.graphicsQuality));
}

static bool ensurePostFrameBuffer(){
    const int w=game.width,h=game.height;
    if(game.postReady&&game.postWidth==w&&game.postHeight==h)return true;
    if(game.postFbo)glDeleteFramebuffers(1,&game.postFbo);
    if(game.postColor)glDeleteTextures(1,&game.postColor);
    if(game.postDepth)glDeleteTextures(1,&game.postDepth);
    game.postFbo=game.postColor=game.postDepth=0;
    game.postReady=false;
    if(w<=0||h<=0||w>4096||h>4096||!game.postProg)return false;
    glGenTextures(1,&game.postColor);
    glBindTexture(GL_TEXTURE_2D,game.postColor);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenTextures(1,&game.postDepth);
    glBindTexture(GL_TEXTURE_2D,game.postDepth);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,w,h,0,
                 GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_NONE);
    glGenFramebuffers(1,&game.postFbo);
    glBindFramebuffer(GL_FRAMEBUFFER,game.postFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,game.postColor,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,game.postDepth,0);
    const bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    game.postWidth=w;game.postHeight=h;
    game.postReady=ok;
    if(!ok)LOGE("Postprocessing FBO unavailable: direct rendering fallback");
    return ok;
}
static void presentPostFrame(Vec3 eye){
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,game.width,game.height);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(game.postProg);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,game.postColor);
    glUniform1i(glGetUniformLocation(game.postProg,"uColor"),0);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,game.postDepth);
    glUniform1i(glGetUniformLocation(game.postProg,"uDepth"),1);
    glUniform2f(glGetUniformLocation(game.postProg,"uResolution"),
                float(game.width),float(game.height));
    glUniform1f(glGetUniformLocation(game.postProg,"uQuality"),
                game.graphicsQuality==2?2.0f:0.0f);
    const bool shafts=game.graphicsQuality==2 && game.treeDepthReady;
    glUniform1f(glGetUniformLocation(game.postProg,"uGodRaysEnabled"),
                shafts?1.f:0.f);
    if(shafts){
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D,game.treeDepthTex);
        glUniform1i(glGetUniformLocation(game.postProg,"uSunShadow"),2);
        glUniformMatrix4fv(glGetUniformLocation(game.postProg,"uSunShadowVP"),
                           1,GL_FALSE,game.sunShadowMatrix.m);
        set3(game.postProg,"uEye",eye);
        set3(game.postProg,"uSunDirection",normalize({.62f,.87f,.33f}));
        glUniform2f(glGetUniformLocation(game.postProg,"uCamera"),game.yaw,game.pitch);
    }
    glBindVertexArray(game.skyVao);
    glDrawArrays(GL_TRIANGLES,0,3);
    countDraw(3);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
}
static void draw(float dt){
    if(!game.ready)return;
    const auto cpuStart=std::chrono::steady_clock::now();
    debugMetrics.drawCalls=0;
    debugMetrics.triangles=0;
    update(std::min(dt,.05f));streamWorld();
    if(game.graphicsQuality>0&&game.graphicsQuality<3&&game.treeDepthProg)
        castForestShadows(game.batches.broad,game.batches.pine,game.graphicsQuality);
    else game.treeDepthReady=false;
    const bool usePost=game.graphicsQuality>0&&game.graphicsQuality<3&&ensurePostFrameBuffer();
    glBindFramebuffer(GL_FRAMEBUFFER,usePost?game.postFbo:0);
    glViewport(0,0,game.width,game.height);
    // Soft daytime colors, with a slowly cycling evening phase.
    float cycle=.5f+.5f*std::sin(game.t*.0105f+.85f);
    float light=.59f+.41f*cycle;
    Vec3 fog={.56f+.16f*cycle,.65f+.17f*cycle,.68f+.20f*cycle};
    glClearColor(fog.x,fog.y,fog.z,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    // Atmospheric sky is a fullscreen triangle, NOT a flat static PNG background.
    if(game.skyProg){
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
        glUseProgram(game.skyProg);
        glUniform2f(glGetUniformLocation(game.skyProg,"uViewport"),float(game.width),float(game.height));
        glUniform2f(glGetUniformLocation(game.skyProg,"uCamera"),game.yaw,game.pitch);
        glUniform1f(glGetUniformLocation(game.skyProg,"uTime"),game.t);
        glUniform1f(glGetUniformLocation(game.skyProg,"uSunPower"),light);
        set3(game.skyProg,"uSunDir",normalize({.62f,.87f,.33f}));
        set3(game.skyProg,"uHaze",fog);
        glBindVertexArray(game.skyVao);glDrawArrays(GL_TRIANGLES,0,3);
        countDraw(3);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
    }
    Vec3 eye{game.x,height(game.x,game.z)+game.feetOffset+1.72f,game.z};
    Vec3 dir{std::sin(game.yaw)*std::cos(game.pitch),std::sin(game.pitch),std::cos(game.yaw)*std::cos(game.pitch)};
    Mat4 view=lookAt(eye,eye+dir);
    Mat4 proj=perspective(68.f*PI/180.f,float(game.width)/std::max(game.height,1),.11f,310.f);
    Mat4 vp=multiply(proj,view);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_BLEND);
    common(game.terrainProg,vp,eye,fog,light);
    set1(game.terrainProg,"uLegacyShading",game.graphicsQuality==3?1.f:0.f);
    bindSunShadow(game.terrainProg,6);
    for(int i=0;i<3;i++){
        glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,game.tex[i]);
        glActiveTexture(GL_TEXTURE3+i);glBindTexture(GL_TEXTURE_2D,game.tex[7+i]);
    }
    glUniform1i(glGetUniformLocation(game.terrainProg,"uGrass"),0);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uDirt"),1);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uMoss"),2);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uGrassNormal"),3);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uDirtNormal"),4);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uMossNormal"),5);
    glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,game.tex[21]);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uGrassRough"),7);
    glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_2D,game.tex[22]);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uDirtRough"),8);
    glActiveTexture(GL_TEXTURE9);glBindTexture(GL_TEXTURE_2D,game.tex[26]);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uRiverPebbles"),9);
    glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D,game.tex[30]);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uRiverNormal"),10);
    glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,game.tex[32]);
    glUniform1i(glGetUniformLocation(game.terrainProg,"uMossRough"),11);
    for(const auto& pair:game.chunks){const Mesh& m=pair.second.terrain;
        if(!forestChunkVisible(vp,pair.first))continue;
        glBindVertexArray(m.vao);glDrawElements(GL_TRIANGLES,m.count,GL_UNSIGNED_INT,nullptr);
        countDraw(m.count);
    }
    glBindVertexArray(0);
    // The circular placeholder shadow is only used when depth maps are unavailable.
    if(!game.treeDepthReady){
    glUseProgram(game.shadowProg);
    glUniformMatrix4fv(glGetUniformLocation(game.shadowProg,"uVP"),1,GL_FALSE,vp.m);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);instDraw(game.shadow,game.batches.shadow);glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    }
    common(game.instProg,vp,eye,fog,light);
    set1(game.instProg,"uLegacyShading",game.graphicsQuality==3?1.f:0.f);
    bindSunShadow(game.instProg,4);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,game.windTex);
    glUniform1i(glGetUniformLocation(game.instProg,"uWindField"),2);
    set1(game.instProg,"uTime",game.t);
    set3(game.instProg,"uPlayerPosition",Vec3{game.x,height(game.x,game.z),game.z});
    // Quality profiles change both triangle counts and alpha-fragment pressure.
    const float nearMeters=game.graphicsQuality==0?32.f:(game.graphicsQuality==2?74.f:52.f);
    const float midMeters=game.graphicsQuality==0?88.f:(game.graphicsQuality==2?155.f:118.f);
    const float grassMeters=game.graphicsQuality==0?42.f:(game.graphicsQuality==2?107.f:75.f);
    const float maxMeters=game.graphicsQuality==0?176.f:(game.graphicsQuality==2?250.f:242.f);
    // Tree instancing with separate species meshes and three geometry LODs.
    // Near: complete branched wood and modelled leaf sprigs.
    // Mid: reduced branches/cards. Far: coarse silhouette (no alpha-test forest).
    std::array<std::vector<Instance>,5> nearTrees,midTrees,farTrees;
    const float forwardX=std::sin(game.yaw),forwardZ=std::cos(game.yaw);
    auto assignTree=[&](const Instance& t,bool pine){
        const float dx=t.x-game.x,dz=t.z-game.z,d2=dx*dx+dz*dz;
        if(d2>maxMeters*maxMeters)return;
        const float distance=std::sqrt(d2);
        if(dx*forwardX+dz*forwardZ < -distance*.52f-12.f)return;
        const int species=pine?
            3+std::min(1,int(t.tint*2.f)):
            std::min(2,int(t.tint*3.f));
        if(d2<nearMeters*nearMeters)nearTrees[species].push_back(t);
        else if(d2<midMeters*midMeters)midTrees[species].push_back(t);
        else farTrees[species].push_back(t);
    };
    for(const auto& t:game.batches.broad)assignTree(t,false);
    for(const auto& t:game.batches.pine)assignTree(t,true);

    for(int s=0;s<5;s++){
        // 3 photographed tree bark species on the 3 broadleaf models.
        material(s>=3?15:(s==0?3:(s==1?33:36)),0.f,0.f);
        instDraw(game.woodHi[s],nearTrees[s]);
        instDraw(game.woodMid[s],midTrees[s]);
        instDraw(game.woodMid[s],farTrees[s]);
    }
    for(int s=0;s<5;s++){
        material(s>=3?13:11,.44f,.75f);
        instDraw(game.leavesHi[s],nearTrees[s]);
        material(s>=3?13:11,.44f,.48f);
        instDraw(game.leavesMid[s],midTrees[s]);
    }
    material(4,0.f,.15f);
    for(int s=0;s<5;s++)instDraw(game.leavesFar[s],farTrees[s]);
    // Distance cull ground cards; their alpha edges dominate fragment cost on Mali.
    std::vector<Instance> closeGrass;closeGrass.reserve(game.batches.grass.size());
    for(const auto& t:game.batches.grass){
        float dx=t.x-game.x,dz=t.z-game.z;
        if(dx*dx+dz*dz<grassMeters*grassMeters)closeGrass.push_back(t);
    }
    material(5,.42f,.8f);
    set1(game.instProg,"uInteractiveVegetation",1.f);
    instDraw(game.grass,closeGrass);
    set1(game.instProg,"uInteractiveVegetation",0.f);
    // Sub-canopy LOD: high detail near, compact atlas impostors in the mid range.
    const float shrubRange=game.graphicsQuality==0?20.f:(game.graphicsQuality==2?45.f:30.f);
    const float fernRange=game.graphicsQuality==0?22.f:(game.graphicsQuality==2?50.f:35.f);
    std::vector<Instance> nearShrub,midShrub,nearFern,nearSeedlings;
    nearShrub.reserve(120);midShrub.reserve(240);nearFern.reserve(150);
    auto closeVegetation=[&](const std::vector<Instance>& src,std::vector<Instance>& dst,float reach){
        for(const auto& obj:src){
            const float dx=obj.x-game.x,dz=obj.z-game.z;
            if(dx*dx+dz*dz<reach*reach)dst.push_back(obj);
        }
    };
    closeVegetation(game.batches.shrub,nearShrub,shrubRange);
    for(const auto& obj:game.batches.shrub){
        const float dx=obj.x-game.x,dz=obj.z-game.z,ds=dx*dx+dz*dz;
        if(ds>=shrubRange*shrubRange && ds<100.f*100.f)midShrub.push_back(obj);
    }
    closeVegetation(game.batches.fern,nearFern,fernRange);
    closeVegetation(game.batches.seedling,nearSeedlings,fernRange*.8f);
    material(3,0.f,.06f);instDraw(game.shrubBranches,nearShrub);
    material(11,.38f,.54f);instDraw(game.shrubLeaflets,nearShrub);
    material(11,.38f,.78f);
    set1(game.instProg,"uInteractiveVegetation",1.f);
    instDraw(game.fernFronds,nearFern);
    set1(game.instProg,"uInteractiveVegetation",0.f);
    material(11,.38f,.70f);instDraw(game.groundSeedlings,nearSeedlings);
    material(6,.36f,.50f);instDraw(game.shrub,midShrub);
    // Layer four: individual wood debris and mushrooms are true mesh details,
    // limited to near-field vegetation to avoid excessive fragment processing.
    std::vector<Instance> localTwigs,localFungi;
    const float microRange=game.graphicsQuality==0?12.f:
                           (game.graphicsQuality==2?30.f:22.f);
    closeVegetation(game.batches.twigs,localTwigs,microRange);
    closeVegetation(game.batches.fungi,localFungi,microRange*.83f);
    material(3,0.f,0.f);instDraw(game.dryTwigs,localTwigs);
    material(3,0.f,0.f);instDraw(game.fungiStems,localFungi);
    material(17,0.f,0.f);instDraw(game.fungiCaps,localFungi);
    // Near-field separate fallen leaves build actual layers on the forest floor.
    std::vector<Instance> nearLitter;
    nearLitter.reserve(500);
    const float litterRadius=game.graphicsQuality==2?49.f:
                            (game.graphicsQuality==0?19.f:32.f);
    closeVegetation(game.batches.litter,nearLitter,litterRadius);
    material(11,.33f,.05f);
    set1(game.instProg,"uLeafLitter",1.f);
    instDraw(game.leafLitter,nearLitter);
    set1(game.instProg,"uLeafLitter",0.f);
    std::vector<Instance> nearReeds,nearFlowers;
    closeVegetation(game.batches.reeds,nearReeds,game.graphicsQuality==2?73.f:47.f);
    closeVegetation(game.batches.flowers,nearFlowers,game.graphicsQuality==2?60.f:40.f);
    material(5,.38f,.48f);instDraw(game.reedsMesh,nearReeds);
    material(29,.02f,.34f);instDraw(game.flowersMesh,nearFlowers);
    material(17,0.f,0.f);instDraw(game.rock,game.batches.stones);
    // Distinct licensed CC0 photogrammetry meshes for stumps and fallen wood.
    // Three LODs selected per instance; old procedural mesh remains fallback
    // if the source download/conversion failed and assets were absent.
    std::array<std::vector<Instance>,3> stumpGroups,logGroups;
    std::vector<Instance> proceduralLogs;
    for(const Instance& object:game.batches.logs){
        const float dx=object.x-game.x,dz=object.z-game.z;
        const float d2=dx*dx+dz*dz;
        const int lod=d2<38.f*38.f?0:(d2<95.f*95.f?1:2);
        const bool stump=object.tint>.51f;
        const bool loaded=stump?game.scannedStumpReady[lod]:game.scannedDeadlogReady[lod];
        if(!loaded)proceduralLogs.push_back(object);
        else (stump?stumpGroups[lod]:logGroups[lod]).push_back(object);
    }
    material(3,0.f,0.f);
    for(int lod=0;lod<3;lod++){
        if(game.scannedStumpReady[lod])instDraw(game.scannedStump[lod],stumpGroups[lod]);
        if(game.scannedDeadlogReady[lod])instDraw(game.scannedDeadlog[lod],logGroups[lod]);
    }
    instDraw(game.fallenLog,proceduralLogs);
    // Autonomous wildlife drawn in three anatomical meshes and fur materials.
    std::array<std::vector<Instance>,3> wildlifeInstances;
    for(const ForestAnimal &a:game.wildlife){
        const float d=std::hypot(a.x-game.x,a.z-game.z);
        const float wildlifeRange=game.graphicsQuality==0?75.f:145.f;
        if(d>wildlifeRange)continue;
        const float bob=.023f*std::sin(a.phase);
        wildlifeInstances[a.species].push_back({a.x,height(a.x,a.z)+bob,a.z,
             a.species==0?1.f:(a.species==1?.92f:.94f),
             a.angle, .55f+.30f*std::sin(a.phase*.31f),0,0});
    }
    for(int species=0;species<3;species++){
        material(27,0.f,0.f);
        const float start=float(species)/3.f;
        glUniform4f(glGetUniformLocation(game.instProg,"uAtlasRect"),
                    start+.0006f,.001f,1.f/3.f-.0012f,.998f);
        instDraw(game.wildlifeMeshes[species],wildlifeInstances[species]);
    }
    if(game.collected<RELIC_COUNT){
        std::vector<Instance> floating;
        for(const Relic&r:game.relics)if(!r.picked){
            float y=height(r.x,r.z)+.35f+std::sin(game.t*1.8f+r.x)*.13f;
            floating.push_back({r.x,y,r.z,.9f,0,.8f,0,0});
        }
        material(2,0,0,2.1f,game.t*.75f);
        instDraw(game.crystal,floating);
    }
    // Soft contact shadows connect moving fauna feet with the ground.
    if(game.shadowProg){
        std::vector<Instance> animalShadows;
        animalShadows.reserve(game.wildlife.size());
        for(const ForestAnimal& animal:game.wildlife){
            const float distance=std::hypot(animal.x-game.x,animal.z-game.z);
            if(distance>80.f)continue;
            const float scale=animal.species==0?.52f:(animal.species==1?.46f:.34f);
            animalShadows.push_back({animal.x,height(animal.x,animal.z)+.043f,animal.z,
                  scale,0.f,0.f,0.f,0.f});
        }
        if(!animalShadows.empty()){
            glUseProgram(game.shadowProg);
            glUniformMatrix4fv(glGetUniformLocation(game.shadowProg,"uVP"),
                               1,GL_FALSE,vp.m);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            instDraw(game.shadow,animalShadows);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }
    // River is transparent; draw after opaque geometry with depth writes off.
    // Masking by the carved terrain avoids a floating blue overlay.
    if(game.riverProg){
        glUseProgram(game.riverProg);
        glUniformMatrix4fv(glGetUniformLocation(game.riverProg,"uVP"),
                           1,GL_FALSE,vp.m);
        set3(game.riverProg,"uEye",eye);
        set3(game.riverProg,"uFog",fog);
        set3(game.riverProg,"uSun",normalize({.62f,.87f,.33f}));
        set1(game.riverProg,"uTime",game.t);
        set1(game.riverProg,"uLight",light);
        bindSunShadow(game.riverProg,2);
        set1(game.riverProg,"uWaterQuality",float(game.graphicsQuality));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,game.tex[25]);
        glUniform1i(glGetUniformLocation(game.riverProg,"uWaterNormal"),0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,game.tex[26]);
        glUniform1i(glGetUniformLocation(game.riverProg,"uRiverStones"),1);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for(const auto& pair:game.chunks){
            const Mesh& water=pair.second.river;
            if(!water.count || !forestChunkVisible(vp,pair.first))continue;
            glBindVertexArray(water.vao);
            glDrawElements(GL_TRIANGLES,water.count,GL_UNSIGNED_INT,nullptr);
            countDraw(water.count);
        }
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    if(usePost)presentPostFrame(eye);
    const auto cpuEnd=std::chrono::steady_clock::now();
    debugMetrics.cpuSubmissionMs=std::chrono::duration<float,std::milli>(cpuEnd-cpuStart).count();
    const float alpha=.045f;
    const float instantaneousFps=dt>.001f?1.f/dt:0.f;
    if(instantaneousFps>0.f){
        debugMetrics.smoothFps=debugMetrics.smoothFps<=0.f?instantaneousFps:
                debugMetrics.smoothFps*(1-alpha)+instantaneousFps*alpha;
    }
    debugMetrics.smoothCpuMs=debugMetrics.smoothCpuMs<=0.f?debugMetrics.cpuSubmissionMs:
                debugMetrics.smoothCpuMs*(1-alpha)+debugMetrics.cpuSubmissionMs*alpha;
    debugMetrics.fps=instantaneousFps;
    if(++debugMetrics.frames%240==0)
        LOGE("METRICS fps=%.1f cpu_submission_ms=%.2f draws=%u tris=%llu texMiB=%.1f (GPU time unavailable)",
          debugMetrics.smoothFps,debugMetrics.smoothCpuMs,debugMetrics.drawCalls,
          (unsigned long long)debugMetrics.triangles,
          double(debugMetrics.textureBytes)/(1024.*1024.));
}
static void initialize(){
    // Called with a current, potentially new, GLES context.
    game.chunks.clear();game.batches.clear();game.batchesDirty=true;
    game.texBytes[0]=0;
    std::fill(std::begin(game.texBytes),std::end(game.texBytes),0u);
    debugMetrics.textureBytes=0;
    game.ready=false;
    game.wildlife.clear();
    for(int lod=0;lod<3;lod++){
        game.scannedStump[lod]=Mesh{};
        game.scannedDeadlog[lod]=Mesh{};
        game.scannedStumpReady[lod]=game.scannedDeadlogReady[lod]=false;
    }
    game.terrainProg=program(TERRAIN_VS,TERRAIN_FS);
    game.instProg=program(INST_VS,INST_FS);
    game.shadowProg=program(SHADOW_VS,SHADOW_FS);
    game.skyProg=program(SKY_VS,SKY_FS);
    game.postProg=program(SKY_VS,POST_FS);
    game.riverProg=program(RIVER_VS,RIVER_FS);
    game.postFbo=game.postColor=game.postDepth=0;
    game.postReady=false;
    game.treeDepthProg=program(TREE_DEPTH_VS,TREE_DEPTH_FS);
    game.treeDepthReady=false;
    game.treeDepthFbo=game.treeDepthTex=0;
    if(game.skyProg)glGenVertexArrays(1,&game.skyVao);
    if(!game.terrainProg||!game.instProg||!game.shadowProg){LOGE("GLES shader initialization failed");return;}
    glGenBuffers(1,&instanceBuffer);
    for(int species=0;species<5;species++){
        ForestTreeMesh model=forestBuildTree(species);
        game.woodHi[species]=makeMesh(model.woodHigh);
        game.woodMid[species]=makeMesh(model.woodMid);
        game.leavesHi[species]=makeMesh(model.foliageHigh);
        game.leavesMid[species]=makeMesh(model.foliageMid);
        game.leavesFar[species]=makeMesh(model.foliageFar);
        LOGE("species %d high: wood %u triangles, leaf %u triangles",
             species,unsigned(model.woodHigh.i.size()/3),unsigned(model.foliageHigh.i.size()/3));
    }
    ForestUnderstoryMesh undergrowth=buildForestUnderstory();
    game.shrubBranches=makeMesh(undergrowth.twig);
    game.shrubLeaflets=makeMesh(undergrowth.shrub);
    game.fernFronds=makeMesh(undergrowth.fern);
    game.groundSeedlings=makeMesh(undergrowth.broadGroundPlant);
    game.leafLitter=makeMesh(forestLeafLitterPatch());
    ForestFloorDetails floor=forestBuildFloorDetails();
    game.dryTwigs=makeMesh(floor.dryTwigs);
    game.fungiStems=makeMesh(floor.fungiStems);
    game.fungiCaps=makeMesh(floor.fungiCaps);
    game.reedsMesh=makeMesh(makeRiverReeds());
    game.flowersMesh=makeMesh(makeMeadowFlowers());
    game.wildlifeMeshes[0]=makeMesh(forestBuildDeer());
    game.wildlifeMeshes[1]=makeMesh(forestBuildBoar());
    game.wildlifeMeshes[2]=makeMesh(forestBuildFox());
    game.grass=makeMesh(makeCards(.8f,1.3f,2));
    game.shrub=makeMesh(makeCards(1.6f,1.5f,3));
    game.rock=makeMesh(forestRockMesh());game.fallenLog=makeMesh(forestFallenLog());
    game.shadow=makeMesh(makeShadow());game.crystal=makeMesh(makeCrystal());
    for(auto &t:game.tex)t=createTexture();
    game.windTex=createTexture();
    game.ready=true;
    glDisable(GL_CULL_FACE);
    LOGE("Engine ready: Graphics 10 god rays, canopy-aware 4-layer PCG, reactive plants, foliage transmission");
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeCreate(JNIEnv*,jclass){initialize();}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeDebugCamera(JNIEnv*,jclass,jint scene){
    // World-space viewpoints chosen for reproducible emulator screenshots.
    const struct Preset {float x,z,yaw,pitch;} scenes[4]={
        {0.f,0.f,.35f,-.08f}, {61.f,31.f,1.17f,-.04f},
        {-109.f,76.f,2.38f,-.10f}, {142.f,-112.f,-.71f,-.06f}
    };
    const int s=std::clamp(int(scene),0,3);
    game.x=scenes[s].x;game.z=scenes[s].z;
    game.yaw=scenes[s].yaw;game.pitch=scenes[s].pitch;
    game.feetOffset=0.f;game.verticalVelocity=0.f;
    game.batchesDirty=true;
    LOGE("CI_CAPTURE scene=%d x=%.1f z=%.1f yaw=%.2f",s,game.x,game.z,game.yaw);
}
static bool importScannedLOD(AAssetManager*manager,const char*filename,Mesh &target){
    AAsset* file=AAssetManager_open(manager,filename,AASSET_MODE_BUFFER);
    if(!file)return false;
    const int64_t length=AAsset_getLength64(file);
    if(length<128 || length>32*1024*1024){AAsset_close(file);return false;}
    std::vector<uint8_t> bytes(size_t(length),0);
    size_t pos=0;
    while(pos<bytes.size()){
        const int got=AAsset_read(file,bytes.data()+pos,
                                  int(std::min<size_t>(bytes.size()-pos,65536)));
        if(got<=0)break;
        pos+=size_t(got);
    }
    AAsset_close(file);
    if(pos!=bytes.size())return false;
    ModelData decoded;
    if(!forestDecodeScannedMesh(bytes.data(),bytes.size(),decoded))return false;
    target=makeMesh(decoded);
    LOGE("GLTF source imported: %s, %zu triangles",filename,decoded.i.size()/3);
    return target.count>0;
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeLoadScannedModels(
       JNIEnv*env,jclass,jobject manager){
    if(!game.ready || !manager)return;
    AAssetManager*assets=AAssetManager_fromJava(env,manager);
    if(!assets)return;
    for(int lod=0;lod<3;lod++){
        char name[80];
        std::snprintf(name,sizeof(name),"scanned_stump_lod%d.fvmesh",lod);
        game.scannedStumpReady[lod]=importScannedLOD(assets,name,game.scannedStump[lod]);
        std::snprintf(name,sizeof(name),"scanned_deadlog_lod%d.fvmesh",lod);
        game.scannedDeadlogReady[lod]=importScannedLOD(assets,name,game.scannedDeadlog[lod]);
    }
    LOGE("Scanned GLTF-derived model LODs ready: stump %d%d%d, log %d%d%d",
          int(game.scannedStumpReady[0]),int(game.scannedStumpReady[1]),int(game.scannedStumpReady[2]),
          int(game.scannedDeadlogReady[0]),int(game.scannedDeadlogReady[1]),int(game.scannedDeadlogReady[2]));
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeSetGraphicsQuality(JNIEnv*,jclass,jint value){
    game.graphicsQuality=std::max(0,std::min(3,int(value)));
}

// Vulkan compute pipeline is implemented in vulkan_wind.cpp.
// Runtime fallback is deterministic CPU noise so Vulkan is optional.
bool bakeVulkanWindField(AAssetManager* manager,std::vector<uint8_t>& rgba,std::string& gpu);
extern "C" JNIEXPORT jstring JNICALL
Java_com_florestaviva_game_MainActivity_nativeBakeWind(JNIEnv*env,jclass,jobject assetManager){
    if(!game.ready)return env->NewStringUTF("Engine not ready");
    AAssetManager* assets=AAssetManager_fromJava(env,assetManager);
    std::vector<uint8_t> bytes;
    std::string gpu;
    const bool vulkan=bakeVulkanWindField(assets,bytes,gpu);
    if(!vulkan){
        bytes.resize(128u*128u*4u);
        for(int y=0;y<128;y++)for(int x=0;x<128;x++){
            uint32_t seed=hash32(uint32_t(x)*174763u^uint32_t(y)*93217u);
            int p=(y*128+x)*4;
            bytes[p]=uint8_t((seed>>8)&255);
            bytes[p+1]=uint8_t((seed>>16)&255);
            bytes[p+2]=uint8_t(seed&255);
            bytes[p+3]=255;
        }
    }
    game.windVulkan=vulkan;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,game.windTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,bytes.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    LOGE("wind field: %s %s",vulkan?"Vulkan compute":"CPU fallback",gpu.c_str());
    return env->NewStringUTF(vulkan?"Vulkan compute GPU: campo de vento pronto":
                                      "CPU fallback: campo de vento pronto");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_florestaviva_game_MainActivity_nativeVulkanInfo(JNIEnv*env,jclass){
    // Runtime Vulkan capability check. The playable forest currently retains
    // its proven OpenGL ES renderer; no false claim of a Vulkan forest backend.
    VkApplicationInfo appInfo{};
    appInfo.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName="FlorestaViva";
    appInfo.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci{};
    ci.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo=&appInfo;
    VkInstance instance=VK_NULL_HANDLE;
    if(vkCreateInstance(&ci,nullptr,&instance)!=VK_SUCCESS)
        return env->NewStringUTF("Vulkan indisponivel; OpenGL ES 3 ativo");
    uint32_t count=0;
    if(vkEnumeratePhysicalDevices(instance,&count,nullptr)!=VK_SUCCESS||count==0){
        vkDestroyInstance(instance,nullptr);
        return env->NewStringUTF("Sem GPU Vulkan; OpenGL ES 3 ativo");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance,&count,devices.data());
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(devices[0],&props);
    char name[220]{};
    std::snprintf(name,sizeof(name),"Vulkan disponivel: %.130s (API %u.%u). Renderer atual: GLES3.",
                  props.deviceName,VK_VERSION_MAJOR(props.apiVersion),VK_VERSION_MINOR(props.apiVersion));
    vkDestroyInstance(instance,nullptr);
    return env->NewStringUTF(name);
}

extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeResize(JNIEnv*,jclass,jint w,jint h){
    game.width=std::max(int(w),1);game.height=std::max(int(h),1);
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeUploadTexture(JNIEnv*env,jclass,jint index,jobject bitmap){
    if(index<0||index>=39||!bitmap||!game.ready)return;
    AndroidBitmapInfo info{};
    if(AndroidBitmap_getInfo(env,bitmap,&info)!=ANDROID_BITMAP_RESULT_SUCCESS || info.format!=ANDROID_BITMAP_FORMAT_RGBA_8888)return;
    void* pixels=nullptr;
    if(AndroidBitmap_lockPixels(env,bitmap,&pixels)!=ANDROID_BITMAP_RESULT_SUCCESS)return;
    glBindTexture(GL_TEXTURE_2D,game.tex[index]);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(info.stride==info.width*4){
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,int(info.width),int(info.height),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    }else{
        // Bitmap row stride may include padding; repack before uploading.
        std::vector<uint8_t> packed(size_t(info.width)*info.height*4);
        for(uint32_t y=0;y<info.height;y++){
            auto* src=static_cast<const uint8_t*>(pixels)+size_t(y)*info.stride;
            std::copy(src,src+info.width*4,packed.begin()+size_t(y)*info.width*4);
        }
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,int(info.width),int(info.height),0,GL_RGBA,GL_UNSIGNED_BYTE,packed.data());
    }
    AndroidBitmap_unlockPixels(env,bitmap);
    glGenerateMipmap(GL_TEXTURE_2D);
    const uint64_t old=game.texBytes[index];
    const uint64_t estimate=uint64_t(info.width)*uint64_t(info.height)*4u*4u/3u;
    game.texBytes[index]=estimate;
    debugMetrics.textureBytes=debugMetrics.textureBytes-old+estimate;
    static bool checked=false, anisotropy=false;
    if(!checked){
        GLint extensionCount=0;
        glGetIntegerv(GL_NUM_EXTENSIONS,&extensionCount);
        for(GLint e=0;e<extensionCount;e++){
            const char* extension=reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,GLuint(e)));
            if(extension && std::strcmp(extension,"GL_EXT_texture_filter_anisotropic")==0){anisotropy=true;break;}
        }
        checked=true;
    }
    if(anisotropy)glTexParameterf(GL_TEXTURE_2D,0x84FE,4.0f);
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_florestaviva_game_MainActivity_nativeUploadKtx2Asset(
    JNIEnv*env,jclass,jobject assetManager,jint index,jstring filename){
    if(!game.ready||!assetManager||!filename||index<0||index>=39)return JNI_FALSE;
    AAssetManager*manager=AAssetManager_fromJava(env,assetManager);
    if(!manager)return JNI_FALSE;
    const char*name=env->GetStringUTFChars(filename,nullptr);
    if(!name)return JNI_FALSE;
    GLuint compressed=0;
    uint64_t resident=0;
    const bool ok=forest_assets::loadAstcKtx2(manager,name,compressed,resident);
    env->ReleaseStringUTFChars(filename,name);
    if(!ok)return JNI_FALSE;
    const GLuint previous=game.tex[index];
    game.tex[index]=compressed;
    if(previous)glDeleteTextures(1,&previous);
    debugMetrics.textureBytes=debugMetrics.textureBytes-game.texBytes[index]+resident;
    game.texBytes[index]=resident;
    LOGE("ASTC_KTX2 slot=%d residentKiB=%llu",int(index),
         (unsigned long long)((resident+1023)/1024));
    return JNI_TRUE;
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeInput(JNIEnv*,jclass,jfloat mx,jfloat mz,
  jfloat lx,jfloat ly,jboolean sprint,jboolean collect){
    game.moveX=clampf(mx,-1,1);game.moveZ=clampf(mz,-1,1);
    game.lookX+=clampf(lx,-150.f,150.f);game.lookY+=clampf(ly,-150.f,150.f);
    game.running=sprint;
    if(collect)game.collect=true;
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeRender(JNIEnv*,jclass,jfloat delta){draw(delta);}
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_florestaviva_game_MainActivity_nativeMetrics(JNIEnv*env,jclass){
    // GL render-thread only. No GPU sync; values are CPU-side counts/estimates.
    const float values[]={
        debugMetrics.smoothFps,debugMetrics.smoothCpuMs,
        float(debugMetrics.drawCalls),float(debugMetrics.triangles),
        float(double(debugMetrics.textureBytes)/(1024.*1024.))
    };
    jfloatArray result=env->NewFloatArray(5);
    if(result)env->SetFloatArrayRegion(result,0,5,values);
    return result;
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeJump(JNIEnv*,jclass){
    game.jumpQueued=true;
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_florestaviva_game_MainActivity_nativeStatus(JNIEnv*env,jclass){
    float minDist=99999.f;
    for(const Relic&r:game.relics)if(!r.picked)
        minDist=std::min(minDist,std::hypot(game.x-r.x,game.z-r.z));
    const int speciesSeen=__builtin_popcount(game.seenSpecies&7u);
    char buff[300]{};
    if(!game.reachedRiver){
        std::snprintf(buff,sizeof(buff),"MISSAO 1: Encontre o rio (%.0f m) | Vento: %s",
                      riverDistance(game.x,game.z),game.windVulkan?"Vulkan":"CPU");
    }else if(game.jumps<3){
        std::snprintf(buff,sizeof(buff),"MISSAO 2: Aprenda a saltar | %d/3 saltos | ARM64",
                      game.jumps);
    }else if(speciesSeen<3){
        std::snprintf(buff,sizeof(buff),"MISSAO 3: Observe cervo, javali e raposa | %d/3 especies",
                      speciesSeen);
    }else if(game.collected<RELIC_COUNT){
        std::snprintf(buff,sizeof(buff),"MISSAO 4: Fragmentos antigos | %d/%d | %.0f m",
                      game.collected,RELIC_COUNT,minDist);
    }else if(!game.survival.firstSurvivalGoal()){
        std::snprintf(buff,sizeof(buff),
            "MISSAO 5: Coletar madeira %d/2 pedra %d/2 plantas %d/1 | PEGA",
            game.survival.wood,game.survival.stone,game.survival.herbs);
    }else{
        std::snprintf(buff,sizeof(buff),"FLORESTA VIVA | 5/5 MISSOES CONCLUIDAS");
    }
    // HUD retains an at-a-glance survival readout even during missions.
    const size_t offset=std::strlen(buff);
    if(offset+46<sizeof(buff)){
        std::snprintf(buff+offset,sizeof(buff)-offset," | EN %.0f%% AG %.0f%%",
                      game.survival.stamina,game.survival.hydration);
    }
    return env->NewStringUTF(buff);
}
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_florestaviva_game_MainActivity_nativeMapData(JNIEnv*env,jclass){
    constexpr int RIVER_MAP_POINTS=21;
    constexpr int COUNT=3+RELIC_COUNT*2+RIVER_MAP_POINTS*2;
    float coords[COUNT]{};
    coords[0]=game.x;coords[1]=game.z;coords[2]=game.yaw;
    for(int i=0;i<RELIC_COUNT;i++){
        coords[3+2*i]=game.relics[i].picked?99999.f:game.relics[i].x;
        coords[4+2*i]=game.relics[i].picked?99999.f:game.relics[i].z;
    }
    for(int i=0;i<RIVER_MAP_POINTS;i++){
        const float z=game.z+(i-10)*12.f;
        coords[3+RELIC_COUNT*2+2*i]=riverCenter(z);
        coords[4+RELIC_COUNT*2+2*i]=z;
    }
    jfloatArray result=env->NewFloatArray(COUNT);
    if(result)env->SetFloatArrayRegion(result,0,COUNT,coords);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_florestaviva_game_MainActivity_nativeSave(JNIEnv*env,jclass){
    unsigned mask=0;
    for(int i=0;i<RELIC_COUNT;i++)if(game.relics[i].picked)mask|=1u<<i;
    char save[220];
    std::snprintf(save,sizeof(save),
                  "%.3f %.3f %.4f %.4f %.3f %u %u %u %d %d %d %d %.1f %.1f",
                  game.x,game.z,game.yaw,game.pitch,game.t,mask,
                  game.reachedRiver?1u:0u,game.seenSpecies,game.jumps,
                  game.survival.wood,game.survival.stone,game.survival.herbs,
                  game.survival.stamina,game.survival.hydration);
    return env->NewStringUTF(save);
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeLoad(JNIEnv*env,jclass,jstring save){
    if(!save)return;
    const char* raw=env->GetStringUTFChars(save,nullptr);
    if(!raw)return;
    float x,z,yaw,pitch,t;
    unsigned mask=0,river=0,seen=0;
    int jumps=0,wood=0,stone=0,herbs=0;
    float stamina=100.f,hydration=100.f;
    int got=std::sscanf(raw,"%f %f %f %f %f %u %u %u %d %d %d %d %f %f",
           &x,&z,&yaw,&pitch,&t,&mask,&river,&seen,&jumps,
           &wood,&stone,&herbs,&stamina,&hydration);
    env->ReleaseStringUTFChars(save,raw);
    if(got<6 || !std::isfinite(x)||!std::isfinite(z)||!std::isfinite(yaw)
       ||!std::isfinite(pitch)||!std::isfinite(t)||std::fabs(x)>WORLD_HALF-8
       ||std::fabs(z)>WORLD_HALF-8 || t<0)return;
    game.x=x;game.z=z;game.yaw=yaw;game.pitch=clampf(pitch,-1.35f,1.35f);
    game.t=t;game.collected=0;
    // Six-field Graphics 7 saves still load without losing collected relics.
    game.reachedRiver=(got>=7 && river!=0);
    game.seenSpecies=(got>=8 ? seen&7u : 0u);
    game.jumps=(got>=9 ? std::max(0,std::min(999,jumps)) : 0);
    game.survival.wood=got>=10?std::clamp(wood,0,9999):0;
    game.survival.stone=got>=11?std::clamp(stone,0,9999):0;
    game.survival.herbs=got>=12?std::clamp(herbs,0,9999):0;
    game.survival.stamina=got>=13&&std::isfinite(stamina)?
        clampf(stamina,0.f,100.f):100.f;
    game.survival.hydration=got>=14&&std::isfinite(hydration)?
        clampf(hydration,0.f,100.f):100.f;
    for(int i=0;i<RELIC_COUNT;i++){
        game.relics[i].picked=(mask&(1u<<i))!=0;
        if(game.relics[i].picked)game.collected++;
    }
}
