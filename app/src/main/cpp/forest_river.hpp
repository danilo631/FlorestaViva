#pragma once
// Graphics 9: independently clipped water polygons for each 64 m chunk.
// Original implementation "clamped" all strip vertices into the chunk,
// producing zero-area triangles, z-fighting and conspicuous broken riverbanks.
// Here each cell is polygon-clipped against both X chunk planes. Interpolated
// coordinates and continuous z stations match exactly across chunk borders.
// All code is original; ForestGenerator (GPLv3) is a reference for concepts.

struct RiverClipVertex {
    float x,y,z,u,v;
};
static std::vector<RiverClipVertex> riverClipX(
    const std::vector<RiverClipVertex>& polygon,float boundary,bool keepRight){
    std::vector<RiverClipVertex> out;
    if(polygon.empty())return out;
    auto inside=[&](const RiverClipVertex& v){
        return keepRight ? v.x>=boundary : v.x<=boundary;
    };
    auto intersection=[&](const RiverClipVertex& a,const RiverClipVertex& b){
        const float dx=b.x-a.x;
        float t=std::fabs(dx)>1e-10f?(boundary-a.x)/dx:0.f;
        t=clampf(t,0.f,1.f);
        return RiverClipVertex{boundary,a.y+(b.y-a.y)*t,
             a.z+(b.z-a.z)*t,a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t};
    };
    for(size_t i=0;i<polygon.size();i++){
        const auto& a=polygon[i];
        const auto& b=polygon[(i+1)%polygon.size()];
        const bool ia=inside(a),ib=inside(b);
        if(ia && ib)out.push_back(b);
        else if(ia && !ib)out.push_back(intersection(a,b));
        else if(!ia && ib){out.push_back(intersection(a,b));out.push_back(b);}
    }
    return out;
}
static float riverMeshX(float z,float u){
    const float outer=riverWidth(z)+.85f;
    // Water x-extent must use the same projected lateral metric as terrain.
    return riverCenter(z)+(u*2.f-1.f)*outer*riverScaleX(z);
}
static ModelData forestRiverSurface(int cx,int cz){
    ModelData mesh;
    constexpr int rows=64,columns=8;
    const float left=float(cx*CHUNK),right=left+float(CHUNK);
    const float bottom=float(cz*CHUNK);
    float minX=1e9f,maxX=-1e9f;
    for(int j=0;j<=rows;j++){
        const float z=bottom+float(j)*float(CHUNK)/rows;
        minX=std::min(minX,riverMeshX(z,0.f));
        maxX=std::max(maxX,riverMeshX(z,1.f));
    }
    if(minX>=right || maxX<=left)return mesh;
    mesh.v.reserve(rows*columns*7);
    mesh.i.reserve(rows*columns*15);
    for(int j=0;j<rows;j++){
        const float z0=bottom+float(j)*float(CHUNK)/rows;
        const float z1=bottom+float(j+1)*float(CHUNK)/rows;
        for(int k=0;k<columns;k++){
            const float u0=float(k)/columns,u1=float(k+1)/columns;
            const float y0=riverLevel(z0)+.035f,y1=riverLevel(z1)+.035f;
            std::vector<RiverClipVertex> polygon={
                {riverMeshX(z0,u0),y0,z0,u0,z0*.047f},
                {riverMeshX(z0,u1),y0,z0,u1,z0*.047f},
                {riverMeshX(z1,u1),y1,z1,u1,z1*.047f},
                {riverMeshX(z1,u0),y1,z1,u0,z1*.047f}
            };
            polygon=riverClipX(polygon,left,true);
            polygon=riverClipX(polygon,right,false);
            if(polygon.size()<3)continue;
            // Duplicate shared vertices within each clipped quad to avoid
            // cracks caused by welding unlike x-z intersections.
            for(size_t v=1;v+1<polygon.size();v++){
                const auto& a=polygon[0];
                const auto& b=polygon[v];
                const auto& c=polygon[v+1];
                const float area=(b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);
                if(std::fabs(area)<1.0e-6f)continue;
                const uint32_t base=uint32_t(mesh.v.size());
                for(const RiverClipVertex& vertex:{a,b,c}){
                    mesh.v.push_back({vertex.x,vertex.y,vertex.z,0,1,0,
                                      vertex.u,vertex.v});
                }
                // Reverse polygon X/Z winding to obtain +Y-facing triangles.
                if(area>0.f)mesh.triangle(base,base+2,base+1);
                else mesh.triangle(base,base+1,base+2);
            }
        }
    }
    return mesh;
}
static const char* RIVER_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
uniform mat4 uVP;
uniform float uTime;
out vec3 vWorld;
out vec2 vTex;
void main(){
    vWorld=aPos;
    vTex=aUV;
    // Tiny crest motion, suppressed at the shore to prevent water penetrating
    // the bank; UVs are continuous on both sides of streamed chunks.
    float edge=abs(aUV.x*2.0-1.0);
    float ripple=sin(aPos.z*.81+uTime*2.1+sin(aPos.x*.39))*.019;
    ripple+=sin(aPos.x*1.36-aPos.z*.31+uTime*1.54)*.010;
    vWorld.y+=ripple*(1.0-smoothstep(.6,.97,edge));
    gl_Position=uVP*vec4(vWorld,1.);
}
)GLSL";
static const char* RIVER_FS=R"GLSL(#version 300 es
precision highp float;
precision highp sampler2DShadow;
in vec3 vWorld;
in vec2 vTex;
uniform vec3 uEye,uFog,uSun;
uniform float uTime,uLight;
uniform sampler2D uWaterNormal,uRiverStones;
uniform sampler2DShadow uSunShadow;
uniform mat4 uSunShadowVP;
uniform float uShadowsEnabled,uShadowTexel,uWaterQuality;
out vec4 Frag;
float sunVisibility(vec3 world){
    if(uShadowsEnabled<.5)return 1.0;
    vec4 light=uSunShadowVP*vec4(world,1.0);
    vec3 projected=light.xyz/max(.0001,light.w)*.5+.5;
    if(any(lessThanEqual(projected,vec3(0.0))) ||
       any(greaterThanEqual(projected,vec3(1.0))))return 1.0;
    float sum=0.0;
    float taps=uWaterQuality>1.5?1.7:1.1;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        vec2 shift=(vec2(float(x),float(y))-.5)*uShadowTexel*taps;
        sum+=texture(uSunShadow,vec3(projected.xy+shift,projected.z-.0017));
    }
    return sum*.25;
}
float hashRiver(vec2 p){
    return fract(sin(dot(p,vec2(127.1,311.7)))*43758.545312);
}
void main(){
    // Channel-aligned two-frequency normal flow; no blue opaque "tape".
    vec2 uv=vec2(vTex.x*2.2,vTex.y*2.4);
    vec2 flow=vec2(.031*uTime,.125*uTime);
    vec3 a=texture(uWaterNormal,uv+flow).rgb*2.-1.;
    vec3 b=texture(uWaterNormal,uv*1.64-flow*.73+vec2(.18,.33)).rgb*2.-1.;
    vec2 slope=(a.xy*.66+b.xy*.34)*.35;
    vec3 n=normalize(vec3(slope.x,1.0,slope.y));
    vec3 view=normalize(uEye-vWorld),sun=normalize(uSun);
    float nv=clamp(dot(view,n),0.,1.);
    float fresnel=.020+.980*pow(1.-nv,5.);
    float edge=clamp(abs(vTex.x*2.-1.),0.,1.);
    float edgeFade=smoothstep(.67,1.0,edge);
    float depth=1.46*(1.-smoothstep(.03,1.,edge));
    // Beer-Lambert approximation using approximate channel depth from
    // the same bank profile as the CPU-carved terrain.
    vec3 extinction=vec3(1.38,.88,.68);
    vec3 transmission=exp(-extinction*depth);
    vec3 stones=texture(uRiverStones,vWorld.xz*.16).rgb;
    vec3 silt=mix(stones*.92,vec3(.18,.21,.16),.28);
    vec3 waterBody=vec3(.035,.205,.195);
    vec3 submerged=silt*transmission+waterBody*(vec3(1.)-transmission);
    vec3 skyReflection=mix(vec3(.12,.30,.40),uFog,.27);
    vec3 reflected=mix(submerged,skyReflection,fresnel*.82);
    vec3 halfway=normalize(view+sun);
    float nH=max(dot(n,halfway),0.);
    float roughness=mix(.105,.18,edgeFade);
    float a2=pow(roughness,4.);
    float d=a2/(3.14159265*pow(nH*nH*(a2-1.)+1.,2.)+.0002);
    float specular=clamp(d*.012,0.,2.2)*
        max(dot(n,sun),0.)*uLight*sunVisibility(vWorld);
    // Shore foam follows the clipped banks and breaks up in patches, not
    // perfectly parallel white stripes.
    float turbulence=texture(uWaterNormal,uv*.63-flow*.4).r;
    float foamMask=smoothstep(.84,.99,edge) *
       smoothstep(.36,.75,turbulence*.6+
               .4*hashRiver(floor(vWorld.xz*2.4+uTime*.65)));
    vec3 color=reflected+vec3(1.0,.89,.66)*specular;
    color=mix(color,vec3(.72,.78,.70),foamMask*.55);
    color=mix(color,uFog,smoothstep(95.,278.,distance(uEye,vWorld)));
    // Mild filmic shoulder matches the terrain rasterization pipeline.
    color=(color*(2.51*color+.03))/
          (color*(2.43*color+.59)+.14);
    color=pow(clamp(color,0.,1.),vec3(.95));
    // Allow the river bed to remain visible through shallow water.
    float opacity=clamp(.34+(1.-transmission.g)*.45+fresnel*.11,
                        .28,.89);
    Frag=vec4(color,opacity);
}
)GLSL";
