#pragma once
// Lightweight original animated fauna geometry, built and instanced once.
// Anatomical approximation (not photogrammetry or skeletal AAA animation).
static ModelData forestBuildDeer(){
    ModelData m;
    ellipsoid(m,{0,1.20f,0},{.50f,.43f,.92f},9,14);
    ellipsoid(m,{0,1.46f,.68f},{.30f,.40f,.23f},8,12);
    ellipsoid(m,{0,1.76f,.86f},{.25f,.19f,.38f},8,12);
    ellipsoid(m,{0,1.68f,1.20f},{.18f,.13f,.19f},6,10);
    for(float sx:{-1.f,1.f}){
        for(float z:{-.58f,.58f}){
            const Vec3 hip{sx*.32f,1.04f,z};
            const Vec3 knee{sx*.39f,.52f,z+(z>0?.06f:-.1f)};
            const Vec3 foot{sx*.41f,.06f,z+.07f};
            forestTube(m,{hip,knee,foot},.102f,.043f,7,3,2.0f);
            ellipsoid(m,foot,{.075f,.085f,.11f},5,8);
        }
        // Ears and antlers are separate branches with real cylinder geometry.
        ellipsoid(m,{sx*.25f,1.91f,.80f},{.15f,.10f,.20f},5,9);
        Vec3 a{sx*.18f,1.91f,.82f};
        Vec3 b{sx*.33f,2.40f,.77f};
        Vec3 c{sx*.61f,2.60f,.69f};
        forestTube(m,{a,b,c},.055f,.015f,5,3,2.f);
        forestTube(m,{b,{sx*.43f,2.65f,.97f}},.028f,.009f,4,2,2.f);
        forestTube(m,{c,{sx*.75f,2.91f,.74f}},.021f,.008f,4,2,2.f);
    }
    forestTube(m,{{0,1.32f,-.78f},{0,1.39f,-1.03f}},.09f,.025f,6,2,2.f);
    return m;
}
static ModelData forestBuildBoar(){
    ModelData m;
    ellipsoid(m,{0,.80f,0},{.60f,.59f,.96f},10,16);
    ellipsoid(m,{0,1.0f,.65f},{.50f,.57f,.54f},8,13);
    ellipsoid(m,{0,.74f,1.05f},{.30f,.26f,.39f},8,12);
    ellipsoid(m,{0,.72f,1.38f},{.25f,.17f,.10f},5,10);
    for(float sx:{-1.f,1.f}){
        for(float z:{-.62f,.58f}){
            forestTube(m,{{sx*.36f,.67f,z},{sx*.40f,.24f,z},
                      {sx*.41f,.04f,z+.045f}},.13f,.07f,7,3,1.8f);
        }
        ellipsoid(m,{sx*.39f,1.35f,.78f},{.18f,.20f,.14f},5,9);
        forestTube(m,{{sx*.25f,.71f,1.14f},{sx*.32f,.81f,1.40f},
                     {sx*.39f,.96f,1.42f}},.045f,.003f,5,3,2.f);
    }
    return m;
}
static ModelData forestBuildFox(){
    ModelData m;
    ellipsoid(m,{0,.61f,0},{.29f,.28f,.81f},9,13);
    ellipsoid(m,{0,.67f,.64f},{.25f,.26f,.29f},7,11);
    ellipsoid(m,{0,.64f,.95f},{.15f,.12f,.26f},6,10);
    for(float sx:{-1.f,1.f}){
        for(float z:{-.51f,.50f}){
            forestTube(m,{{sx*.20f,.51f,z},{sx*.24f,.24f,z},
                       {sx*.23f,.045f,z+.06f}},.065f,.032f,6,2,2.f);
        }
        // Triangular ear silhouette, sculpted as a thin elongated ellipsoid.
        ellipsoid(m,{sx*.18f,1.02f,.75f},{.095f,.22f,.105f},5,7);
    }
    // Bushy tapered tail made of offset ellipsoidal sections.
    ellipsoid(m,{0,.68f,-.91f},{.27f,.27f,.40f},6,10);
    ellipsoid(m,{0,.78f,-1.29f},{.32f,.31f,.49f},7,12);
    ellipsoid(m,{0,.88f,-1.62f},{.18f,.19f,.29f},6,9);
    return m;
}
struct ForestAnimal {
    float x=0,z=0,homeX=0,homeZ=0,angle=0,phase=0,moveSpeed=0;
    int species=0;
    bool fleeing=false;
};
