"""Download real scanned river-pebble albedo and tangent-space normals.

Asset: "Clean Pebbles" by Charlotte Baglioni/Dario Barresi, Poly Haven CC0.
Downloaded during GitHub Actions after old procedural texture generation, to
replace the plastic-looking fake stones used in Graphics 8.
https://polyhaven.com/a/clean_pebbles
"""
import argparse
import io
import time
import urllib.request
from pathlib import Path
from PIL import Image

def fetch(slug,kind):
    url=(f"https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/"
         f"{slug}/{slug}_{kind}_2k.jpg")
    error=None
    for attempt in range(3):
        try:
            request=urllib.request.Request(url,headers={
                "User-Agent":"FlorestaViva-CC0-Riverbank/1.0"})
            with urllib.request.urlopen(request,timeout=65) as response:
                image=Image.open(io.BytesIO(response.read())).convert("RGB")
            image.load()
            if min(image.size)<1800:raise ValueError("invalid photo map resolution")
            return image
        except Exception as exc:
            error=exc
            if attempt<2:time.sleep(attempt+2)
    raise RuntimeError(f"Could not fetch Poly Haven {slug} {kind}: {error}")

def run(path):
    albedo=fetch("clean_pebbles","diff")
    normals=fetch("clean_pebbles","nor_gl")
    # Both must validate before replacing earlier generated outputs.
    for image,name in [(albedo,"riverbed.png"),
                       (normals,"riverbed_normal.png")]:
        temp=path/(name+".tmp")
        image.save(temp,format="PNG",optimize=True)
        temp.replace(path/name)
    (path/"riverbed_material_source.txt").write_text(
        "Clean Pebbles: Poly Haven CC0\n"
        "https://polyhaven.com/a/clean_pebbles\n"
        "2K diffuse + OpenGL normal, derived from photographic scanning\n")
    print("REAL CC0 RIVER STONES:",albedo.size,normals.size)

if __name__=="__main__":
    arg=argparse.ArgumentParser()
    arg.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    run(arg.parse_args().assets)
