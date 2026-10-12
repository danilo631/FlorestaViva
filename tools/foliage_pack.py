"""Offline unified foliage material pack for mobile.

Replaces six independently uploaded foliage textures with two GPU atlases:
foliage_color.png and foliage_normal.png. A 2x2 grid holds grass tufts,
shrub clusters, broad leaves and pine needles. All inputs are baked during
GitHub Actions (Pillow+NumPy), preserving complete game sources.

Indices: 0 grass blade; 1 bush; 2 broad-leaf 4x4 sub-atlas; 3 pine 4x4.
UVs are transformed in the GLES shader using uAtlasRect, not repacked per mesh.
"""
import argparse
from pathlib import Path
from PIL import Image

LAYERS = (
    ("blades.png",None),
    ("bush.png",None),
    ("leaves_atlas.png","leaves_normal.png"),
    ("pine_atlas.png","pine_normal.png")
)

def fallback_normal(source: Image.Image, size: int) -> Image.Image:
    # A photographic normal map cannot be derived from albedo intensity.
    # Unsupported legacy cutouts get a physically neutral tangent normal,
    # preserving their alpha only; measured normals come from Leaf001.
    rgba=source.convert("RGBA").resize((size,size),Image.Resampling.LANCZOS)
    out=Image.new("RGBA",(size,size),(128,128,255,255))
    out.putalpha(rgba.getchannel("A"))
    return out

def bake(root: Path, tile: int=768) -> None:
    if tile<256 or tile%16: raise ValueError("Tile must be >=256 and multiple of 16")
    color=Image.new("RGBA",(tile*2,tile*2),(0,0,0,0))
    normal=Image.new("RGBA",(tile*2,tile*2),(128,128,255,0))
    for i,(albedo,normal_file) in enumerate(LAYERS):
        src=Image.open(root/albedo).convert("RGBA").resize((tile,tile),Image.Resampling.LANCZOS)
        if normal_file:
            nm=Image.open(root/normal_file).convert("RGBA").resize((tile,tile),Image.Resampling.LANCZOS)
            nm.putalpha(src.getchannel("A"))
        else:
            nm=fallback_normal(src,tile)
        pos=(i%2*tile,i//2*tile)
        color.paste(src,pos)
        normal.paste(nm,pos)
        print("baked foliage layer",i,albedo,"tile",tile)
    color.save(root/"foliage_color.png",optimize=True)
    normal.save(root/"foliage_normal.png",optimize=True)
    assert color.size==normal.size
    # All fields are deterministic and suitable for caching in future builds.
    import json
    (root/"foliage_manifest.json").write_text(json.dumps(
        {"atlas":"foliage_color.png","normal":"foliage_normal.png",
         "grid":[2,2],"tile_size":tile,
         "materials":["wild_grass","shrub","broadleaf","conifer"]},indent=2))

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    p.add_argument("--tile",type=int,default=768)
    args=p.parse_args()
    bake(args.assets,args.tile)
