#include "renderer/shaders.hpp"
// GLES 3.0 renderer shader sources; linked separately from JNI/gameplay.
// Exact shader source preserved during this refactor for visual parity.
const char* const POST_FS=R"GLSL(#version 300 es
precision highp float;
precision highp sampler2DShadow;
uniform sampler2D uColor;
uniform sampler2D uDepth;
uniform vec2 uResolution;
uniform float uQuality;
uniform sampler2DShadow uSunShadow;
uniform mat4 uSunShadowVP;
uniform vec3 uEye,uSunDirection;
uniform vec2 uCamera;
uniform float uGodRaysEnabled;
out vec4 Frag;
float sampleSun(vec3 world){
    vec4 clip=uSunShadowVP*vec4(world,1.);
    vec3 q=clip.xyz/max(clip.w,.0001)*.5+.5;
    if(any(lessThanEqual(q,vec3(0.))) ||
       any(greaterThanEqual(q,vec3(1.))))return 1.;
    return texture(uSunShadow,vec3(q.xy,q.z-.0017));
}
float lum(vec3 c){return dot(c,vec3(.299,.587,.114));}
float linearDepth(float d){
    const float nearPlane=.11;
    const float farPlane=310.0;
    return 2.0*nearPlane*farPlane/
        (farPlane+nearPlane-(2.0*d-1.0)*(farPlane-nearPlane));
}
void main(){
    vec2 uv=gl_FragCoord.xy/uResolution;
    vec2 t=1.0/uResolution;
    vec3 current=texture(uColor,uv).rgb;
    vec3 north=texture(uColor,uv+vec2(0,t.y)).rgb;
    vec3 south=texture(uColor,uv-vec2(0,t.y)).rgb;
    vec3 east=texture(uColor,uv+vec2(t.x,0)).rgb;
    vec3 west=texture(uColor,uv-vec2(t.x,0)).rgb;
    float center=lum(current),ln=lum(north),ls=lum(south),
          le=lum(east),lw=lum(west);
    // Mobile FXAA-style edge blending, no temporal ghost trails on moving leaves.
    float variation=max(max(ln,ls),max(le,lw))-
                    min(min(ln,ls),min(le,lw));
    float edge=smoothstep(.045,.21,variation);
    vec3 horizontal=(east+west)*.5,vertical=(north+south)*.5;
    vec3 smoothColor=abs(ln-ls)>abs(le-lw)?horizontal:vertical;
    vec3 rgb=mix(current,smoothColor,edge*.46);
    // Short-radius screen-space contact shadow makes overlapping roots,
    // rocks and understory read as connected layers rather than flat cutouts.
    float depth0=texture(uDepth,uv).r;
    if(uQuality>.5&&depth0<.9998){
        float sceneDistance=linearDepth(depth0);
        if(sceneDistance<44.0){
            float occlusion=0.0;
            for(int i=0;i<8;i++){
                float a=float(i)*.785398163;
                float radius=(i%2==0?2.0:3.75);
                vec2 off=vec2(cos(a),sin(a))*radius*t;
                float ds=linearDepth(texture(uDepth,clamp(uv+off,t,1.0-t)).r);
                float difference=sceneDistance-ds;
                occlusion+=smoothstep(.25,1.1,difference)*
                           (1.0-smoothstep(1.9,5.2,difference));
            }
            float fade=1.0-smoothstep(20.0,44.0,sceneDistance);
            rgb*=1.0-occlusion*.022*fade;
        }
    }
    // ULTRA only: four-step screen-space volumetric single-scattering.
    // Samples the SAME foliage shadow atlas along camera rays. Sunlit air
    // between leaf silhouettes produces modest shafts without a full 3D fog
    // voxel allocation or an expensive second full-resolution render pass.
    if(uGodRaysEnabled>.5){
        vec2 screen=uv*2.-1.;
        float aspect=uResolution.x/max(1.,uResolution.y);
        float yaw=uCamera.x,pitch=uCamera.y;
        vec3 fw=vec3(sin(yaw)*cos(pitch),sin(pitch),cos(yaw)*cos(pitch));
        vec3 right=normalize(cross(fw,vec3(0.,1.,0.)));
        vec3 up=normalize(cross(right,fw));
        vec3 ray=normalize(fw+screen.x*aspect*.6745*right+
                             screen.y*.6745*up);
        float scattering=pow(max(dot(ray,normalize(uSunDirection)),0.),2.2);
        if(scattering>.015){
            float tEnd=depth0>.999?68.:min(68.,linearDepth(depth0));
            float stepsize=(tEnd-2.0)*.25;
            float total=0.;
            if(stepsize>0.){
                for(int i=0;i<4;i++){
                    float d=2.0+(float(i)+.5)*stepsize;
                    vec3 pos=uEye+ray*d;
                    float density=exp(-max(pos.y-5.,0.)*.028)*
                            exp(-d*.013);
                    float sunlight=sampleSun(pos);
                    total+=sunlight*density;
                }
                float source=(total*.25)*scattering;
                vec3 aerial=vec3(1.,.86,.64)*source*.11*
                             (1.-exp(-tEnd*.046));
                rgb=mix(rgb,vec3(.69,.75,.70),.018*
                        (1.-exp(-tEnd*.035)));
                rgb+=aerial;
            }
        }
    }
    // Color grade after rendering, preserving the existing material PBR curve.
    vec3 neutral=vec3(lum(rgb));
    rgb=mix(neutral,rgb,.94);
    rgb=clamp((rgb-.5)*1.07+.5,0.0,1.0);
    float vignette=dot((uv-.5)*vec2(1.35,1.0),(uv-.5)*vec2(1.35,1.0));
    rgb*=1.0-.085*smoothstep(.24,.58,vignette);
    Frag=vec4(rgb,1.0);
}
)GLSL";
