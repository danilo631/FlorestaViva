#!/usr/bin/env python3
"""Compare actual GLES SwiftShader captures with prior run (no synthetic screenshots)."""
import argparse
import json
from pathlib import Path
from PIL import Image,ImageChops,ImageStat

def compare(before:Path,after:Path):
    # Reject screenshots that passed brightness tests but show a crash dialog.
    # The first baseline artifact was invalid: scenes 2 and 3 had identical
    # SHA256 because the game process was dead on the Android launcher.
    import hashlib
    for directory,prefix in [(before,"before"),(after,"after")]:
        photos=[directory/f"{prefix}_scene_{i}.png" for i in range(4)]
        for p in photos:
            if not p.is_file():
                raise RuntimeError(f"Missing {prefix} screenshot: {p}")
        hashes=[hashlib.sha256(p.read_bytes()).hexdigest() for p in photos]
        if len(set(hashes))<4:
            raise RuntimeError(f"INVALID {prefix} screenshot baseline: duplicate frames (game may have crashed) {hashes}")
        if not (directory/"renderer_metrics_logcat.txt").is_file():
            raise RuntimeError(f"No renderer logcat for {prefix}; cannot verify camera/game status")
    rows=[]
    for i in range(4):
        old=before/f"before_scene_{i}.png"
        new=after/f"after_scene_{i}.png"
        if not old.is_file() or not new.is_file():
            raise RuntimeError(f"Scene {i}: missing screenshot {old} {new}")
        a=Image.open(old).convert("RGB")
        b=Image.open(new).convert("RGB")
        different_resolution=a.size!=b.size
        if different_resolution:b=b.resize(a.size,Image.Resampling.LANCZOS)
        diff=ImageChops.difference(a,b)
        mean=ImageStat.Stat(diff).mean
        overlay=Image.new("RGB",(a.width*2,a.height))
        overlay.paste(a,(0,0));overlay.paste(b,(a.width,0))
        dest=after/f"side_by_side_scene_{i}.png"
        overlay.save(dest,optimize=True)
        rows.append({"scene":i,"before":str(old),"after":str(new),
                     "beforeSize":list(a.size),"afterSize":list(b.size),
                     "diffR_Mean":round(mean[0],3),
                     "diffG_Mean":round(mean[1],3),
                     "diffB_Mean":round(mean[2],3),
                     "resizedForComparison":different_resolution})
    (after/"comparison_metrics.json").write_text(json.dumps(rows,indent=2)+"\n")
    print("Real before/after 4-scene software-render capture comparison:",json.dumps(rows))

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--before",type=Path,required=True)
    p.add_argument("--after",type=Path,required=True)
    args=p.parse_args()
    compare(args.before,args.after)
