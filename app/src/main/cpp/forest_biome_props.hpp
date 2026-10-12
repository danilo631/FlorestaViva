#pragma once
// Biome props generated as real low-to-mid polygon meshes, not stickers.
static ModelData makeRiverReeds(){
    ModelData m;
    std::mt19937 rng(49200u);
    for(int i=0;i<12;i++){
        const float ang=2.f*PI*i/12.f+forestRnd(rng,-.14f,.14f);
        const float rad=forestRnd(rng,.04f,.60f);
        const float h=forestRnd(rng,1.17f,2.15f);
        Vec3 base{rad*std::cos(ang),0,rad*std::sin(ang)};
        Vec3 top=base+Vec3{forestRnd(rng,-.19f,.19f),h,
                           forestRnd(rng,-.19f,.19f)};
        forestTube(m,{base,base+Vec3{0,h*.56f,0},top},.022f,.005f,5,2,3.f);
        for(int j=0;j<3;j++){
            float angle=ang+float(j)*PI*.67f;
            Vec3 direction=normalize({std::cos(angle)*.74f,.62f,std::sin(angle)*.74f});
            forestLeaf(m,base+Vec3{0,h*(.29f+.17f*j),0},
                       direction,.53f,.044f,(i+j*3)%16,.09f,2);
        }
    }
    return m;
}
static ModelData makeMeadowFlowers(){
    ModelData m;
    std::mt19937 rng(567883u);
    for(int j=0;j<9;j++){
        const float ang=2*PI*j/9+forestRnd(rng,-.2f,.2f);
        const float dist=forestRnd(rng,.08f,.55f);
        const float ht=forestRnd(rng,.43f,.81f);
        const Vec3 origin{std::cos(ang)*dist,.025f,std::sin(ang)*dist};
        const Vec3 top=origin+Vec3{forestRnd(rng,-.09f,.09f),ht,forestRnd(rng,-.08f,.08f)};
        forestTube(m,{origin,top},.022f,.005f,4,2,2.f);
        for(int petal=0;petal<5;petal++){
            const float theta=float(petal)*2*PI/5.f;
            Vec3 dir=normalize({std::cos(theta)*.82f,.51f,std::sin(theta)*.82f});
            forestLeaf(m,top,dir,.19f,.085f,(j+petal)%16,.05f,2);
        }
    }
    return m;
}
