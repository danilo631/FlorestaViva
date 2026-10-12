#!/usr/bin/env python3
"""Encode external CC0 RGB+normal+ORM maps as native ASTC 6x6 KTX2 mip chains.

Khronos KTX-Software v4.4.2 is SHA-256-pinned. Unlike BasisU/UASTC this
contains GPU-ready ASTC blocks and does NOT need transcoding on ASTC GPUs.
The Android GLES renderer detects support; non-ASTC devices retain PNG
fallback. Alpha coverage-preserving foliage mips still need improvements.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

VERSION="4.4.2"
ARCHIVE=f"KTX-Software-{VERSION}-Linux-x86_64.tar.bz2"
URL=f"https://github.com/KhronosGroup/KTX-Software/releases/download/v{VERSION}/{ARCHIVE}"
SHA256="a8781bad05f9624edbf910b7f258cd0a4ba7d3e63b49ecc0a0ab440bf6a0a245"

def install_ktx(base):
    exe=shutil.which("ktx")
    if exe:
        print("Using preinstalled KTX executable:",exe,flush=True)
        return Path(exe)
    base.mkdir(parents=True,exist_ok=True)
    archive=base/ARCHIVE
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256:
        print("Downloading verified KTX-Software:",URL,flush=True)
        with urllib.request.urlopen(
            urllib.request.Request(URL,headers={"User-Agent":"FlorestaViva-CC0-KTX2/1.0"}),
            timeout=100) as r:
            data=r.read(12*1024*1024)
        if hashlib.sha256(data).hexdigest()!=SHA256:
            raise RuntimeError("Khronos tools SHA256 checksum mismatch")
        archive.write_bytes(data)
    target=base/"extract"
    if not target.exists():
        target.mkdir(parents=True)
        with tarfile.open(archive,"r:bz2") as tf:tf.extractall(target,filter="data")
    candidates=[p for p in target.rglob("ktx") if p.is_file()]
    if not candidates:raise RuntimeError("KTX release has no ktx CLI binary")
    for p in candidates:p.chmod(p.stat().st_mode|0o111)
    return candidates[0]

def encode(root,dest,tool,limit):
    manifest=json.loads(Path("assets_manifest.json").read_text(encoding="utf-8"))
    photos=[]
    for data in manifest["texture_sets"]:
        for suffix in (".png","_normal.png","_rough.png"):
            photos.append(root/(data["role"]+suffix))
    photos.extend([root/"foliage_color.png",root/"foliage_normal.png"])
    if limit:photos=photos[:limit]
    dest.mkdir(parents=True,exist_ok=True)
    records=[]
    for src in photos:
        if not src.exists():raise RuntimeError("Missing audited source texture "+str(src))
        dst=dest/(src.stem+".ktx2")
        command=[str(tool),"create","--format","ASTC_6x6_UNORM_BLOCK",
                 "--generate-mipmap",str(src),str(dst)]
        env=os.environ.copy()
        libdirs=[str(p) for p in tool.parent.parent.glob("lib*") if p.is_dir()]
        env["LD_LIBRARY_PATH"]=(":".join(libdirs)+":"+env.get("LD_LIBRARY_PATH","")).strip(":")
        subprocess.run(command,check=True,env=env)
        body=dst.read_bytes()
        if len(body)<100 or body[:12]!=b"\xabKTX 20\xbb\r\n\x1a\n":
            raise RuntimeError("Output is not KTX2: "+str(dst))
        records.append({"from":src.name,"name":dst.name,"codec":"ASTC_6x6_UNORM",
                        "mips":"generated","bytes":len(body),
                        "sha256":hashlib.sha256(body).hexdigest()})
        print("KTX2_ASTC_6x6",src.name,dst.name,len(body),flush=True)
    (root/"ktx2_codec_manifest.json").write_text(
        json.dumps({"encoder":"KTX-Software "+VERSION,
                    "url":URL,"tool_sha256":SHA256,
                    "codec":"ASTC_6x6_UNORM with full mip chain",
                    "fallback":"PNG (ASTC extension absent)",
                    "files":records},indent=2)+"\n",encoding="utf-8")
    print("KTX2_VERIFIED",len(records),flush=True)

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    p.add_argument("--output",type=Path,default=Path("app/src/main/assets/ktx2"))
    p.add_argument("--tool-cache",type=Path,default=Path("build/ktx-tools"))
    p.add_argument("--limit",type=int,default=0)
    args=p.parse_args()
    encode(args.assets,args.output,install_ktx(args.tool_cache),args.limit)
