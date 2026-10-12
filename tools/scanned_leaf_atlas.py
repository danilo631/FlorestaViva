#!/usr/bin/env python3
"""Generate a 4x4 photographic botanical leaf cluster atlas from ambientCG Leaf001.

Unlike the old synthetic ellipse/canopy masking, this uses *actual scanned leaves*
with their original opacity and normal maps. Each atlas cell contains a different
small sprig, allowing tree mesh UVs to select a biologically convincing variant.

Source: https://ambientcg.com/view?id=Leaf001 (CC0).
Downloaded as Leaf001_2K-PNG.zip from ambientCG. No proprietary game assets.
Requires Pillow + NumPy only. The builder fails if the source is unavailable,
rather than silently reusing procedurally generated blurry assets.
"""
import argparse
import io
import random
import urllib.request
import zipfile
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

SOURCES = (
    "https://ambientcg.com/get?file=Leaf001_2K-PNG.zip",
    "https://ambientcg.com/get?file=LeafSet024_2K-PNG.zip",
)

def fetch_photos(source):
    request = urllib.request.Request(source, headers={"User-Agent":"FlorestaViva-CC0-Foliage/1.0"})
    with urllib.request.urlopen(request, timeout=100) as response:
        archive = zipfile.ZipFile(io.BytesIO(response.read()))
    by_name={Path(name).stem.lower():name for name in archive.namelist()}
    def load(suffix):
        names=[name for stem,name in by_name.items() if stem.endswith("_"+suffix.lower())]
        if not names:raise RuntimeError(f"ambientCG Leaf001 missing {suffix}: {list(by_name)}")
        with archive.open(names[0]) as stream:
            return Image.open(io.BytesIO(stream.read())).convert("RGB")
    color=load("Color")
    alpha=load("Opacity").convert("L")
    normal=load("NormalGL")
    if color.size!=alpha.size or color.size!=normal.size:raise RuntimeError("photo map mismatch")
    if min(color.size)<1000:raise RuntimeError("low-resolution source")
    return color,alpha,normal

def extract_two_leaves(color,opacity,normal):
    width,height=color.size
    out=[]
    for k in range(2):
        bounds=(width*k//2,0,width*(k+1)//2,height)
        c=color.crop(bounds);a=opacity.crop(bounds);n=normal.crop(bounds)
        # Threshold cuts out near-zero opacity fringe and preserve soft AA transition.
        a=a.point(lambda v:0 if v<14 else (v-14)*255//241)
        box=a.getbbox()
        if not box:raise RuntimeError("scanned leaf opacity mask empty")
        inset=4
        box=(max(0,box[0]-inset),max(0,box[1]-inset),
             min(c.width,box[2]+inset),min(c.height,box[3]+inset))
        c=c.crop(box).convert("RGBA");a=a.crop(box);n=n.crop(box).convert("RGBA")
        c.putalpha(a);n.putalpha(a)
        out.append((c,n))
    return out

def extract_grid_leaves(color,opacity,normal,rows=3,cols=3):
    width,height=color.size
    if width!=height:raise RuntimeError("LeafSet024 grid must be square")
    items=[]
    for r in range(rows):
        for cidx in range(cols):
            bounds=(width*cidx//cols,height*r//rows,
                    width*(cidx+1)//cols,height*(r+1)//rows)
            co=color.crop(bounds)
            al=opacity.crop(bounds)
            no=normal.crop(bounds)
            al=al.point(lambda v:0 if v<14 else (v-14)*255//241)
            box=al.getbbox()
            if not box:continue
            margin=3
            box=(max(0,box[0]-margin),max(0,box[1]-margin),
                 min(co.width,box[2]+margin),min(co.height,box[3]+margin))
            co=co.crop(box).convert("RGBA")
            no=no.crop(box).convert("RGBA")
            alpha=al.crop(box)
            co.putalpha(alpha)
            no.putalpha(alpha)
            items.append((co,no))
    if len(items)<7:raise RuntimeError(f"Expected at least 7 distinct scanned leaves, got {len(items)}")
    return items

def rotated(pic, degrees, new_size):
    img=pic.resize(new_size,Image.Resampling.LANCZOS)
    return img.rotate(degrees,Image.Resampling.BICUBIC,expand=True)

def bake(assets:Path, size:int=2048):
    if size%4:raise ValueError("Size must be divisible by 4")
    baseColor,baseOpacity,baseNormal=fetch_photos(SOURCES[0])
    setColor,setOpacity,setNormal=fetch_photos(SOURCES[1])
    photos=(extract_two_leaves(baseColor,baseOpacity,baseNormal)+
            extract_grid_leaves(setColor,setOpacity,setNormal))
    atlas=Image.new("RGBA",(size,size),(0,0,0,0))
    normals=Image.new("RGBA",(size,size),(128,128,255,0))
    tile=size//4
    for cell in range(16):
        rng=random.Random(1337+cell*197)
        cTile=Image.new("RGBA",(tile,tile))
        nTile=Image.new("RGBA",(tile,tile),(128,128,255,0))
        stems=ImageDraw.Draw(cTile)
        branches=2+cell%3
        # Woody twigs tie leaves together in three-dimensional cards.
        root=(tile*.47+tile*.08*rng.random(),tile*.94)
        for b in range(branches):
            tip=(tile*rng.uniform(.21,.82),tile*rng.uniform(.08,.42))
            stems.line([root,tip],fill=(79,72,33,225),width=max(2,tile//105))
        for b in range(4+cell%3):
            leafID=(b*7+cell*3)%len(photos)
            c, n=photos[leafID]
            leafh=int(tile*rng.uniform(.33,.55))
            leafw=max(6,int(c.width/c.height*leafh))
            rot=rng.uniform(-53,53)
            lc=rotated(c,rot,(leafw,leafh))
            ln=rotated(n,rot,(leafw,leafh))
            tx=int(tile*(.10+.80*(b+.36)/(4+cell%3)))
            ty=int(tile*rng.uniform(.08,.42))
            x=max(-lc.width//4,min(tile-lc.width*3//4,tx-lc.width//2))
            y=max(-lc.height//4,min(tile-lc.height*3//4,ty))
            if (cell+b)%4==0:
                a=lc.getchannel("A")
                lc=ImageEnhance.Color(lc).enhance(0.83)
                lc.putalpha(a)
            cTile.alpha_composite(lc,(x,y))
            nTile.alpha_composite(ln,(x,y))
        # Use 4-pixel guard band; card UVs already stay within each cell.
        atlas.alpha_composite(cTile,(cell%4*tile,cell//4*tile))
        normals.alpha_composite(nTile,(cell%4*tile,cell//4*tile))
    atlas.save(assets/"leaves_atlas.png",optimize=True)
    normals.save(assets/"leaves_normal.png",optimize=True)
    assert atlas.getchannel("A").getbbox()
    (assets/"leaf_asset_source.txt").write_text(
        "Leaf001 + LeafSet024 (ambientCG), Color/NormalGL/Opacity, CC0 1.0.\n"
        "https://ambientcg.com/view?id=Leaf001\n"
        "https://ambientcg.com/view?id=LeafSet024\n"
        f"Atlas {size}x{size}, 16 photographed leaf cluster variants.\n")
    print("Baked REAL SCANNED LEAVES",len(photos),"specimens",atlas.size,"from",SOURCES)

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    parser.add_argument("--size",type=int,default=2048)
    a=parser.parse_args()
    bake(a.assets,a.size)
