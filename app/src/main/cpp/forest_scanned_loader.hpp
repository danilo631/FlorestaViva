#pragma once
// Verified build-time converted glTF -> FVME mobile mesh.
// This is a compact loader, NOT a glTF runtime parser; it preserves
// scanned vertex positions/normals/UVs and bounds memory allocations.
static bool forestDecodeScannedMesh(const uint8_t* raw,size_t length,ModelData& out){
    if(raw==nullptr||length<16||std::memcmp(raw,"FVME0001",8)!=0)return false;
    uint32_t vertices=0,indices=0;
    std::memcpy(&vertices,raw+8,sizeof(vertices));
    std::memcpy(&indices,raw+12,sizeof(indices));
    if(vertices<10||vertices>180000||indices<30||indices>540000||indices%3)return false;
    if(length!=16+size_t(vertices)*sizeof(Vertex)+size_t(indices)*sizeof(uint32_t))
        return false;
    static_assert(sizeof(Vertex)==48,"FVME requires 12 float32 channels per vertex");
    ModelData mesh;
    mesh.v.resize(vertices);mesh.i.resize(indices);
    std::memcpy(mesh.v.data(),raw+16,size_t(vertices)*sizeof(Vertex));
    std::memcpy(mesh.i.data(),raw+16+size_t(vertices)*sizeof(Vertex),
                size_t(indices)*sizeof(uint32_t));
    for(const auto& v:mesh.v){
        const float values[]={v.x,v.y,v.z,v.nx,v.ny,v.nz,v.u,v.v};
        for(float a:values)if(!std::isfinite(a))return false;
    }
    for(uint32_t index:mesh.i)if(index>=vertices)return false;
    out=std::move(mesh);
    return true;
}
