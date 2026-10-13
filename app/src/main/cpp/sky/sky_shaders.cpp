#include "renderer/shaders.hpp"
// GLES 3.0 renderer shader sources; linked separately from JNI/gameplay.
// Exact shader source preserved during this refactor for visual parity.
const char* const SKY_VS=R"GLSL(#version 300 es
precision highp float;
void main(){
    vec2 p=vec2(gl_VertexID==1?3.0:-1.0,gl_VertexID==2?3.0:-1.0);
    gl_Position=vec4(p,0.0,1.0);
}
)GLSL";

const char* const SKY_FS=R"GLSL(#version 300 es
precision highp float;
uniform vec2 uViewport,uCamera;
uniform float uTime,uSunPower;
uniform vec3 uSunDir,uHaze;
out vec4 Frag;
float hashSky(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noiseSky(vec2 p){
    vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(hashSky(i),hashSky(i+vec2(1,0)),f.x),
        mix(hashSky(i+vec2(0,1)),hashSky(i+vec2(1,1)),f.x),f.y);
}
void main(){
    float aspect=uViewport.x/max(uViewport.y,1.0);
    vec2 screen=(gl_FragCoord.xy/uViewport)*2.0-1.0;
    float yaw=uCamera.x,pitch=uCamera.y;
    vec3 fw=vec3(sin(yaw)*cos(pitch),sin(pitch),cos(yaw)*cos(pitch));
    vec3 right=normalize(cross(fw,vec3(0,1,0)));
    vec3 up=normalize(cross(right,fw));
    float tanFov=.6745;
    vec3 ray=normalize(fw+screen.x*aspect*tanFov*right+screen.y*tanFov*up);
    float altitude=max(ray.y,0.0);
    float horizon=pow(1.0-altitude,5.0);
    vec3 zenith=mix(vec3(.085,.24,.45),vec3(.18,.39,.63),uSunPower);
    vec3 sky=mix(zenith,mix(uHaze,vec3(.70,.77,.78),uSunPower*.40),horizon);
    float cosSun=max(dot(ray,normalize(uSunDir)),0.0);
    float mie=pow(cosSun,42.0)*.32+pow(cosSun,360.0)*.60;
    sky+=vec3(1.0,.72,.38)*mie*uSunPower;
    // Clouds are a thin moving layer with two procedural detail scales.
    if(ray.y>.08){
        vec2 cloudXY=ray.xz/max(ray.y,.09)*.64+vec2(uTime*.0035,uTime*.0007);
        float broad=noiseSky(cloudXY);
        float wisps=noiseSky(cloudXY*2.4+13.1);
        float clouds=smoothstep(.53,.76,broad*.72+wisps*.28);
        clouds*=smoothstep(.08,.20,ray.y)*.44;
        sky=mix(sky,vec3(.93,.93,.90)*(.70+.3*uSunPower),clouds);
    }
    // Short broad solar halo and high-dynamic-range inspired sunlight.
    float disk=smoothstep(.99935,.99978,cosSun)*uSunPower;
    sky+=vec3(1.0,.83,.53)*disk*.95;
    Frag=vec4(sky,1.0);
}
)GLSL";
