#pragma once
// Native undergrowth meshes share the same Vertex/ModelData format as the
// trees, so low-memory mobile GPUs can instance hundreds of them cheaply.
struct ForestUnderstoryMesh {
    ModelData twig;
    ModelData shrub;
    ModelData fern;
    ModelData broadGroundPlant;
};
static ForestUnderstoryMesh buildForestUnderstory(){
    ForestUnderstoryMesh out;
    std::mt19937 rng(883109u);
    // Shrub woody architecture: actual tapered main stems and secondary
    // twig forks, not crossed rectangles wearing a bush PNG.
    for(int stem=0;stem<9;stem++){
        float a=2.f*PI*stem/9.f+forestRnd(rng,-.25f,.25f);
        Vec3 dir{std::cos(a),0,std::sin(a)};
        float h=forestRnd(rng,1.0f,1.75f);
        Vec3 base{forestRnd(rng,-.14f,.14f),0,forestRnd(rng,-.14f,.14f)};
        Vec3 middle=base+dir*.28f+Vec3{0,h*.55f,0};
        Vec3 end=base+dir*forestRnd(rng,.55f,.95f)+Vec3{0,h,0};
        forestTube(out.twig,{base,middle,end},.055f,.007f,5,2,2.7f);
        for(int branch=0;branch<3;branch++){
            float pos=.35f+.24f*branch;
            Vec3 origin=forestMix(middle,end,pos);
            float angle=a+(branch%2?1.f:-1.f)*forestRnd(rng,.45f,1.1f);
            Vec3 tip=origin+Vec3{std::cos(angle)*.40f,forestRnd(rng,.20f,.54f),
                                std::sin(angle)*.40f};
            forestTube(out.twig,{origin,tip},.021f,.003f,4,1,3.f);
            for(int leaf=0;leaf<5;leaf++){
                Vec3 center=tip+Vec3{forestRnd(rng,-.19f,.19f),forestRnd(rng,-.18f,.15f),
                                     forestRnd(rng,-.19f,.19f)};
                Vec3 direction=normalize((center-origin)+
                                   Vec3{forestRnd(rng,-.26f,.26f),.3f,
                                        forestRnd(rng,-.26f,.26f)});
                forestLeaf(out.shrub,center,direction,forestRnd(rng,.20f,.42f),
                    forestRnd(rng,.08f,.13f),(stem*7+branch*5+leaf)%16,
                    forestRnd(rng,-.16f,.16f),2);
            }
        }
    }
    // Fern fronds have true pinnate leaves placed along a 3D stem:
    // oscillating curvature and a visible dark centre.
    for(int frond=0;frond<9;frond++){
        float a=2.f*PI*frond/9.f+forestRnd(rng,-.19f,.19f);
        Vec3 radial{std::cos(a),0,std::sin(a)};
        float h=forestRnd(rng,.73f,1.37f);
        Vec3 origin{0,.045f,0};
        Vec3 tip=radial*forestRnd(rng,.48f,.90f)+Vec3{0,h,0};
        Vec3 mid=forestMix(origin,tip,.60f)+Vec3{0,.20f,0};
        forestTube(out.twig,{origin,mid,tip},.009f,.002f,4,2,2.f);
        Vec3 across=normalize(cross(radial,Vec3{0,1,0}));
        for(int segment=0;segment<9;segment++){
            float t=(segment+.4f)/9.5f;
            Vec3 center=forestMix(mid,tip,std::min(1.f,t));
            if(t<.57f)center=forestMix(origin,mid,t/.57f);
            float span=forestRnd(rng,.17f,.22f)*std::pow(1.f-t,.65f);
            for(int sign:{-1,1}){
                Vec3 leafDir=normalize(across*float(sign)*.78f+
                    radial*.22f+Vec3{0,.27f,0});
                Vec3 offset=center+leafDir*.028f;
                forestLeaf(out.fern,offset,leafDir,span*forestRnd(rng,.83f,1.10f),
                    span*.25f,(frond*3+segment+sign+16)%16,.06f,2);
            }
        }
    }
    // Broadleaf seedlings and wild plants create dense layer near eye/feet.
    for(int branch=0;branch<11;branch++){
        float a=2.f*PI*float(branch)/11.f+forestRnd(rng,-.22f,.22f);
        Vec3 d{std::cos(a),0,std::sin(a)};
        float len=forestRnd(rng,.35f,.73f);
        Vec3 p=d*.09f+Vec3{0,.025f,0};
        Vec3 direction=normalize(d*.6f+Vec3{0,.88f,0});
        forestLeaf(out.broadGroundPlant,p,direction,len,len*.33f,
            (branch*5+7)%16,forestRnd(rng,-.12f,.14f),3);
    }
    return out;
}
