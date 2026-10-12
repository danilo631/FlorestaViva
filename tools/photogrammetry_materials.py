"""Fetch licensed, photographic PBR materials from Poly Haven's public CDN.

CC0 assets: forrest_ground_01, forest_floor, bark_brown_01, pine_bark.
Run AFTER all procedural atlas/material generators; these replace ONLY
four terrain/wood material families and their matching normal maps.

Offline builds can use the already committed procedural PNGs by omitting
this stage; production GitHub CI uses --strict to avoid silent downgrades.
"""
import argparse
from pathlib import Path
import io
import urllib.request
import time
from PIL import Image, ImageOps

ASSETS={
    "forrest_ground_01":("grass.png","grass_normal.png","grass_rough.png"),
    "forest_floor":("soil.png","soil_normal.png","soil_rough.png"),
    "bark_brown_01":("bark.png","bark_normal.png","bark_rough.png"),
    "pine_bark":("pine_bark.png","pine_bark_normal.png","pine_bark_rough.png")
}
CDN="https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k"

def fetch(asset:str,texture_type:str):
    url=f"{CDN}/{asset}/{asset}_{texture_type}_2k.jpg"
    error=None
    for attempt in range(3):
        try:
            req=urllib.request.Request(url,headers={"User-Agent":"FlorestaViva-CC0-AssetBake/1.0"})
            with urllib.request.urlopen(req,timeout=55) as response:
                data=response.read()
            image=Image.open(io.BytesIO(data)).convert("RGB")
            image.load()
            if image.width<1800 or image.height<1800:
                raise ValueError(f"unexpected image dimensions: {image.size}")
            return image
        except Exception as exc:
            error=exc
            print(f"Download attempt {attempt+1}: {url} -> {error}",flush=True)
            if attempt<2: time.sleep(2*(attempt+1))
    raise RuntimeError(f"Poly Haven asset unavailable {asset}/{texture_type}: {error}")

def bake(root:Path,strict:bool):
    success=0
    for slug,(color,normal,rough) in ASSETS.items():
        try:
            colorImage=fetch(slug,"diff")
            normalImage=fetch(slug,"nor_gl")
            roughImage=fetch(slug,"rough")
            # Save files atomically only after BOTH source maps have validated.
            for output,image in ((color,colorImage),(normal,normalImage),(rough,roughImage)):
                path=root/output
                tmp=root/(output+".tmp")
                image.save(tmp,format="PNG",optimize=True)
                tmp.replace(path)
            success+=1
            print(f"CC0 PHOTOGRAMMETRY {slug}: {colorImage.size}, {color}, {normal}, {rough}",flush=True)
        except Exception as exc:
            print("ERROR:",exc,flush=True)
            if strict: raise
    (root/"photogrammetry_manifest.txt").write_text(
        "Poly Haven CC0 materials: "+", ".join(ASSETS)+
        f"\nSuccessfully downloaded families: {success}/{len(ASSETS)}\n"+
        "https://polyhaven.com/license\n")
    if strict and success<len(ASSETS):
        raise RuntimeError("Incomplete photographic PBR material set")
    print("Real photographed material families:",success,"/",len(ASSETS))

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    p.add_argument("--strict",action="store_true")
    args=p.parse_args()
    bake(args.assets,args.strict)
