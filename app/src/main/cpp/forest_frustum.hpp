#pragma once
// View-frustum plane extraction for column-major GLES matrices.
// Parent engine only distance-culls some instances; this also prevents
// dispatching entire terrain / river chunks behind the camera.
struct ForestClipPlane{float a,b,c,d;};
static bool forestSphereVisible(const Mat4& vp,Vec3 center,float radius){
    const float* m=vp.m;
    const ForestClipPlane planes[6]={
        {m[3]+m[0], m[7]+m[4], m[11]+m[8], m[15]+m[12]},
        {m[3]-m[0], m[7]-m[4], m[11]-m[8], m[15]-m[12]},
        {m[3]+m[1], m[7]+m[5], m[11]+m[9], m[15]+m[13]},
        {m[3]-m[1], m[7]-m[5], m[11]-m[9], m[15]-m[13]},
        {m[3]+m[2], m[7]+m[6], m[11]+m[10],m[15]+m[14]},
        {m[3]-m[2], m[7]-m[6], m[11]-m[10],m[15]-m[14]}
    };
    for(const auto& p:planes){
        const float len=std::sqrt(p.a*p.a+p.b*p.b+p.c*p.c);
        if(len<1e-6f)continue;
        if(p.a*center.x+p.b*center.y+p.c*center.z+p.d < -radius*len)
            return false;
    }
    return true;
}
static bool forestChunkVisible(const Mat4& vp,int64_t chunkKey){
    const uint64_t key=uint64_t(chunkKey);
    const int cx=int(int32_t(uint32_t(key>>32)));
    const int cz=int(int32_t(uint32_t(key)));
    const Vec3 c{float(cx*CHUNK)+CHUNK*.5f,0.f,
                 float(cz*CHUNK)+CHUNK*.5f};
    // Chunk diagonal ~45m; 54m conservative sphere includes tall trees/river.
    return forestSphereVisible(vp,c,56.f);
}
