#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
static uint32_t hash32(uint32_t x){
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;
}
static float randCell(int x,int z){
    uint32_t a=hash32(uint32_t(x)*73856093u ^ uint32_t(z)*19349663u ^ 913547u);
    return float(a&0xffffffu)/float(0xffffffu);
}
static float noise(float x,float z){
    int ix=int(std::floor(x)),iz=int(std::floor(z)); float fx=x-ix,fz=z-iz;
    fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
    float a=randCell(ix,iz),b=randCell(ix+1,iz);
    float c=randCell(ix,iz+1),d=randCell(ix+1,iz+1);
    return (a+(b-a)*fx)*(1-fz)+(c+(d-c)*fx)*fz;
}
static float height(float x,float z){
    return 5.2f*(noise(x*.0048f,z*.0048f)-.5f)+
           1.8f*(noise(x*.020f,z*.020f)-.5f)+
           .55f*(noise(x*.065f,z*.065f)-.5f)+
           .7f*std::sin(x*.012f)*std::cos(z*.014f);
}
static Vec3 normalAt(float x,float z){
    return normalize({height(x-1,z)-height(x+1,z),2.f,height(x,z-1)-height(x,z+1)});
}
struct Vertex {float x,y,z,nx,ny,nz,u,v;};
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
static void cylinder(ModelData& m,Vec3 from,Vec3 to,float bottomRadius,float topRadius,int sectors){
    Vec3 axis=normalize(to-from);
    Vec3 tangent=normalize(cross(axis,std::fabs(axis.y)>.9f?Vec3{1,0,0}:Vec3{0,1,0}));
    Vec3 bit=cross(axis,tangent);
    uint32_t k=uint32_t(m.v.size());
    for(int j=0;j<=sectors;j++){
        float a=2*PI*float(j)/float(sectors);
        Vec3 radial=tangent*std::cos(a)+bit*std::sin(a);
        Vec3 p=from+radial*bottomRadius,q=to+radial*topRadius;
        m.v.push_back({p.x,p.y,p.z,radial.x,radial.y,radial.z,float(j)/sectors,1.f});
        m.v.push_back({q.x,q.y,q.z,radial.x,radial.y,radial.z,float(j)/sectors,0.f});
        if(j<sectors){uint32_t t=k+2*j; m.triangle(t,t+2,t+1);m.triangle(t+1,t+2,t+3);}
    }
}
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
static void cone(ModelData& m,float y0,float y1,float rad,int segments){
    uint32_t k=uint32_t(m.v.size());
    for(int j=0;j<=segments;j++){
        float t=float(j)/segments*2*PI,c=std::cos(t),s=std::sin(t);
        Vec3 n=normalize({c,rad/(y1-y0),s});
        m.v.push_back({c*rad,y0,s*rad,n.x,n.y,n.z,float(j)/segments*2,1});
        m.v.push_back({0,y1,0,n.x,n.y,n.z,float(j)/segments*2,0});
        if(j<segments){uint32_t a=k+j*2;m.triangle(a,a+2,a+1);}
    }
}
static ModelData makeTrunk(bool pine){
    ModelData m;
    cylinder(m,{0,0,0},{.08f,pine?7.1f:6.5f,0},pine?.27f:.35f,.07f,12);
    if(!pine) {
        for(int i=0;i<5;i++) {
            float a=2.f*PI*float(i)/5.f;
            Vec3 foot{.8f*std::cos(a),-.03f,.8f*std::sin(a)};
            cylinder(m,{0,.47f,0},foot,.16f,.035f,5);
        }
    }
    if(!pine){
        cylinder(m,{0,3.4f,0},{1.7f,5.9f,.55f},.14f,.03f,6);
        cylinder(m,{0,4.0f,0},{-1.4f,6.5f,-.7f},.13f,.025f,6);
        cylinder(m,{.05f,4.8f,0},{.9f,7.4f,-1.2f},.10f,.025f,5);
        cylinder(m,{-.02f,4.4f,0},{-1.0f,6.0f,1.4f},.12f,.025f,5);
        cylinder(m,{0,5.2f,.12f},{-.65f,7.5f,.98f},.08f,.018f,5);
        cylinder(m,{.08f,3.75f,-.03f},{1.05f,5.1f,-1.8f},.095f,.016f,5);
    }
    return m;
}
static ModelData makeBroadLeaves(){
    ModelData m;
    ellipsoid(m,{0,7.25f,0},{2.2f,1.9f,2.1f});
    ellipsoid(m,{1.35f,6.6f,.5f},{1.65f,1.5f,1.55f});
    ellipsoid(m,{-1.25f,6.6f,-.4f},{1.7f,1.45f,1.6f});
    ellipsoid(m,{.15f,6.5f,-1.3f},{1.55f,1.5f,1.65f});
    ellipsoid(m,{-.4f,7.65f,.95f},{1.5f,1.5f,1.35f});
    ellipsoid(m,{1.5f,5.95f,-.8f},{1.05f,.98f,1.1f},6,10);
    ellipsoid(m,{-1.8f,5.8f,1.f},{1.1f,.95f,1.05f},6,10);
    return m;
}
static ModelData makeLeafDetail(){
    // Alpha-tested leaf cards interrupt the spherical silhouette of deciduous canopies.
    ModelData m;
    std::mt19937 rng(123984u);
    std::uniform_real_distribution<float> random(0.f,1.f);
    for(int i=0;i<80;i++){
        float a=random(rng)*2*PI;
        float v=random(rng)*2.f-1.f;
        float radial=std::sqrt(1.f-v*v);
        float r=1.7f+random(rng)*.80f;
        Vec3 center{std::cos(a)*radial*r,7.0f+v*1.65f,std::sin(a)*radial*r};
        float sz=.45f+random(rng)*.66f;
        Vec3 side{std::cos(a+PI*.5f)*sz,0,std::sin(a+PI*.5f)*sz};
        Vec3 up{0,sz*1.3f,0};
        m.quad(center-side,center+side,center+side+up,center-side+up,
               normalize(cross(up,side)));
    }
    return m;
}
static ModelData makePineLeaves(){
    ModelData m;
    cone(m,2.4f,5.9f,2.25f,12);
    cone(m,4.0f,7.15f,1.8f,12);
    cone(m,5.45f,8.35f,1.23f,14);
    cone(m,6.65f,8.85f,.80f,14);
    return m;
}
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
static const char* TERRAIN_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
uniform mat4 uVP;
out vec3 vP;out vec3 vN;out vec2 vUV;
void main(){vP=aPos;vN=aNormal;vUV=aUV;gl_Position=uVP*vec4(aPos,1.0);}
)GLSL";
static const char* TERRAIN_FS=R"GLSL(#version 300 es
precision highp float;
in vec3 vP;in vec3 vN;in vec2 vUV;
uniform sampler2D uGrass,uDirt,uMoss,uGrassNormal,uDirtNormal,uMossNormal;
uniform vec3 uEye,uFog,uSun;
uniform float uLight;
out vec4 Frag;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float ggxSpec(vec3 N,vec3 V,vec3 L,float roughness){
    vec3 H=normalize(V+L);
    float nh=max(dot(N,H),0.0),nl=max(dot(N,L),0.01),nv=max(dot(N,V),0.01);
    float a=roughness*roughness,a2=a*a;
    float D=a2/(3.14159265*pow(nh*nh*(a2-1.0)+1.0,2.0)+.0001);
    float k=pow(roughness+1.0,2.0)*.125;
    float G=nl/(nl*(1.0-k)+k)*nv/(nv*(1.0-k)+k);
    float F=.04+.96*pow(1.0-max(dot(H,V),0.0),5.0);
    return D*G*F/(4.0*max(nl*nv,.05));
}
void main(){
    vec3 N=normalize(vN),L=normalize(uSun),V=normalize(uEye-vP);
    float slope=1.0-N.y;
    float grain=hash(floor(vP.xz*.125));
    float dirt=clamp(slope*2.6+smoothstep(.49,.83,grain)*.34,0.0,.80);
    float moss=clamp((1.0-dirt)*(.24+.25*sin(vP.x*.030+vP.z*.039)),0.0,.5);
    vec2 guv=vUV*.16,duv=vUV*.19,muv=vUV*.17;
    vec3 albedo=mix(texture(uGrass,guv).rgb,texture(uMoss,muv).rgb,moss);
    albedo=mix(albedo,texture(uDirt,duv).rgb,dirt);
    vec2 gn=texture(uGrassNormal,guv).rg*2.0-1.0;
    vec2 mn=texture(uMossNormal,muv).rg*2.0-1.0;
    vec2 dn=texture(uDirtNormal,duv).rg*2.0-1.0;
    vec2 mapXY=mix(mix(gn,mn,moss),dn,dirt);
    vec3 n=normalize(N+vec3(mapXY.x*.72,0.0,mapXY.y*.72));
    float diffuse=max(dot(n,L),0.0);
    float wet=clamp(moss*.4+dirt*.22,0.0,.34);
    float roughness=mix(.94,.69,wet);
    float highlight=ggxSpec(n,V,L,roughness)*diffuse*.30;
    vec3 rgb=albedo*(.28+.79*diffuse*uLight)+vec3(highlight*uLight);
    rgb*=1.0-.12*smoothstep(.12,.7,slope);
    rgb=rgb/(rgb+vec3(.34));
    rgb=pow(rgb,vec3(.92));
    float fog=smoothstep(92.0,276.0,distance(uEye,vP));
    Frag=vec4(mix(rgb,uFog,fog),1.0);
}
)GLSL";
static const char* INST_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aInst;
layout(location=4) in vec4 aMeta;
uniform mat4 uVP;uniform float uTime,uWind,uSpin;
out vec3 vN,vP;out vec2 vUV;out float vTint,vType;
void main(){
    float ang=aMeta.x+uSpin,ca=cos(ang),sa=sin(ang);
    vec3 p=aPos*aInst.w;
    if(uWind>0.0){float sway=sin(uTime*1.25+aInst.x*.12+aInst.z*.12+aPos.y*.38);
        p.x+=uWind*sway*max(aPos.y,0.0)*.035;
    }
    vec3 r=vec3(p.x*ca-p.z*sa,p.y,p.x*sa+p.z*ca);
    vP=r+aInst.xyz;
    vN=normalize(vec3(aNormal.x*ca-aNormal.z*sa,aNormal.y,aNormal.x*sa+aNormal.z*ca));
    vUV=aUV;vTint=aMeta.y;vType=aMeta.z;
    gl_Position=uVP*vec4(vP,1.0);
}
)GLSL";
static const char* INST_FS=R"GLSL(#version 300 es
precision highp float;
in vec3 vN,vP;in vec2 vUV;in float vTint,vType;
uniform sampler2D uTex,uMaterialNormal;
uniform vec3 uEye,uFog,uSun;
uniform float uLight,uCutout,uEmissive,uRoughness,uBump,uFoliage;
out vec4 Frag;
float specGGX(vec3 N,vec3 V,vec3 L,float roughness){
    vec3 H=normalize(V+L);
    float nh=max(dot(N,H),0.0),nv=max(dot(N,V),0.01),nl=max(dot(N,L),0.01);
    float a=roughness*roughness,a2=a*a;
    float d=a2/(3.14159265*pow(nh*nh*(a2-1.0)+1.0,2.0)+.0001);
    float k=pow(roughness+1.0,2.0)*.125;
    float g=nl/(nl*(1.0-k)+k)*nv/(nv*(1.0-k)+k);
    float f=.04+.96*pow(1.0-max(dot(H,V),0.0),5.0);
    return d*g*f/(4.0*max(nl*nv,.05));
}
void main(){
    vec4 s=texture(uTex,vUV);
    if(s.a<uCutout)discard;
    vec3 n=normalize(vN),V=normalize(uEye-vP),L=normalize(uSun);
    if(uBump>.01){
        vec3 q0=dFdx(vP),q1=dFdy(vP);
        vec2 st0=dFdx(vUV),st1=dFdy(vUV);
        float det=st0.x*st1.y-st0.y*st1.x;
        if(abs(det)>.00001){
            vec3 T=normalize((q0*st1.y-q1*st0.y)/det);
            vec3 B=normalize((-q0*st1.x+q1*st0.x)/det);
            vec3 map=texture(uMaterialNormal,vUV).rgb*2.0-1.0;
            n=normalize(n+uBump*(T*map.x+B*map.y));
        }
    }
    float nl=dot(n,L);
    float diff=uFoliage>.5 ? (.43+.57*max(nl*.7+.30,0.0)) : (.26+.78*max(nl,0.0)*uLight);
    vec3 rgb=s.rgb*diff*(.82+vTint*.33);
    if(uFoliage>.5){
        float transmission=pow(max(-nl,0.0),1.7)*.23*uLight;
        rgb+=s.rgb*vec3(.38,.60,.18)*transmission;
    }else{
        rgb+=vec3(specGGX(n,V,L,uRoughness)*max(nl,0.0)*uLight*.46);
    }
    if(vType>.5)rgb*=vec3(.84,1.04,.86);
    rgb+=uEmissive*vec3(.10,.44,.65);
    rgb=rgb/(rgb+vec3(.32));
    rgb=pow(rgb,vec3(.94));
    float fog=smoothstep(92.0,276.0,distance(uEye,vP));
    Frag=vec4(mix(rgb,uFog,fog),s.a);
}
)GLSL";
static const char* SHADOW_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aInst;
uniform mat4 uVP;
out vec2 vUV;
void main(){vUV=aUV;gl_Position=uVP*vec4(aPos*aInst.w+aInst.xyz,1.0);}
)GLSL";
static const char* SHADOW_FS=R"GLSL(#version 300 es
precision mediump float;
in vec2 vUV;out vec4 Frag;
void main(){float x=length(vUV-vec2(.5))*2.0;float a=(1.0-smoothstep(.16,.95,x))*.23;Frag=vec4(.015,.025,.014,a);}
)GLSL";

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
    glBindBuffer(GL_ARRAY_BUFFER,instanceBuffer);
    glEnableVertexAttribArray(3);glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Instance),(void*)0);
    glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(Instance),(void*)(4*sizeof(float)));
    glVertexAttribDivisor(3,1);glVertexAttribDivisor(4,1);
    glBindVertexArray(0);m.count=GLsizei(data.i.size());return m;
}
struct ChunkData {
    Mesh terrain;
    std::vector<Instance> broad,pine,grass,shrub,stones,shadow;
};
struct Batches {
    std::vector<Instance> broad,pine,grass,shrub,stones,shadow;
    void clear(){broad.clear();pine.clear();grass.clear();shrub.clear();stones.clear();shadow.clear();}
};
struct Relic {float x,z;bool picked=false;};
struct State {
    int width=1280,height=720;
    GLuint terrainProg=0,instProg=0,shadowProg=0;
    GLuint tex[11]{};
    Mesh trunk,pineTrunk,leaves,leafDetail,pineLeaves,grass,shrub,rock,shadow,crystal;
    std::unordered_map<int64_t,ChunkData> chunks;
    Batches batches;
    bool batchesDirty=true;
    float x=0,z=0,yaw=.35f,pitch=-.06f,t=0;
    float moveX=0,moveZ=0,lookX=0,lookY=0;
    bool running=false,collect=false;
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
static void freeChunk(ChunkData &c){freeMesh(c.terrain);}
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
    std::mt19937 rng(hash32(uint32_t(cx)*12582917u ^ uint32_t(cz)*4256249u ^ 0x8af553u));
    std::uniform_real_distribution<float> unit(0,1);
    auto samplePos=[&]()->Vec3{
        float x=cx*CHUNK+3.f+unit(rng)*(CHUNK-6.f);
        float z=cz*CHUNK+3.f+unit(rng)*(CHUNK-6.f);
        return {x,height(x,z),z};
    };
    const float forestDensity=.65f+noise((cx+.5f)*.57f,(cz+.5f)*.57f)*.72f;
    const int treeCount=int(12.f*forestDensity);
    for(int i=0;i<treeCount;i++){
        Vec3 p=samplePos();
        if(std::hypot(p.x,p.z)<8.f)continue;
        float scale=.67f+unit(rng)*.79f,angle=unit(rng)*6.283f,tint=unit(rng);
        Instance ins{p.x,p.y,p.z,scale,angle,tint,0,0};
        if(unit(rng)<.41f){ins.type=1;out.pine.push_back(ins);}else out.broad.push_back(ins);
        out.shadow.push_back({p.x,p.y+.06f,p.z,scale*1.65f,0,tint,0,0});
    }
    const int undergrowthCount=int(200.f*forestDensity);
    for(int i=0;i<undergrowthCount;i++){
        Vec3 p=samplePos();float scale=.55f+unit(rng)*1.10f;
        Instance ins{p.x,p.y+.025f,p.z,scale,unit(rng)*6.283f,unit(rng),0,0};
        if(i%19==0)out.shrub.push_back({p.x,p.y,p.z,scale*2.f,ins.rotation,ins.tint,0,0});
        else out.grass.push_back(ins);
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
        append(b.shrub,c.shrub);append(b.stones,c.stones);append(b.shadow,c.shadow);
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
    glBindVertexArray(0);
}
static void material(int index,float cutout,float wind,float emissive=0.f,float spin=0.f){
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,game.tex[index]);
    glUniform1i(glGetUniformLocation(game.instProg,"uTex"),0);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,game.tex[10]);
    glUniform1i(glGetUniformLocation(game.instProg,"uMaterialNormal"),1);
    set1(game.instProg,"uCutout",cutout);
    set1(game.instProg,"uWind",wind);
    set1(game.instProg,"uEmissive",emissive);
    set1(game.instProg,"uSpin",spin);
    set1(game.instProg,"uRoughness",index==3?.83f:.94f);
    set1(game.instProg,"uBump",index==3?.63f:0.f);
    set1(game.instProg,"uFoliage",(index==4||index==5||index==6)?1.f:0.f);
}
static void update(float dt){
    dt=clampf(dt,0.f,.05f);game.t+=dt;
    game.yaw-=game.lookX*.0034f;game.pitch=clampf(game.pitch-game.lookY*.003f,-1.35f,1.35f);
    game.lookX=game.lookY=0;
    Vec3 f{std::sin(game.yaw),0,std::cos(game.yaw)},rt{-f.z,0,f.x};
    Vec3 delta=f*game.moveZ+rt*game.moveX;
    if(dot(delta,delta)>1.f)delta=normalize(delta);
    float speed=(game.running?7.5f:4.2f)*dt;
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
        for(auto &r:game.relics){
            if(r.picked)continue;
            if(std::hypot(game.x-r.x,game.z-r.z)<3.6f){r.picked=true;game.collected++;break;}
        }
    }
    game.collect=false;
}
static void draw(float dt){
    if(!game.ready)return;
    update(dt);streamWorld();
    glViewport(0,0,game.width,game.height);
    // Soft daytime colors, with a slowly cycling evening phase.
    float cycle=.5f+.5f*std::sin(game.t*.0105f+.85f);
    float light=.59f+.41f*cycle;
    Vec3 fog={.56f+.16f*cycle,.65f+.17f*cycle,.68f+.20f*cycle};
    glClearColor(fog.x,fog.y,fog.z,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    Vec3 eye{game.x,height(game.x,game.z)+1.72f,game.z};
    Vec3 dir{std::sin(game.yaw)*std::cos(game.pitch),std::sin(game.pitch),std::cos(game.yaw)*std::cos(game.pitch)};
    Mat4 view=lookAt(eye,eye+dir);
    Mat4 proj=perspective(68.f*PI/180.f,float(game.width)/std::max(game.height,1),.11f,310.f);
    Mat4 vp=multiply(proj,view);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_BLEND);
    common(game.terrainProg,vp,eye,fog,light);
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
    for(const auto& pair:game.chunks){const Mesh& m=pair.second.terrain;
        glBindVertexArray(m.vao);glDrawElements(GL_TRIANGLES,m.count,GL_UNSIGNED_INT,nullptr);
    }
    glBindVertexArray(0);
    glUseProgram(game.shadowProg);
    glUniformMatrix4fv(glGetUniformLocation(game.shadowProg,"uVP"),1,GL_FALSE,vp.m);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);instDraw(game.shadow,game.batches.shadow);glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    common(game.instProg,vp,eye,fog,light);
    set1(game.instProg,"uTime",game.t);
    material(3,0,0);instDraw(game.trunk,game.batches.broad);
    material(3,0,0);instDraw(game.pineTrunk,game.batches.pine);
    material(4,0,.7f);instDraw(game.leaves,game.batches.broad);
    material(4,0,.55f);instDraw(game.pineLeaves,game.batches.pine);
    material(6,.36f,.85f);instDraw(game.leafDetail,game.batches.broad);
    material(5,.42f,.8f);instDraw(game.grass,game.batches.grass);
    material(6,.36f,.5f);instDraw(game.shrub,game.batches.shrub);
    material(1,0,0);instDraw(game.rock,game.batches.stones);
    if(game.collected<RELIC_COUNT){
        std::vector<Instance> floating;
        for(const Relic&r:game.relics)if(!r.picked){
            float y=height(r.x,r.z)+.35f+std::sin(game.t*1.8f+r.x)*.13f;
            floating.push_back({r.x,y,r.z,.9f,0,.8f,0,0});
        }
        material(2,0,0,2.1f,game.t*.75f);
        instDraw(game.crystal,floating);
    }
}
static void initialize(){
    // Called with a current, potentially new, GLES context.
    game.chunks.clear();game.batches.clear();game.batchesDirty=true;
    game.ready=false;
    game.terrainProg=program(TERRAIN_VS,TERRAIN_FS);
    game.instProg=program(INST_VS,INST_FS);
    game.shadowProg=program(SHADOW_VS,SHADOW_FS);
    if(!game.terrainProg||!game.instProg||!game.shadowProg){LOGE("GLES shader initialization failed");return;}
    glGenBuffers(1,&instanceBuffer);
    game.trunk=makeMesh(makeTrunk(false));game.pineTrunk=makeMesh(makeTrunk(true));
    game.leaves=makeMesh(makeBroadLeaves());game.leafDetail=makeMesh(makeLeafDetail());
    game.pineLeaves=makeMesh(makePineLeaves());
    game.grass=makeMesh(makeCards(.8f,1.3f,2));
    game.shrub=makeMesh(makeCards(1.6f,1.5f,3));
    ModelData stone;ellipsoid(stone,{0,0,0},{.8f,.42f,.65f},6,8);game.rock=makeMesh(stone);
    game.shadow=makeMesh(makeShadow());game.crystal=makeMesh(makeCrystal());
    for(auto &t:game.tex)t=createTexture();
    game.ready=true;
    glDisable(GL_CULL_FACE);
    LOGE("Engine ready: GLES3 / 2K world / normal-mapped materials");
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeCreate(JNIEnv*,jclass){initialize();}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeResize(JNIEnv*,jclass,jint w,jint h){
    game.width=std::max(int(w),1);game.height=std::max(int(h),1);
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeUploadTexture(JNIEnv*env,jclass,jint index,jobject bitmap){
    if(index<0||index>=11||!bitmap||!game.ready)return;
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
extern "C" JNIEXPORT jstring JNICALL
Java_com_florestaviva_game_MainActivity_nativeStatus(JNIEnv*env,jclass){
    float minDist=std::numeric_limits<float>::max();
    for(const Relic&r:game.relics)if(!r.picked)minDist=std::min(minDist,std::hypot(game.x-r.x,game.z-r.z));
    char buff[240];
    if(game.collected==RELIC_COUNT)std::snprintf(buff,sizeof(buff),"FLORESTA VIVA   |   %d/%d fragmentos   |   MISSAO CONCLUIDA!",game.collected,RELIC_COUNT);
    else std::snprintf(buff,sizeof(buff),"FLORESTA VIVA   |   Fragmentos: %d/%d   |   Proximo: %.0f m",game.collected,RELIC_COUNT,minDist);
    return env->NewStringUTF(buff);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_florestaviva_game_MainActivity_nativeMapData(JNIEnv*env,jclass){
    float coords[3+RELIC_COUNT*2];
    coords[0]=game.x;coords[1]=game.z;coords[2]=game.yaw;
    for(int i=0;i<RELIC_COUNT;i++){
        coords[3+2*i]=game.relics[i].picked?99999.f:game.relics[i].x;
        coords[4+2*i]=game.relics[i].picked?99999.f:game.relics[i].z;
    }
    jfloatArray result=env->NewFloatArray(3+RELIC_COUNT*2);
    if(result)env->SetFloatArrayRegion(result,0,3+RELIC_COUNT*2,coords);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_florestaviva_game_MainActivity_nativeSave(JNIEnv*env,jclass){
    unsigned mask=0;
    for(int i=0;i<RELIC_COUNT;i++)if(game.relics[i].picked)mask|=1u<<i;
    char save[180];
    std::snprintf(save,sizeof(save),"%.3f %.3f %.4f %.4f %.3f %u",
                  game.x,game.z,game.yaw,game.pitch,game.t,mask);
    return env->NewStringUTF(save);
}
extern "C" JNIEXPORT void JNICALL
Java_com_florestaviva_game_MainActivity_nativeLoad(JNIEnv*env,jclass,jstring save){
    if(!save)return;
    const char* raw=env->GetStringUTFChars(save,nullptr);
    if(!raw)return;
    float x,z,yaw,pitch,t;unsigned mask;
    int got=std::sscanf(raw,"%f %f %f %f %f %u",&x,&z,&yaw,&pitch,&t,&mask);
    env->ReleaseStringUTFChars(save,raw);
    if(got!=6 || !std::isfinite(x)||!std::isfinite(z)||!std::isfinite(yaw)
       ||!std::isfinite(pitch)||!std::isfinite(t)||std::fabs(x)>WORLD_HALF-8
       ||std::fabs(z)>WORLD_HALF-8 || t<0)return;
    game.x=x;game.z=z;game.yaw=yaw;game.pitch=clampf(pitch,-1.35f,1.35f);
    game.t=t;game.collected=0;
    for(int i=0;i<RELIC_COUNT;i++){
        game.relics[i].picked=(mask&(1u<<i))!=0;
        if(game.relics[i].picked)game.collected++;
    }
}
