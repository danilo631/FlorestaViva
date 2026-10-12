"""Build-time licensed photogrammetric mossy rock material (Poly Haven CC0).

The Godot-4-forest-benchmark references the rock_moss_set_02 MODEL.
We instead use the independent Mossy Rock TEXTURE to avoid depending on
proprietary model-conversion pipelines and ensure simple stable URL paths.
We download the original CC0 maps independently, never copy Godot project assets.
https://polyhaven.com/a/mossy_rock
"""
from pathlib import Path
import argparse
import io
import time
import urllib.request
from PIL import Image

ROOT="https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/mossy_rock"
def fetch(name):
    url=f"{ROOT}/mossy_rock_{name}_2k.jpg"
    error=None
    for attempt in range(3):
        try:
            req=urllib.request.Request(url,headers={"User-Agent":"FlorestaViva-OpenCC0/1.0"})
            with urllib.request.urlopen(req,timeout=60) as response:
                im=Image.open(io.BytesIO(response.read())).convert("RGB")
                im.load()
            if min(im.size)<1800:
                raise RuntimeError(f"unexpected dimensions: {im.size}")
            return im
        except Exception as e:
            error=e
            if attempt<2: time.sleep(2*(attempt+1))
    raise RuntimeError(f"Photographic material unavailable: {name}: {error}")

def bake(path):
    # Fetch/verify all resources before committing any replacement files.
    textures=[(name,fetch(tag)) for name,tag in [
        ("rock.png","diff"),("rock_normal.png","nor_gl"),
        ("rock_rough.png","rough")]]
    for name,image in textures:
        temp=path/(name+".tmp")
        image.save(temp,format="PNG",optimize=True)
        temp.replace(path/name)
    (path/"rock_photo_source.txt").write_text(
        "mossy_rock Poly Haven CC0: diffuse, normal OpenGL, roughness\n"
        "https://polyhaven.com/a/mossy_rock\n")
    print("PHOTO ROCK PBR MAPS",[(n,im.size) for n,im in textures])

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    bake(p.parse_args().assets)
