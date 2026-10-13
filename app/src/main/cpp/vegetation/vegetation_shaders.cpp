#include "renderer/shaders.hpp"
// GLES 3.0 renderer shader sources; linked separately from JNI/gameplay.
// Exact shader source preserved during this refactor for visual parity.
const char* const INST_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aInst;
layout(location=4) in vec4 aMeta;
layout(location=5) in vec4 aPivot;
uniform mat4 uVP;uniform float uTime,uWind,uSpin,uLeafFlutter,uAnimalGait,uPivotWind;
uniform sampler2D uWindField;
uniform vec3 uPlayerPosition;
uniform float uInteractiveVegetation;
out vec3 vN,vP;out vec2 vUV;out float vTint,vType;
void main(){
    float ang=aMeta.x+uSpin,ca=cos(ang),sa=sin(ang);
    vec3 p=aPos*aInst.w;
    // Local-space pivot hierarchy: branch bases stay stable while tips flex.
    // Encoded once at mesh creation, no bone or per-leaf draw call.
    if(uPivotWind>.0 && aPivot.w>.0){
        vec3 pivot=aPivot.xyz*aInst.w;
        vec3 rel=p-pivot;
        float phase=uTime*1.54+aInst.x*.19+aInst.z*.13+
                    dot(aPivot.xyz,vec3(.13,.21,.18));
        float angle=(sin(phase)+.23*sin(phase*2.47))*aPivot.w*uPivotWind;
        float si=sin(angle),co=cos(angle);
        rel.xz=mat2(co,-si,si,co)*rel.xz;
        p=pivot+rel;
    }
    if(uAnimalGait>.0){
        // Mobile procedural quadruped gait: light hoof swing and limb lift.
        // Real skeletal locomotion remains a separate animation milestone.
        float phase=uTime*7.0+aInst.x*.23+aInst.z*.17;
        float leftRight=sign(aPos.x+.001);
        float hindFront=sign(aPos.z+.001);
        float hooves=1.0-smoothstep(.20,.83,aPos.y);
        float stride=sin(phase+leftRight*1.5708+hindFront*3.14159);
        p.z+=stride*hooves*.13*aInst.w*uAnimalGait;
        p.y+=max(0.0,stride)*hooves*.072*aInst.w*uAnimalGait;
    }
    if(uWind>0.0){
        float phase=uTime*1.08+aInst.x*.13+aInst.z*.17;
        float primary=sin(phase+aPos.y*.32);
        vec2 fieldUV=fract(aInst.xz*.0082+vec2(uTime*.0074,uTime*.0031));
        vec3 windMap=texture(uWindField,fieldUV).rgb;
        float gust=(windMap.r-.48)*.92+
            sin(phase*.41+sin(aInst.z*.065))*.26;
        float local=sin(uTime*2.45+aPos.x*1.9+aPos.z*1.6+aInst.y)*.18;
        float force=(primary+gust+local)*uWind*max(aPos.y,0.0)*.032;
        p.x+=force;
        p.z+=force*.52;
    }
    // Cheap player-driven bend (local plant collisions), based on proximity.
    // Root stays anchored: only taller vertex regions react to movement.
    if(uInteractiveVegetation>.5){
        vec2 delta=aInst.xz-uPlayerPosition.xz;
        float nearDist=length(delta);
        float strength=(1.0-smoothstep(.38,2.35,nearDist))*
                       smoothstep(.03,.75,aPos.y);
        vec2 push=delta/max(nearDist,.24);
        float flutter=sin(uTime*8.0+aInst.x*.54+aInst.z*.42);
        p.xz+=push*(.11+flutter*.023)*strength*aInst.w;
    }
    // Small, per-card flutter is distinct from the entire branch wind sway.
    if(uLeafFlutter>0.0){
        float phase=aPos.x*9.17+aPos.z*7.23+aInst.x*.31+uTime*3.4;
        float flutter=sin(phase)*sin(uTime*.67+aInst.z*.15);
        p.x+=flutter*uLeafFlutter*aInst.w;
        p.z+=cos(phase*.84)*uLeafFlutter*.65*aInst.w;
    }
    vec3 r=vec3(p.x*ca-p.z*sa,p.y,p.x*sa+p.z*ca);
    vP=r+aInst.xyz;
    vN=normalize(vec3(aNormal.x*ca-aNormal.z*sa,aNormal.y,aNormal.x*sa+aNormal.z*ca));
    vUV=aUV;vTint=aMeta.y;vType=aMeta.z;
    gl_Position=uVP*vec4(vP,1.0);
}
)GLSL";

const char* const INST_FS=R"GLSL(#version 300 es
precision highp float;
precision highp sampler2DShadow;
in vec3 vN,vP;in vec2 vUV;in float vTint,vType;
uniform sampler2D uTex,uMaterialNormal,uMaterialRough;
uniform vec4 uAtlasRect;
uniform vec3 uEye,uFog,uSun;
uniform float uLight,uCutout,uEmissive,uRoughness,uBump,uFoliage,uUseRoughnessMap,uLeafLitter;
uniform sampler2DShadow uSunShadow;
uniform mat4 uSunShadowVP;
uniform float uShadowsEnabled,uShadowTexel,uShadowQuality;
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
    // Non-atlas materials (notably bark) MUST keep their repeating UVs.
    vec2 uv=(uAtlasRect.z<.99)?
          uAtlasRect.xy+clamp(vUV,vec2(.002),vec2(.998))*uAtlasRect.zw:vUV;
    vec4 s=texture(uTex,uv);
    if(s.a<uCutout)discard;
    vec3 n=normalize(vN),V=normalize(uEye-vP),L=normalize(uSun);
    // Alpha-tested foliage cards are rendered two-sided, including their lighting.
    if(uFoliage>.5 && !gl_FrontFacing)n=-n;
    if(uBump>.01){
        vec3 q0=dFdx(vP),q1=dFdy(vP);
        vec2 st0=dFdx(vUV),st1=dFdy(vUV);
        float det=st0.x*st1.y-st0.y*st1.x;
        if(abs(det)>.00001){
            vec3 T=normalize((q0*st1.y-q1*st0.y)/det);
            vec3 B=normalize((-q0*st1.x+q1*st0.x)/det);
            vec3 map=texture(uMaterialNormal,uv).rgb*2.0-1.0;
            n=normalize(n+uBump*(T*map.x+B*map.y));
        }
    }
    float nl=dot(n,L);
    float visibility=1.0;
    if(uShadowsEnabled>.5){
        vec4 clip=uSunShadowVP*vec4(vP,1.0);
        vec3 sp=clip.xyz/max(.0001,clip.w)*.5+.5;
        if(all(greaterThan(sp,vec3(0.0)))&&all(lessThan(sp,vec3(1.0)))){
            float bias=max(.0007,(1.0-max(dot(n,L),0.0))*.0013);
            if(uFoliage<.5 && uShadowQuality>1.5){
                float sum=0.0;
                for(int y=0;y<2;y++)for(int x=0;x<2;x++){
                    vec2 uvShift=(vec2(float(x),float(y))-.5)*uShadowTexel*1.7;
                    sum+=texture(uSunShadow,vec3(sp.xy+uvShift,sp.z-bias));
                }
                visibility=sum*.25;
            }else{
                visibility=texture(uSunShadow,vec3(sp.xy,sp.z-bias));
            }
        }
    }
    float diff=uFoliage>.5 ? (.43+.57*max(nl*.7+.30,0.0)) : (.26+.78*max(nl,0.0)*uLight);
    vec3 hemisphere=mix(vec3(.72,.67,.56),vec3(.87,.96,1.03),
                        clamp(n.y*.5+.5,0.,1.));
    vec3 rgb=s.rgb*diff*(.82+vTint*.33)*hemisphere*
        mix(1.0,.38+.62*visibility,max(dot(n,L),0.0));
    if(uFoliage>.5){
        // A thin leaf transmits warm green sunlight even when back-facing;
        // alpha coverage approximates optical leaf thickness.
        float thickness=clamp(.47+.45*(1.0-s.g),.18,.82);
        float backLight=pow(max(-nl,0.0),1.45);
        float wrap=clamp((nl+.42)/1.42,0.,1.);
        float transmitted=(backLight*.30+wrap*.065)*uLight*
                           mix(.62,1.0,visibility)*(1.0-thickness*.44);
        rgb+=s.rgb*vec3(.42,.71,.26)*transmitted;
    }else{
        vec3 orm=texture(uMaterialRough,uv).rgb;
        float rough=(uUseRoughnessMap>.5)?clamp(orm.g,.28,.98):uRoughness;
        if(uUseRoughnessMap>.5)rgb*=mix(.72,1.,orm.r);
        rgb+=vec3(specGGX(n,V,L,rough)*max(nl,0.0)*uLight*.46);
    }
    if(uLeafLitter>.5){
        // Warm, weathered dead leaves with variation from each patch's tint.
        float age=.38+.32*vTint;
        rgb*=mix(vec3(1.03,.96,.74),vec3(1.24,.83,.52),age);
    }
    if(vType>.5)rgb*=vec3(.84,1.04,.86);
    rgb+=uEmissive*vec3(.10,.44,.65);
    rgb=rgb/(rgb+vec3(.32));
    rgb=pow(rgb,vec3(.94));
    float fog=smoothstep(92.0,276.0,distance(uEye,vP));
    Frag=vec4(mix(rgb,uFog,fog),s.a);
}
)GLSL";
