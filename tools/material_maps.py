"""Generate tangent-space normal maps from forest albedo images at build time.

These are height-derived approximations (not measured PBR normals).
"""
from pathlib import Path
import argparse
import numpy as np
from PIL import Image, ImageFilter

DEFAULT_ASSETS = Path(__file__).resolve().parents[1] / "app/src/main/assets"

def generate(assets: Path, size: int = 512):
    for material in ("grass","soil","moss","bark"):
        source = assets / (material + ".png")
        img = Image.open(source).convert("RGB").resize((size,size), Image.Resampling.LANCZOS)
        gray = img.convert("L").filter(ImageFilter.GaussianBlur(.8))
        height = np.asarray(gray, dtype=np.float32) / 255.
        scale = 5.0 if material == "bark" else 3.2
        dx = (np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))*scale
        dy = (np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))*scale
        nx,ny,nz = -dx,-dy,np.ones_like(height)
        length = np.sqrt(nx*nx+ny*ny+nz*nz)
        normals = np.stack((nx/length,ny/length,nz/length),axis=-1)
        result = np.uint8(np.clip(np.rint((normals*.5+.5)*255),0,255))
        Image.fromarray(result,"RGB").save(assets/(material+"_normal.png"),optimize=True)
        print("Generated",material+"_normal.png")

if __name__ == "__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--assets",type=Path,default=DEFAULT_ASSETS)
    parser.add_argument("--size",type=int,default=512)
    args=parser.parse_args()
    generate(args.assets,args.size)
