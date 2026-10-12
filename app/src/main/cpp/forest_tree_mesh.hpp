#pragma once
// Procedural, deterministic tree meshes in game.cpp's local geometry types.
// No third-party runtime mesh library or game-engine dependency.
// Wood, close leaf cards, mid foliage and far silhouettes are *separate* GPU meshes.
struct ForestTreeMesh {
    ModelData woodHigh,woodMid,foliageHigh,foliageMid,foliageFar;
};
static float forestRnd(std::mt19937 &rng,float a=0.f,float b=1.f){
    return std::uniform_real_distribution<float>(a,b)(rng);
}
static Vec3 forestMix(Vec3 a,Vec3 b,float t){return a*(1.f-t)+b*t;}
static float forestLength(Vec3 p){return std::sqrt(std::max(dot(p,p),0.f));}
static Vec3 forestPerpendicular(Vec3 a){
    Vec3 v=std::fabs(a.y)>.87f?Vec3{1,0,0}:Vec3{0,1,0};
    return normalize(cross(a,v));
}
// Tube sections track a curved piecewise cubic centerline; radii shrink to fine
// branches rather than terminating in straight cylinders.
static void forestTube(ModelData &mesh,const std::vector<Vec3>& control,
                       float radiusStart,float radiusEnd,int facets,int substeps,
                       float barkRepeat=1.f){
    if(control.size()<2||facets<3||substeps<1)return;
    const int segments=int(control.size()-1)*substeps;
    std::vector<Vec3> samples;
    samples.reserve(size_t(segments+1));
    for(size_t seg=0;seg+1<control.size();++seg){
        Vec3 p0=control[seg==0?seg:seg-1],p1=control[seg];
        Vec3 p2=control[seg+1],p3=control[std::min(seg+2,control.size()-1)];
        for(int j=0;j<substeps;j++){
            float t=float(j)/substeps, t2=t*t, t3=t2*t;
            Vec3 c=(p1*2.f + (p2-p0)*t +
                (p0*2.f-p1*5.f+p2*4.f-p3)*t2 +
                (p1*3.f-p0-p2*3.f+p3)*t3)*.5f;
            samples.push_back(c);
        }
    }
    samples.push_back(control.back());
    const uint32_t base=uint32_t(mesh.v.size());
    float distance=0.f;
    for(int i=0;i<=segments;i++){
        Vec3 tangent=normalize(samples[std::min(i+1,segments)]-
            samples[std::max(i-1,0)]);
        Vec3 side=forestPerpendicular(tangent),binormal=normalize(cross(tangent,side));
        float t=float(i)/segments;
        float rad=std::max(.008f,radiusStart*std::pow(1.f-t,1.05f)+radiusEnd*t);
        if(i>0)distance+=forestLength(samples[i]-samples[i-1]);
        for(int s=0;s<=facets;s++){
            float angle=2.f*PI*float(s)/facets;
            Vec3 n=normalize(side*std::cos(angle)+binormal*std::sin(angle));
            Vec3 p=samples[i]+n*rad;
            mesh.v.push_back({p.x,p.y,p.z,n.x,n.y,n.z,
                float(s)/facets, distance*barkRepeat});
        }
    }
    for(int i=0;i<segments;i++)for(int s=0;s<facets;s++){
        uint32_t a=base+uint32_t(i*(facets+1)+s),b=a+uint32_t(facets+1);
        mesh.triangle(a,b,a+1);mesh.triangle(a+1,b,b+1);
    }
}
// Creates one tapered and slightly folded cluster card with a physically
// modelled tip and centre vein. Card samples one of 16 alpha-mask atlas cells.
static void forestLeaf(ModelData &mesh,Vec3 start,Vec3 outward,
                       float length,float width,int cell,float fold,int detail){
    outward=normalize(outward);
    Vec3 across=forestPerpendicular(outward);
    Vec3 face=normalize(cross(across,outward));
    const int slices=std::max(2,detail);
    const uint32_t base=uint32_t(mesh.v.size());
    const float cellX=float(cell%4)*.25f,cellY=float(cell/4)*.25f;
    for(int k=0;k<=slices;k++){
        const float t=float(k)/slices;
        const float halfWidth=width*std::pow(std::max(std::sin(PI*t),0.f),.74f);
        const float curve=std::sin(t*PI)*fold*length;
        Vec3 center=start+outward*length*t+face*curve;
        const float vv=cellY+(.025f+.95f*(1.f-t))*.25f;
        for(int j=0;j<3;j++){
            const float lateral=float(j-1);
            Vec3 p=center+across*(lateral*halfWidth);
            Vec3 nn=normalize(face+across*(lateral*.22f)+outward*(.18f*std::cos(PI*t)));
            float uu=cellX+(.025f+.95f*float(j)*.5f)*.25f;
            mesh.v.push_back({p.x,p.y,p.z,nn.x,nn.y,nn.z,uu,vv});
        }
    }
    for(int k=0;k<slices;k++)for(int j=0;j<2;j++){
        uint32_t a=base+uint32_t(k*3+j),b=a+3;
        mesh.triangle(a,b,a+1);mesh.triangle(a+1,b,b+1);
    }
}
static void forestSpray(ModelData &hi,ModelData &mid,Vec3 tip,Vec3 heading,
                        float scale,int species,std::mt19937 &rng,int sprigIndex,
                        bool conifer){
    // Separate leaflets originate at twigs. Sparse centre gives layered depth.
    const int count=conifer?5:3;
    Vec3 basis=forestPerpendicular(heading);
    for(int i=0;i<count;i++){
        float a=2.f*PI*float(i)/count+forestRnd(rng,-.45f,.45f);
        float tilt=forestRnd(rng,-.14f,.44f);
        Vec3 dir=normalize(heading*.51f+basis*std::cos(a)+
                           normalize(cross(heading,basis))*std::sin(a)+Vec3{0,tilt,0});
        Vec3 origin=tip+dir*forestRnd(rng,.03f,.17f);
        float length=(conifer?.78f:1.07f)*scale*forestRnd(rng,.76f,1.15f);
        float width=(conifer?.16f:.36f)*length;
        int cell=(sprigIndex*7+i*5+species*3)%16;
        forestLeaf(hi,origin,dir,length,width,cell,
                   forestRnd(rng,-.11f,.14f),conifer?3:4);
        if(i==0 || (sprigIndex%3==0 && i==1)){
            forestLeaf(mid,origin,dir,length*.95f,width*1.06f,cell,
                       .09f,2);
        }
    }
}
static void forestFarCrown(ForestTreeMesh &result,
                           const std::vector<Vec3>& tips,bool conifer,float height){
    if(conifer){
        for(int layer=0;layer<4;layer++){
            float y=height*(.24f+.16f*layer);
            float top=height*(.66f+.115f*layer);
            float r=(1.f-.19f*layer)*2.25f;
            uint32_t index=uint32_t(result.foliageFar.v.size());
            for(int s=0;s<=10;s++){
                float a=float(s)*2.f*PI/10.f;
                Vec3 n=normalize({std::cos(a),r/(top-y),std::sin(a)});
                result.foliageFar.v.push_back({r*std::cos(a),y,r*std::sin(a),
                    n.x,n.y,n.z,float(s)/10.f,1.f});
                result.foliageFar.v.push_back({0,top,0,n.x,n.y,n.z,float(s)/10.f,0.f});
                if(s<10){
                    uint32_t k=index+uint32_t(s*2);
                    result.foliageFar.triangle(k,k+2,k+1);
                }
            }
        }
    }else{
        // This is only the *distant impostor* silhouette, never close tree geometry.
        for(size_t i=0;i<tips.size();i+=3){
            Vec3 p=tips[i];
            ellipsoid(result.foliageFar,p+Vec3{0,.28f,0},
                {1.05f,1.15f,1.0f},5,7);
        }
    }
}
static ForestTreeMesh forestBuildTree(int species){
    ForestTreeMesh result;
    bool conifer=species>=3;
    std::mt19937 rng(0xC0FEEF00u+uint32_t(species)*32771u);
    const float height=(conifer?11.f:8.4f)+forestRnd(rng,-.65f,1.45f);
    const float trunkR=conifer?.37f:.44f;
    std::vector<Vec3> stem{
        {0,0,0},{forestRnd(rng,-.08f,.08f),height*.23f,0},
        {forestRnd(rng,-.18f,.18f),height*.51f,forestRnd(rng,-.17f,.17f)},
        {forestRnd(rng,-.25f,.25f),height*.78f,forestRnd(rng,-.18f,.18f)},
        {forestRnd(rng,-.27f,.27f),height,forestRnd(rng,-.21f,.21f)}
    };
    forestTube(result.woodHigh,stem,trunkR,.022f,12,4,1.4f);
    forestTube(result.woodMid,stem,trunkR,.025f,7,2,1.3f);
    if(!conifer){
        for(int i=0;i<5;i++){
            float a=float(i)*2.f*PI/5.f+forestRnd(rng,-.16f,.16f);
            Vec3 foot{std::cos(a)*forestRnd(rng,.64f,.91f),-.12f,
                      std::sin(a)*forestRnd(rng,.60f,.95f)};
            forestTube(result.woodHigh,
                {stem[1]*.43f,stem[0]+Vec3{.24f*std::cos(a),.21f,.24f*std::sin(a)},foot},
                .17f,.028f,6,3,1.3f);
        }
    }
    std::vector<Vec3> outerTips;
    int sprig=0;
    if(!conifer){
        const int primary=6+species%3;
        for(int b=0;b<primary;b++){
            const float a=float(b)*2.f*PI/primary+forestRnd(rng,-.30f,.30f);
            const Vec3 direction{std::cos(a),0,std::sin(a)};
            const float h=height*forestRnd(rng,.43f,.72f);
            Vec3 base=forestMix(stem[1],stem[3],(h-height*.23f)/(height*.78f-height*.23f));
            const float reach=forestRnd(rng,2.6f,3.7f);
            Vec3 middle=base+direction*(reach*.42f)+Vec3{0,forestRnd(rng,.52f,1.15f),0};
            Vec3 tip=base+direction*reach+Vec3{0,forestRnd(rng,1.4f,2.7f),0};
            const std::vector<Vec3> branch{base,middle,tip};
            forestTube(result.woodHigh,branch,.17f,.038f,7,4,2.3f);
            forestTube(result.woodMid,branch,.13f,.029f,5,2,2.f);
            outerTips.push_back(tip);
            for(int secondary=0;secondary<3;secondary++){
                const float along=.52f+.16f*secondary;
                Vec3 attach=forestMix(middle,tip,along);
                float twist=a+(secondary==1?-1.2f:1.2f)+forestRnd(rng,-.38f,.38f);
                Vec3 d{std::cos(twist),forestRnd(rng,.18f,.61f),std::sin(twist)};
                Vec3 twig=attach+d*forestRnd(rng,1.05f,2.0f);
                forestTube(result.woodHigh,
                    {attach,forestMix(attach,twig,.58f)+Vec3{0,.17f,0},twig},
                    .062f,.011f,5,2,3.f);
                outerTips.push_back(twig);
                for(int leaves=0;leaves<5;leaves++){
                    Vec3 p=twig+Vec3{forestRnd(rng,-.60f,.60f),forestRnd(rng,-.50f,.60f),
                                    forestRnd(rng,-.60f,.60f)};
                    forestSpray(result.foliageHigh,result.foliageMid,p,
                        normalize(twig-attach+Vec3{0,.36f,0}),
                        forestRnd(rng,.58f,1.0f),species,rng,sprig++,false);
                }
            }
            for(int leaves=0;leaves<5;leaves++){
                Vec3 p=tip+Vec3{forestRnd(rng,-.85f,.85f),forestRnd(rng,-.65f,.65f),
                                forestRnd(rng,-.85f,.85f)};
                forestSpray(result.foliageHigh,result.foliageMid,p,
                    normalize(tip-base),forestRnd(rng,.68f,1.15f),species,rng,sprig++,false);
            }
        }
        for(int j=0;j<8;j++){
            Vec3 top=stem.back()+Vec3{forestRnd(rng,-1.1f,1.1f),
                forestRnd(rng,-.25f,.80f),forestRnd(rng,-1.1f,1.1f)};
            forestSpray(result.foliageHigh,result.foliageMid,top,{0,1,0},
                forestRnd(rng,.63f,.92f),species,rng,sprig++,false);
        }
    }else{
        const int layers=10;
        for(int layer=0;layer<layers;layer++){
            float t=float(layer)/layers;
            float level=height*(.19f+t*.71f);
            float radius=(1.f-t)*forestRnd(rng,2.5f,3.4f)+.15f;
            int spokes=5+(layer%2);
            for(int spoke=0;spoke<spokes;spoke++){
                float angle=2.f*PI*(float(spoke)+.35f*(layer%2))/spokes+
                            forestRnd(rng,-.17f,.17f);
                Vec3 dir{std::cos(angle),0,std::sin(angle)};
                Vec3 base{0,level,0};
                Vec3 middle=base+dir*(radius*.57f)+Vec3{0,-.17f,0};
                Vec3 tip=base+dir*radius+Vec3{0,forestRnd(rng,-.22f,.34f),0};
                forestTube(result.woodHigh,{base,middle,tip},
                           .065f*(1.f-t*.65f),.009f,5,2,2.2f);
                if(spoke%2==0)forestTube(result.woodMid,{base,tip},.055f,.009f,4,1,2.f);
                outerTips.push_back(tip);
                for(int group=0;group<4;group++){
                    Vec3 center=forestMix(middle,tip,(group+.2f)/4.f);
                    forestSpray(result.foliageHigh,result.foliageMid,
                        center+Vec3{forestRnd(rng,-.15f,.15f),
                            forestRnd(rng,-.20f,.20f),forestRnd(rng,-.15f,.15f)},
                        normalize(dir+Vec3{0,.21f,0}),
                        forestRnd(rng,.40f,.73f),species,rng,sprig++,true);
                }
            }
        }
        for(int q=0;q<7;q++){
            Vec3 p=stem.back()+Vec3{forestRnd(rng,-.4f,.4f),
                forestRnd(rng,-.3f,.3f),forestRnd(rng,-.4f,.4f)};
            forestSpray(result.foliageHigh,result.foliageMid,p,{0,1,0},
                .44f,species,rng,sprig++,true);
        }
    }
    forestFarCrown(result,outerTips,conifer,height);
    return result;
}
