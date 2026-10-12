#!/usr/bin/env python3
"""Discover and bake CC0 Poly Haven PBR maps with its public API.

The API is a BUILD-TIME service only; the game never needs an internet
connection. Exact fallback CDN paths permit reproducible temporary outages.
Source: https://api.polyhaven.com/files/forest_leaves_03
License: CC0 (https://polyhaven.com/license)
Powered by Poly Haven — https://polyhaven.com
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import time
from urllib.parse import urlparse
from urllib.request import Request, urlopen
from PIL import Image

ASSET="forest_leaves_03"
KINDS={
    "diff":"moss.png",
    "nor_gl":"moss_normal.png",
    "rough":"moss_rough.png",
}
API=f"https://api.polyhaven.com/files/{ASSET}"
CDN=f"https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/{ASSET}"

def request_bytes(url):
    url_info=urlparse(url)
    if url_info.scheme!="https" or url_info.hostname not in {
        "api.polyhaven.com","dl.polyhaven.org"
    }:
        raise ValueError("Untrusted Poly Haven URL: "+url)
    error=None
    for n in range(3):
        try:
            with urlopen(Request(url,headers={"User-Agent":"FlorestaViva-OpenCC0/1.0"}),timeout=60) as result:
                data=result.read(28*1024*1024)
            if not data:raise RuntimeError("empty response")
            return data
        except Exception as ex:
            error=ex
            if n<2:time.sleep(n+1)
    raise RuntimeError(f"Fetch failed {url}: {error}")

def flatten(root,path=()):
    """The API /files object may use varied nesting (resolution/format/etc)."""
    if isinstance(root,dict):
        if isinstance(root.get("url"),str):
            yield path,root
        for key,value in root.items():
            if key!="url":yield from flatten(value,(*path,str(key)))
    elif isinstance(root,list):
        for i,value in enumerate(root):
            yield from flatten(value,(*path,str(i)))

def find_file(files,kind):
    hits=[]
    for path,item in flatten(files):
        url=item["url"]
        info="/".join(path).lower()+"/"+urlparse(url).path.lower()
        basename=urlparse(url).path.rsplit("/",1)[-1].lower()
        if not basename.endswith((".jpg",".png",".jpeg")):continue
        if not (f"_{kind}_" in basename or f"_{kind}." in basename):
            continue
        score=(100 if "_2k." in basename else 0)+(20 if basename.endswith(".jpg") else 0)
        if "_4k." in basename:score-=50
        hits.append((score,url,item))
    return max(hits,key=lambda x:x[0])[1:] if hits else (None,None)

def bake(root:Path,use_api=True):
    root.mkdir(parents=True,exist_ok=True)
    api_files={}
    if use_api:
        try:api_files=json.loads(request_bytes(API).decode("utf-8"))
        except Exception as ex:
            print("API currently unavailable, falling back to canonical CDN URLs:",ex,flush=True)
    entries=[]
    converted=[]
    for kind,output in KINDS.items():
        url,entry=find_file(api_files,kind)
        if not url:
            url=f"{CDN}/{ASSET}_{kind}_2k.jpg"
            entry=None
        raw=request_bytes(url)
        if entry and entry.get("md5"):
            digest=hashlib.md5(raw).hexdigest()
            if digest.lower()!=entry["md5"].lower():
                raise RuntimeError("API MD5 verification failed "+kind)
        with Image.open(io.BytesIO(raw)) as input_image:
            im=input_image.convert("RGB")
            im.load()
        if min(im.size)<1800:raise RuntimeError("bad PBR map resolution "+kind)
        # Never destroy existing output until all source maps validate.
        converted.append((output,im))
        entries.append({"kind":kind,"asset":ASSET,"url":url,
                        "sha256":hashlib.sha256(raw).hexdigest(),
                        "dimensions":list(im.size)})
    for output,im in converted:
        temp=root/(output+".temporary")
        im.save(temp,format="PNG",optimize=True)
        temp.replace(root/output)
    (root/"polyhaven_api_manifest.json").write_text(json.dumps({
        "license":"CC0", "credit":"Powered by Poly Haven",
        "page":f"https://polyhaven.com/a/{ASSET}","files":entries
    },ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("Poly Haven CC0 API material baked:",[x["kind"] for x in entries],flush=True)

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    parser.add_argument("--offline-cdn",action="store_true",
                        help="Skip metadata lookup; use canonical CDN files.")
    args=parser.parse_args()
    bake(args.assets,not args.offline_cdn)
