#!/usr/bin/env python3
"""Strict download of externally authored CC0 PBR material families.

Used by GitHub Actions after the legacy atlas generators. No procedural
"photographic" fallback. A failed URL/checksum/license aborts the build.

The manifest identifies external provenance; the Poly Haven public API
provides source MD5 values and exact versions. Every downloaded JPEG
is verified against this checksum *before* conversion into Android PNGs.
Source SHA256 and generated PNG SHA256 are recorded in provenance.json.

Output "*_rough.png" is packed ORM: R=AO, G=roughness, B=metalness(0).
The GLES shaders must sample .g for roughness and .r for occlusion.
Normal PNGs carry 2 sourced channels; Z is reconstructed in the shader.

API terms require attribution "Powered by Poly Haven": https://polyhaven.com
"""
import argparse
import hashlib
import io
import json
import time
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import Request,urlopen
from PIL import Image,ImageOps

VALID_DOMAINS={"api.polyhaven.com","dl.polyhaven.org"}
MAX_DOWNLOAD=30*1024*1024

def request(url):
    parsed=urlparse(url)
    if parsed.scheme!="https" or parsed.hostname not in VALID_DOMAINS:
        raise ValueError("Untrusted download host: "+url)
    last=None
    for attempt in range(3):
        try:
            with urlopen(Request(url,headers={"User-Agent":"FlorestaViva-CC0-Audit/1.0"}),timeout=70) as stream:
                data=stream.read(MAX_DOWNLOAD+1)
            if not data or len(data)>MAX_DOWNLOAD:raise RuntimeError("oversized/empty download")
            return data
        except Exception as exc:
            last=exc
            if attempt<2:time.sleep(attempt+1)
    raise RuntimeError(f"Failed to fetch {url}: {last}")

def metadata(asset):
    raw=request("https://api.polyhaven.com/files/"+asset)
    data=json.loads(raw)
    if not isinstance(data,dict) or not data:
        raise ValueError("Asset missing texture mappings: "+asset)
    return data

def source(files,kind,resolution):
    # Provider map names and filenames are not uniformly spelled:
    # Forest Leaves 02 uses 'Diffuse' whereas Forest Ground uses 'diff'.
    aliases={"diff":("Diffuse","diff","diffuse"),
             "nor_gl":("nor_gl","NormalGL","normal_gl"),
             "rough":("Rough","rough","roughness"),
             "ao":("AO","ao","ambient_occlusion"),
             "disp":("Displacement","disp","height")}
    for alias in aliases.get(kind,(kind,)):
        variants=files.get(alias)
        if not isinstance(variants,dict):continue
        chosen=variants.get(resolution)
        if isinstance(chosen,dict):
            for fmt in ("jpg","png","jpeg"):
                if isinstance(chosen.get(fmt),dict) and chosen[fmt].get("url"):
                    return chosen[fmt]
    # Secondary fallback: nested variants and provider filenames.
    def walk(node):
        if isinstance(node,dict):
            if isinstance(node.get("url"),str):
                yield node
            for key,value in node.items():
                if key!="url":
                    yield from walk(value)
        elif isinstance(node,list):
            for item in node:
                yield from walk(item)
    candidates=[]
    for entry in walk(files):
        filename=urlparse(entry["url"]).path.rsplit("/",1)[-1].lower()
        if f"_{kind}_" not in filename:
            continue
        if f"_{resolution}." not in filename:
            continue
        if not filename.endswith((".jpg",".jpeg",".png")):
            continue
        score=1 if filename.endswith(".jpg") else 0
        candidates.append((score,entry))
    if not candidates:
        raise RuntimeError(f"No {kind} {resolution} provider map; top-level keys: {list(files)[:16]}")
    return max(candidates,key=lambda x:x[0])[1]

def decode_and_verify(entry,asset,kind):
    info=source(entry,kind,asset["resolution"])
    url=info["url"]
    expected=info.get("md5")
    if not isinstance(expected,str) or len(expected)!=32:
        raise RuntimeError(f"No source checksum from API for {asset['id']}/{kind}")
    body=request(url)
    digest=hashlib.md5(body).hexdigest()
    if digest.lower()!=expected.lower():
        raise RuntimeError(f"Checksum mismatch for {asset['id']}/{kind} expected {expected} got {digest}")
    with Image.open(io.BytesIO(body)) as stream:
        image=stream.copy()
        image.load()
    if image.width<480 or image.height<480 or image.width>8192 or image.height>8192:
        raise RuntimeError(f"Unexpected source dimensions for {asset['id']}/{kind}: {image.size}")
    size=1024 if asset["resolution"]=="1k" else 512
    image=ImageOps.fit(image.convert("RGB"),(size,size),method=Image.Resampling.LANCZOS)
    details={"map":kind,"url":url,"api_md5":expected,"sha256":hashlib.sha256(body).hexdigest()}
    return image,details

def bake(manifest_path:Path,root:Path):
    manifest=json.loads(manifest_path.read_text(encoding="utf-8"))
    root.mkdir(parents=True,exist_ok=True)
    all_records=[]
    for asset in manifest["texture_sets"]:
        if asset["license"]!="CC0-1.0":
            raise RuntimeError(f"Forbidden non-CC0 source: {asset['id']}")
        if not asset["id"].replace("_","").isalnum():
            raise RuntimeError("Invalid asset ID")
        files=metadata(asset["id"])
        parts={}
        sources=[]
        for kind in ("diff","nor_gl","rough","ao"):
            image,entry=decode_and_verify(files,asset,kind)
            parts[kind]=image;sources.append(entry)
        # Optional real-world height/displacement map, never synthesized.
        try:
            image,entry=decode_and_verify(files,asset,"disp")
            parts["disp"]=image;sources.append(entry)
        except RuntimeError as ex:
            print("Optional height map unavailable",asset["id"],str(ex)[:140],flush=True)
        role=asset["role"]
        normal=parts["nor_gl"].copy()
        # Normal XY are preserved from photos; reconstruct positive Z in GLES.
        nx,ny,_=normal.split()
        normalRG=Image.merge("RGB",(nx,ny,Image.new("L",normal.size,255)))
        orm=Image.merge("RGB",(
            ImageOps.grayscale(parts["ao"]),
            ImageOps.grayscale(parts["rough"]),
            Image.new("L",normal.size,0)))
        outputs=[
            (f"{role}.png",parts["diff"]),
            (f"{role}_normal.png",normalRG),
            (f"{role}_rough.png",orm)
        ]
        if "disp" in parts:
            outputs.append((f"{role}_height.png",ImageOps.grayscale(parts["disp"])))
        baked=[]
        for filename,image in outputs:
            dest=root/filename
            tmp=root/(filename+".new")
            image.save(tmp,format="PNG",optimize=True)
            tmp.replace(dest)
            content=dest.read_bytes()
            baked.append({"file":filename,"size":list(image.size),
                           "sha256":hashlib.sha256(content).hexdigest()})
        all_records.append({"id":asset["id"],"license":"CC0-1.0",
                            "author":asset["author"],"page":asset["url"],
                            "sources":sources,"output":baked})
        print(f"INSTALLED PHOTOGRAPHY {asset['id']}: "
              f"{len(outputs)} maps, {len(parts['diff'].tobytes())//1024} KiB decoded RGB",
              flush=True)
        for src in sources:
            print("SOURCE_SHA256",asset["id"],src["map"],src["sha256"],flush=True)
    provenance={"generator":"tools/fetch_assets.py","provider":"Poly Haven",
                "license":"CC0-1.0","credits":"Powered by Poly Haven",
                "sources":all_records}
    (root/"assets_provenance.json").write_text(
        json.dumps(provenance,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print("VALIDATED ALL MATERIALS",len(all_records),flush=True)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--manifest",type=Path,default=Path("assets_manifest.json"))
    parser.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    args=parser.parse_args()
    bake(args.manifest,args.assets)

if __name__=="__main__":main()
