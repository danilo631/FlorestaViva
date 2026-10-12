#pragma once
// Original low-cost meshes to complete the forest floor (layer 4 of the PCG).
// Reuses already licensed and batched bark/rock materials: no new bitmap slots.
struct ForestFloorDetails {
    ModelData dryTwigs;
    ModelData fungiStems;
    ModelData fungiCaps;
};
static ForestFloorDetails forestBuildFloorDetails(){
    ForestFloorDetails out;
    std::mt19937 rng(0xA51C7E4u);
    // Woody detritus: many split, curved twigs that cast correct 3D silhouettes.
    for(int i=0;i<9;i++){
        float a=forestRnd(rng,0.f,2.f*PI);
        float d=forestRnd(rng,.08f,.53f);
        Vec3 start{std::cos(a)*d,.034f,std::sin(a)*d};
        Vec3 middle=start+Vec3{std::cos(a)*.18f,.045f,std::sin(a)*.18f};
        Vec3 end=middle+Vec3{std::cos(a+.25f)*forestRnd(rng,.11f,.34f),
                             .024f,std::sin(a+.25f)*forestRnd(rng,.11f,.34f)};
        forestTube(out.dryTwigs,{start,middle,end},.035f,.004f,5,2,3.f);
        if(i%3==0){
            Vec3 split=end+Vec3{std::cos(a+1.15f)*.20f,.06f,std::sin(a+1.15f)*.20f};
            forestTube(out.dryTwigs,{middle,split},.018f,.002f,4,2,4.f);
        }
    }
    // Small fungi are short cylindrical stalks with convex caps. Their parts
    // can be instanced independently with earthy wood / stone materials.
    for(int i=0;i<7;i++){
        float a=forestRnd(rng,0.f,2.f*PI);
        float r=forestRnd(rng,.04f,.37f);
        float h=forestRnd(rng,.13f,.27f);
        Vec3 p{r*std::cos(a),.01f,r*std::sin(a)};
        forestTube(out.fungiStems,{p,p+Vec3{0,h,0}},.030f,.020f,7,2,4.f);
        ellipsoid(out.fungiCaps,p+Vec3{0,h+.012f,0},
                  {forestRnd(rng,.09f,.16f),.07f,forestRnd(rng,.09f,.16f)},7,10);
    }
    return out;
}
