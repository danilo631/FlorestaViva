#pragma once
// River surface conforms to spline centre, and is clipped to chunk bounds.
// Geometry streams alongside terrain; tiles never draw a duplicate river.
static ModelData forestRiverSurface(int cx,int cz){
    ModelData m;
    constexpr int rows=33,cols=8;
    const float left=cx*CHUNK,right=left+CHUNK;
    const float bottom=cz*CHUNK,top=bottom+CHUNK;
    float minX=1e10f,maxX=-1e10f;
    for(int j=0;j<=rows;j++){
        float z=bottom+float(j)*CHUNK/rows;
        minX=std::min(minX,riverCenter(z)-riverWidth(z));
        maxX=std::max(maxX,riverCenter(z)+riverWidth(z));
    }
    if(minX>right || maxX<left)return m;
    for(int j=0;j<=rows;j++){
        const float z=bottom+float(j)*CHUNK/rows;
        const float c=riverCenter(z),width=riverWidth(z)-.4f;
        const float y=riverLevel(z)+.045f;
        for(int i=0;i<=cols;i++){
            const float across=(float(i)/cols*2.f-1.f);
            const float x=clampf(c+width*across,left,right);
            const float flowing=riverLevel(z);
            (void)flowing;
            m.v.push_back({x,y,z,0,1,0,
                float(i)/cols,float(z)*.04f});
        }
    }
    for(int j=0;j<rows;j++)for(int i=0;i<cols;i++){
        uint32_t a=uint32_t(j*(cols+1)+i),b=a+uint32_t(cols+1);
        m.triangle(a,a+1,b);m.triangle(a+1,b+1,b);
    }
    return m;
}
static const char* RIVER_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
uniform mat4 uVP;
out vec3 vWorld;
out vec2 vTex;
void main(){
    vWorld=aPos;
    vTex=aUV;
    gl_Position=uVP*vec4(aPos,1.);
}
)GLSL";
static const char* RIVER_FS=R"GLSL(#version 300 es
precision highp float;
in vec3 vWorld;
in vec2 vTex;
uniform vec3 uEye,uFog,uSun;
uniform float uTime,uLight;
uniform sampler2D uWaterNormal,uRiverStones;
out vec4 Frag;
void main(){
    vec2 p=vWorld.xz*.115;
    vec2 flow=vec2(-.15,uTime*.083);
    vec3 a=texture(uWaterNormal,p+flow).xyz*2.-1.;
    vec3 b=texture(uWaterNormal,p*1.73-flow*.75).xyz*2.-1.;
    vec3 n=normalize(vec3((a.x+b.x)*.23,1.,(a.y+b.y)*.21));
    vec3 V=normalize(uEye-vWorld);
    vec3 L=normalize(uSun);
    float fresnel=pow(1.-max(dot(V,n),0.),4.);
    float sparkling=pow(max(dot(reflect(-L,n),V),0.),72.);
    float deep=clamp(abs(vTex.x-.5)*2.,0.,1.);
    float foam=(smoothstep(.77,.96,deep))*
       (.5+.5*sin(vWorld.z*.75+uTime*3.+sin(vWorld.x*.6)));
    float microFoam=smoothstep(.70,.91,deep)*
        texture(uRiverStones,vWorld.xz*.22+vec2(0,uTime*.025)).r;
    vec3 blue=vec3(.035,.21,.23);
    vec3 shallow=vec3(.16,.33,.29);
    vec3 bottom=texture(uRiverStones,vWorld.xz*.135).rgb;
    vec3 color=mix(blue,shallow,deep*.55);
    color=mix(color,bottom*.65,deep*.22);
    color=mix(color,vec3(.52,.76,.70),fresnel*.51);
    color+=vec3(1.,.88,.65)*sparkling*.47*uLight;
    color=mix(color,vec3(.65,.73,.68),clamp(foam*.23+microFoam*.10,0.,.40));
    float fog=smoothstep(95.,276.,distance(uEye,vWorld));
    color=mix(color,uFog,fog);
    float opacity=clamp(.82+fresnel*.14,0.,.98);
    Frag=vec4(color,opacity);
}
)GLSL";
