#include "renderer/shaders.hpp"
// GLES 3.0 renderer shader sources; linked separately from JNI/gameplay.
// Exact shader source preserved during this refactor for visual parity.
const char* const TERRAIN_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
uniform mat4 uVP;
out vec3 vP;out vec3 vN;out vec2 vUV;
void main(){vP=aPos;vN=aNormal;vUV=aUV;gl_Position=uVP*vec4(aPos,1.0);}
)GLSL";

const char* const TERRAIN_FS=R"GLSL(#version 300 es
precision highp float;
uniform float uLegacyShading;
precision highp sampler2DShadow;
in vec3 vP;in vec3 vN;in vec2 vUV;
uniform sampler2D uGrass,uDirt,uMoss,uGrassNormal,uDirtNormal,uMossNormal;
uniform sampler2D uGrassRough,uDirtRough,uMossRough,uRiverPebbles,uRiverNormal;
uniform vec3 uEye,uFog,uSun;
uniform float uLight;
uniform sampler2DShadow uSunShadow;
uniform mat4 uSunShadowVP;
uniform float uShadowsEnabled,uShadowTexel,uShadowQuality;
out vec4 Frag;
float visibleSun(vec3 world,vec3 normal){
    if(uShadowsEnabled<.5)return 1.0;
    vec4 clip=uSunShadowVP*vec4(world,1.0);
    vec3 pos=clip.xyz/max(clip.w,.0001)*.5+.5;
    if(pos.x<=0.0||pos.x>=1.0||pos.y<=0.0||pos.y>=1.0||pos.z<=0.0||pos.z>=1.0)return 1.0;
    float bias=max(.0005,(1.0-max(dot(normal,normalize(uSun)),0.0))*.0019);
    float sum=0.0;
    if(uShadowQuality>1.5){
        for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
            vec2 off=vec2(float(x),float(y))*uShadowTexel*1.7;
            sum+=texture(uSunShadow,vec3(pos.xy+off,pos.z-bias));
        }
        return sum/9.0;
    }
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        vec2 off=(vec2(float(x),float(y))-.5)*uShadowTexel*1.7;
        sum+=texture(uSunShadow,vec3(pos.xy+off,pos.z-bias));
    }
    return sum*.25;
}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise2(vec2 p){
    vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
    float a=mix(hash(i),hash(i+vec2(1,0)),f.x);
    float b=mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x);
    return mix(a,b,f.y);
}
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
    // Continuous, multi-scale terrain macro noise removes obvious square tiles.
    float biome=noise2(vP.xz*.044)*.67+noise2(vP.xz*.15)*.33;
    float dirt=clamp(slope*2.35+smoothstep(.48,.78,biome)*.43,0.0,.82);
    float damp=noise2(vP.xz*.027+vec2(18.3,6.2));
    float moss=clamp((1.0-dirt)*smoothstep(.29,.82,damp)*.56,0.0,.54);
    vec2 guv=vUV*.16,duv=vUV*.19,muv=vUV*.17;
    vec3 groundColor=texture(uGrass,guv).rgb;
    // Subtle second UV frequency helps hide repeated photographic motifs.
    vec3 groundVariation=texture(uGrass,guv*.47+vec2(12.4,21.8)).rgb;
    groundColor=mix(groundColor,groundVariation,.16+.10*damp);
    vec3 albedo=mix(groundColor,texture(uMoss,muv).rgb,moss);
    albedo=mix(albedo,texture(uDirt,duv).rgb,dirt);
    // Scanned river-pebble albedo near water. Uses the SAME projected
    // distance metric as the CPU's carved banks to prevent water/land gaps.
    float riverSlope=78.0*.0067*cos(vP.z*.0067)+24.0*.020*cos(vP.z*.020+.47);
    float riverCenter=38.0+78.0*sin(vP.z*.0067)+24.0*sin(vP.z*.020+.47);
    float riverWidth=9.2+2.7*sin(vP.z*.013+1.1)+1.1*sin(vP.z*.043);
    float shoreDistance=abs(vP.x-riverCenter)/sqrt(1.+riverSlope*riverSlope);
    float wetShore=1.-smoothstep(riverWidth+2.5,riverWidth+10.,shoreDistance);
    vec2 riverUV=vP.xz*.24;
    albedo=mix(albedo,texture(uRiverPebbles,riverUV).rgb,wetShore*.83);
    // Legacy fallback uses albedo-only Lambertian light (not old source images).
    if(uLegacyShading>.5){
        float diffuse=max(dot(normalize(vN),L),0.);
        vec3 legacy=albedo*(.29+.71*diffuse*uLight);
        float distFog=smoothstep(92.,276.,distance(uEye,vP));
        Frag=vec4(mix(legacy,uFog,distFog),1.0);
        return;
    }
    vec2 gn=texture(uGrassNormal,guv).rg*2.0-1.0;
    vec2 mn=texture(uMossNormal,muv).rg*2.0-1.0;
    vec2 dn=texture(uDirtNormal,duv).rg*2.0-1.0;
    vec2 mapXY=mix(mix(gn,mn,moss),dn,dirt);
    vec2 pebbleXY=texture(uRiverNormal,riverUV).rg*2.0-1.0;
    mapXY=mix(mapXY,pebbleXY,wetShore*.68);
    vec3 n=normalize(N+vec3(mapXY.x*.72,0.0,mapXY.y*.72));
    float diffuse=max(dot(n,L),0.0);
    float wet=clamp(moss*.4+dirt*.22,0.0,.34);
    float measuredRough=mix(texture(uGrassRough,guv).g,
        texture(uDirtRough,duv).g,dirt);
    float photographedMossRough=texture(uMossRough,muv).g;
    float photoOcclusion=mix(texture(uGrassRough,guv).r,
                            texture(uMossRough,muv).r,moss);
    photoOcclusion=mix(photoOcclusion,texture(uDirtRough,duv).r,dirt);
    float roughness=clamp(mix(measuredRough,photographedMossRough,moss*.84)-wet*.11,.34,.97);
    roughness=mix(roughness,.67,wetShore*.82);
    float highlight=ggxSpec(n,V,L,roughness)*diffuse*.30;
    float sunVisibility=visibleSun(vP,n);
    // Outdoor skylight hemisphere in the raster pass: cool upper hemisphere
    // and warm ground bounce instead of fixed grey ambient everywhere.
    vec3 ambientHemisphere=mix(vec3(.22,.21,.17),vec3(.29,.34,.37),
                             clamp(n.y*.5+.5,0.,1.));
    vec3 rgb=albedo*(ambientHemisphere+.79*diffuse*uLight*(.23+.77*sunVisibility))+
             vec3(highlight*uLight*sunVisibility);
    rgb*=mix(.87,1.09,noise2(vP.xz*.013+vec2(6.1,12.4)));
    rgb*=mix(.72,1.,photoOcclusion);
    rgb*=1.0-.12*smoothstep(.12,.7,slope);
    // Filmic, channel-wise highlight compression instead of harsh clip.
    rgb=(rgb*(2.51*rgb+.03))/(rgb*(2.43*rgb+.59)+.14);
    rgb=pow(max(rgb,vec3(0.0)),vec3(.91));
    float fog=smoothstep(92.0,276.0,distance(uEye,vP));
    Frag=vec4(mix(rgb,uFog,fog),1.0);
}
)GLSL";
