"""Offline material baking for C++ forest renderer.
Outputs a pine-needle atlas, realistic-textured pine bark variant,
and granite-like rock albedo/normal, without extra per-frame CPU work.
"""
from pathlib import Path
import argparse, math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

def normal_map(img: Image.Image, strength: float=3.5, side: int=512):
    gray=img.convert("L").resize((side,side),Image.Resampling.LANCZOS)
    h=np.asarray(gray.filter(ImageFilter.GaussianBlur(.65)),np.float32)/255.
    dx=(np.roll(h,-1,axis=1)-np.roll(h,1,axis=1))*strength
    dy=(np.roll(h,-1,axis=0)-np.roll(h,1,axis=0))*strength
    n=np.stack((-dx,-dy,np.ones_like(dx)),axis=-1)
    n/=np.maximum(np.linalg.norm(n,axis=-1,keepdims=True),1e-5)
    return Image.fromarray(np.uint8(np.clip((n*.5+.5)*255,0,255)),"RGB")

def create_pine(root: Path, size: int=1024):
    cell=size//4
    sheet=Image.new("RGBA",(size,size),(0,0,0,0))
    nsheet=Image.new("RGBA",(size,size),(128,128,255,0))
    colors=np.asarray(Image.open(root/"canopy.png").convert("RGB").resize((128,128)),np.uint8)
    for idx in range(16):
        rng=random.Random(idx*991+77)
        layer=Image.new("RGBA",(cell,cell))
        bump=Image.new("L",(cell,cell),127)
        draw=ImageDraw.Draw(layer)
        height=ImageDraw.Draw(bump)
        for twig in range(4+(idx%4)):
            a=2*math.pi*(twig+.3*rng.random())/(4+idx%4)
            base=(cell*.46+rng.uniform(-cell*.10,cell*.10),
                  cell*.63+rng.uniform(-cell*.12,cell*.12))
            long=cell*rng.uniform(.27,.44)
            end=(base[0]+math.cos(a)*long,base[1]+math.sin(a)*long)
            c=np.asarray(colors[rng.randrange(128),rng.randrange(128)],np.int32)
            hue=(int(max(15,c[0]*.60)),int(max(30,c[1]*.83)),int(max(17,c[2]*.63)),255)
            draw.line([base,end],fill=(77,74,34,255),width=2)
            height.line([base,end],fill=182,width=2)
            for needle in range(4,26):
                t=needle/27.
                px=base[0]+(end[0]-base[0])*t
                py=base[1]+(end[1]-base[1])*t
                reach=cell*rng.uniform(.035,.105)*(1.-t*.68)
                angle=a+math.pi/2.
                for sign in (-1,1):
                    x=px+math.cos(angle)*reach*sign+math.cos(a)*reach*.4
                    y=py+math.sin(angle)*reach*sign+math.sin(a)*reach*.4
                    col=(min(255,int(hue[0]*rng.uniform(.85,1.17))),
                         min(255,int(hue[1]*rng.uniform(.85,1.22))),
                         min(255,int(hue[2]*rng.uniform(.86,1.14))),255)
                    draw.line([(px,py),(x,y)],fill=col,width=rng.choice((1,2,2,3)))
                    height.line([(px,py),(x,y)],fill=201,width=2)
        h=np.asarray(bump.filter(ImageFilter.GaussianBlur(.7)),np.float32)/255.
        dx=(np.roll(h,-1,axis=1)-np.roll(h,1,axis=1))*2.0
        dy=(np.roll(h,-1,axis=0)-np.roll(h,1,axis=0))*2.0
        nn=np.stack((-dx,-dy,np.ones_like(dx)),axis=-1)
        nn/=np.maximum(np.linalg.norm(nn,axis=-1,keepdims=True),1e-6)
        rgb=np.uint8(np.clip((nn*.5+.5)*255,0,255))
        offset=(idx%4*cell,idx//4*cell)
        sheet.paste(layer,offset)
        nsheet.paste(Image.fromarray(np.dstack((rgb,np.asarray(layer.getchannel("A")))),"RGBA"),offset)
    sheet.save(root/"pine_atlas.png",optimize=True)
    nsheet.save(root/"pine_normal.png",optimize=True)

def rock_surface(side: int):
    rng=np.random.default_rng(418)
    x,y=np.meshgrid(np.arange(side),np.arange(side))
    noise=np.zeros((side,side),np.float32)
    for n,w in ((8,1.0),(18,.58),(40,.32),(96,.22),(256,.12)):
        coarse=np.uint8(rng.integers(0,256,(n,n)))
        smooth=np.asarray(Image.fromarray(coarse,"L").resize((side,side),Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(max(0.45,side/(n*4)))) ,np.float32)/255.
        noise+=(smooth-.5)*w
    crack=np.sin(x*.045+np.sin(y*.018)*2.2)*np.cos(y*.057+np.sin(x*.026)*1.2)
    shape=np.clip(noise*.62+crack*.065,-.37,.37)
    base=np.empty((side,side,3),np.float32)
    base[:,:,0]=103+shape*95
    base[:,:,1]=104+shape*103
    base[:,:,2]=99+shape*97
    veins=(crack<-.91).astype(np.float32)*22.
    base-=veins[:,:,None]
    return Image.fromarray(np.uint8(np.clip(base,5,235)),"RGB")

def create_assets(root: Path):
    # Real CC0 PBR photos are shipped in Git, checked by SHA256 and refreshed
    # by tools/fetch_assets.py. Never replace rocks or bark with synthetic noise.
    for name in ("pine_bark.png","pine_bark_normal.png","rock.png","rock_normal.png"):
        if not (root/name).is_file():
            raise RuntimeError("Missing pinned photographic material: "+name)
    create_pine(root)
    for name in ("pine_atlas.png","pine_normal.png"):
        print("baked legacy pine cards",root/name)

if __name__=="__main__":
    cli=argparse.ArgumentParser()
    cli.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    args=cli.parse_args()
    create_assets(args.assets)
