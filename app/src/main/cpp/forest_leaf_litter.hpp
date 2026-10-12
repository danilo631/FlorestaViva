#pragma once
// Physical 3D leaf litter mesh. Instanced in small scattered patches,
// not a ground decal or a flat 2D wallpaper. Reuses CC0 leaf atlas UVs.
static ModelData forestLeafLitterPatch(){
    ModelData leaves;
    std::mt19937 rng(0x1EAFA118u);
    for(int i=0;i<11;i++){
        float angle=forestRnd(rng,0.f,2.f*PI);
        float radial=forestRnd(rng,.05f,.63f);
        Vec3 p{radial*std::cos(angle),.012f+forestRnd(rng,.001f,.018f),
               radial*std::sin(angle)};
        float heading=forestRnd(rng,0.f,2.f*PI);
        Vec3 dir=normalize({std::cos(heading),forestRnd(rng,.008f,.09f),
                            std::sin(heading)});
        float scale=forestRnd(rng,.16f,.33f);
        float width=scale*forestRnd(rng,.24f,.35f);
        forestLeaf(leaves,p,dir,scale,width,(i*7+3)%16,
                   forestRnd(rng,-.04f,.08f),3);
    }
    return leaves;
}
