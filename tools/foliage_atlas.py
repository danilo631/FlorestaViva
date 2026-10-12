"""Generate a packed 4x4 leaf-card atlas and normal atlas from canopy albedo."""
import argparse,random,math
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFilter

def bake(root,size):
    src=Image.open(root/"canopy.png").convert("RGB").resize((size,size),Image.Resampling.LANCZOS)
    cell=size//4
    atlas=Image.new("RGBA",(size,size))
    norms=Image.new("RGBA",(size,size),(128,128,255,0))
    for i in range(16):
        r=random.Random(i*9973+13)
        layer=Image.new("RGBA",(cell,cell))
        height=Image.new("L",(cell,cell),128)
        dr=ImageDraw.Draw(layer); dh=ImageDraw.Draw(height)
        for k in range(3+i%3):
            a=k*2*math.pi/(3+i%3)+r.uniform(-.27,.27)
            x=cell*.5+math.cos(a)*r.uniform(0,cell*.12)
            y=cell*.5+math.sin(a)*r.uniform(0,cell*.12)
            reach=r.uniform(.30,.44)*cell
            ex=x+reach*math.cos(a);ey=y+reach*math.sin(a)
            tx,ty=-math.sin(a),math.cos(a)
            w=cell*r.uniform(.11,.18)
            pts=[]
            for s in (1,-1):
                order=range(13) if s==1 else range(12,-1,-1)
                for j in order:
                    t=j/12.
                    thick=(max(0,math.sin(math.pi*t))**.80)*w
                    pts.append((round(x+(ex-x)*t+tx*thick*s),round(y+(ey-y)*t+ty*thick*s)))
            mask=Image.new("L",(cell,cell),0)
            ImageDraw.Draw(mask).polygon(pts,fill=255)
            px=r.randrange(size-cell);py=r.randrange(size-cell)
            patch=src.crop((px,py,px+cell,py+cell)).convert("RGBA")
            patch.putalpha(mask)
            layer.alpha_composite(patch)
            dh.polygon(pts,fill=165)
            dr.line([(round(x),round(y)),(round(ex),round(ey))],fill=(148,181,108,245),width=2)
            dh.line([(round(x),round(y)),(round(ex),round(ey))],fill=232,width=2)
            for j in range(2,10):
                t=j/11.
                mx=x+(ex-x)*t;my=y+(ey-y)*t
                width=w*math.sin(math.pi*t)*.68
                for s in (-1,1):
                    ep=(round(mx+tx*width*s),round(my+ty*width*s))
                    dr.line([(round(mx),round(my)),ep],fill=(42,73,29,120),width=1)
                    dh.line([(round(mx),round(my)),ep],fill=210,width=1)
        h=np.asarray(height.filter(ImageFilter.GaussianBlur(.9)),dtype=np.float32)/255.
        dx=(np.roll(h,-1,axis=1)-np.roll(h,1,axis=1))*3.
        dy=(np.roll(h,-1,axis=0)-np.roll(h,1,axis=0))*3.
        n=np.stack((-dx,-dy,np.ones_like(dx)),axis=-1)
        n/=np.maximum(np.linalg.norm(n,axis=-1,keepdims=True),1e-6)
        rgb=np.uint8(np.clip((n*.5+.5)*255,0,255))
        nm=Image.fromarray(np.dstack((rgb,np.asarray(layer.getchannel("A")))),"RGBA")
        at=((i%4)*cell,(i//4)*cell)
        atlas.paste(layer,at)
        norms.paste(nm,at)
    atlas.save(root/"leaves_atlas.png",optimize=True)
    norms.save(root/"leaves_normal.png",optimize=True)
    print("Generated atlas and normals:",size,"x",size)

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    p.add_argument("--size",type=int,default=1024)
    args=p.parse_args()
    bake(args.assets,args.size)
