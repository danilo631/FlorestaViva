#pragma once
// Offline CPU preprocessor for Nanite-like mesh clustering.
// THIS IS NOT Nanite, GPU mesh shading, virtualized geometry or occlusion.
// Strict limits allow later GPU-driven selection; current GLES renderer
// still uses regular indexed triangle meshes and its existing 3 LOD tiers.
struct ForestMeshlet{
    uint32_t firstTriangle=0,triangleCount=0,uniqueVertices=0;
    float cx=0,cy=0,cz=0,radius=0;
};
static std::vector<ForestMeshlet> forestClusterMesh(
    const ModelData& data,uint32_t maxVertices=64,uint32_t maxTriangles=126){
    if(maxVertices<3||maxTriangles<1)throw std::invalid_argument("bad meshlet limits");
    std::vector<ForestMeshlet> out;
    std::vector<uint32_t> vertexSet;
    auto close=[&](uint32_t start,uint32_t triangles){
        if(!triangles)return;
        float cx=0,cy=0,cz=0;
        for(uint32_t idx:vertexSet){
            const auto& v=data.v[idx];cx+=v.x;cy+=v.y;cz+=v.z;
        }
        float n=float(vertexSet.size());cx/=n;cy/=n;cz/=n;
        float radius=0;
        for(uint32_t idx:vertexSet){
            const auto& v=data.v[idx];
            float dx=v.x-cx,dy=v.y-cy,dz=v.z-cz;
            radius=std::max(radius,std::sqrt(dx*dx+dy*dy+dz*dz));
        }
        out.push_back({start,triangles,uint32_t(vertexSet.size()),cx,cy,cz,radius});
        vertexSet.clear();
    };
    uint32_t start=0,count=0;
    for(uint32_t k=0;k<data.i.size();k+=3){
        uint32_t candidate[3]={data.i[k],data.i[k+1],data.i[k+2]};
        uint32_t extra=0;
        for(int c=0;c<3;c++){
            const uint32_t vertex=candidate[c];
            bool earlier=false;
            for(int i=0;i<c;i++)if(candidate[i]==vertex){earlier=true;break;}
            if(!earlier && std::find(vertexSet.begin(),vertexSet.end(),vertex)==vertexSet.end())extra++;
        }
        // Prevent more than 64 distinct vertices per cluster.
        if(count && (count>=maxTriangles || vertexSet.size()+extra>maxVertices)){
            close(start,count);
            start=k/3;count=0;
        }
        for(uint32_t vertex:candidate){
            if(vertex>=data.v.size())throw std::out_of_range("index outside mesh");
            if(std::find(vertexSet.begin(),vertexSet.end(),vertex)==vertexSet.end())
                vertexSet.push_back(vertex);
        }
        count++;
    }
    close(start,count);
    return out;
}
