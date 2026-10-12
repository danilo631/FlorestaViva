#version 450
layout(location=0) in vec2 vUV;
layout(set=0,binding=1) uniform sampler2D leafAtlas;
layout(location=0) out vec4 Frag;
void main(){vec4 leaf=texture(leafAtlas,vUV);if(leaf.a<.42)discard;Frag=vec4(leaf.rgb,1.0);}
