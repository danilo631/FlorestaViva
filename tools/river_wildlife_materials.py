"""Deterministic tileable water, streambed, wildlife fur and flower resources.

Sources for ground/tree remain licensed 2K photographs (Poly Haven);
this module bakes procedural SUPPORTING detail in one Python build step.
No runtime dependence on Pillow, NumPy or Java graphics.
"""
import argparse
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter

def periodic_noise(rng,size,period):
    grid=rng.integers(0,256,(period,period),dtype=np.uint8)
    sheet=np.tile(grid,(3,3))
    return np.asarray(Image.fromarray(sheet).resize((size*3,size*3),
                    Image.Resampling.BICUBIC).crop((size,size,size*2,size*2)),np.float32)/255.

def bake(path:Path):
    size=1024
    rng=np.random.default_rng(631204)
    y,x=np.mgrid[:size,:size].astype(np.float32)
    x=x*2*np.pi/size
    y=y*2*np.pi/size
    # Tileable small-scale ripples with waves at two intersecting frequencies.
    wave=(.29*np.sin(x*20+y*6)+.17*np.sin(x*9-y*15)+
          .12*np.sin(x*43+y*9)+.07*np.sin(x*16-y*38))
    dx=(np.roll(wave,-1,axis=1)-np.roll(wave,1,axis=1))*5.5
    dy=(np.roll(wave,-1,axis=0)-np.roll(wave,1,axis=0))*5.5
    normal=np.stack((-dx,-dy,np.ones_like(dx)),axis=-1)
    normal/=np.maximum(np.linalg.norm(normal,axis=-1,keepdims=True),1e-6)
    Image.fromarray(np.uint8(np.clip((normal*.5+.5)*255,0,255)),"RGB").save(path/"river_normal.png",optimize=True)
    # Stone/gravel bed with small pebbles and sediment color.
    n=(periodic_noise(rng,size,35)*.63+periodic_noise(rng,size,120)*.26+
       periodic_noise(rng,size,300)*.11)
    pebble=np.sin(x*65+np.sin(y*31)) * np.sin(y*76+np.sin(x*29))
    rocks=n*.75+np.clip(pebble,0,1)*.25
    rgb=np.stack((rocks*.51+.15,rocks*.57+.20,rocks*.50+.19),axis=-1)
    Image.fromarray(np.uint8(np.clip(rgb*255,0,255)),"RGB").save(path/"riverbed.png",optimize=True)
    # Atlas of deer, boar, fox fur: 3 horizontal strips with subtle strand noise.
    fur=np.zeros((512,1536,3),dtype=np.uint8)
    grain=np.asarray(Image.fromarray(rng.integers(0,256,(512,1536),dtype=np.uint8))
         .filter(ImageFilter.GaussianBlur(1.15)),np.float32)/255.
    horizontal=np.sin(np.linspace(0,2*np.pi*128,1536))[None,:]*.05
    for sp,(base,variation) in enumerate([
        ((145,112,70),(.28,.23,.18)),
        ((90,83,72),(.18,.17,.16)),
        ((168,85,44),(.22,.19,.14))]):
        start=sp*512
        g=np.clip((grain[:,start:start+512]-.5)*1.2+
                  horizontal[:,start:start+512],-.5,.5)
        for ch in range(3):
            fur[:,start:start+512,ch]=np.uint8(np.clip(base[ch]*
                            (1+g*variation[ch]),0,255))
    Image.fromarray(fur,"RGB").save(path/"wildlife_fur.png",optimize=True)
    fur_normal=np.full((512,1536,3),128,np.uint8)
    fur_normal[:,:,2]=250
    Image.fromarray(fur_normal,"RGB").save(path/"wildlife_fur_normal.png",optimize=True)
    # Flower petals are individual curved polygons; shader samples this color.
    flower=np.zeros((512,512,4),np.uint8)
    palette=np.array([(241,211,79,255),(225,226,217,255),
                      (162,153,211,255),(238,155,178,255)],np.uint8)
    for py in range(2):
        for px in range(2):
            flower[py*256:(py+1)*256,px*256:(px+1)*256]=palette[py*2+px]
    Image.fromarray(flower,"RGBA").save(path/"wildflowers.png",optimize=True)
    print("Generated river PBR-like normal/stone maps, three-species fur and flowers")
if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    args=parser.parse_args()
    bake(args.assets)
