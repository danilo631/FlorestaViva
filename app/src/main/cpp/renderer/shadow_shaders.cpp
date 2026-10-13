#include "renderer/shaders.hpp"
// GLES 3.0 renderer shader sources; linked separately from JNI/gameplay.
// Exact shader source preserved during this refactor for visual parity.
const char* const TREE_DEPTH_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aInst;
layout(location=4) in vec4 aMeta;
layout(location=5) in vec4 aPivot;
uniform mat4 uLightVP;
uniform float uTime,uPivotWind;
out vec2 vUV;
void main(){
  float a=aMeta.x;
  vec3 p=aPos*aInst.w;
  if(uPivotWind>.0&&aPivot.w>.0){
      vec3 pivot=aPivot.xyz*aInst.w;
      vec3 rel=p-pivot;
      float phase=uTime*1.54+aInst.x*.19+aInst.z*.13+
                  dot(aPivot.xyz,vec3(.13,.21,.18));
      float angle=(sin(phase)+.23*sin(phase*2.47))*aPivot.w*uPivotWind;
      float si=sin(angle),co=cos(angle);
      rel.xz=mat2(co,-si,si,co)*rel.xz;
      p=pivot+rel;
  }
  vec3 world=vec3(p.x*cos(a)-p.z*sin(a),p.y,p.x*sin(a)+p.z*cos(a))+aInst.xyz;
  gl_Position=uLightVP*vec4(world,1.0);
  vUV=aUV;
}
)GLSL";

const char* const TREE_DEPTH_FS=R"GLSL(#version 300 es
precision highp float;
in vec2 vUV;
uniform sampler2D uMask;
uniform vec4 uRect;
uniform float uThreshold;
layout(location=0) out vec4 unusedOutput;
void main(){
  vec2 uv=(uRect.z<.99)?uRect.xy+clamp(vUV,vec2(.002),vec2(.998))*uRect.zw:vUV;
  if(uThreshold>0.0 && texture(uMask,uv).a<uThreshold)discard;
  unusedOutput=vec4(1.0);
}
)GLSL";

const char* const SHADOW_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aInst;
uniform mat4 uVP;
out vec2 vUV;
void main(){vUV=aUV;gl_Position=uVP*vec4(aPos*aInst.w+aInst.xyz,1.0);}
)GLSL";

const char* const SHADOW_FS=R"GLSL(#version 300 es
precision mediump float;
in vec2 vUV;out vec4 Frag;
void main(){float x=length(vUV-vec2(.5))*2.0;float a=(1.0-smoothstep(.16,.95,x))*.23;Frag=vec4(.015,.025,.014,a);}
)GLSL";
