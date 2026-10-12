#!/usr/bin/env python3
"""Repository + built-asset forensic audit. No marketing claims from filenames."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from PIL import Image

def git_bytes(rev,path):
    result=subprocess.run(["git","show",f"{rev}:{path}"],
        check=False,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    return result.stdout if result.returncode==0 else None

def digest(data):
    return hashlib.sha256(data).hexdigest() if data is not None else None

def dimensions(path):
    with Image.open(path) as im:
        return {"pixels":list(im.size),"channels":im.mode,"format":im.format}

def audit(base,output):
    root=Path("app/src/main/assets")
    tracked=subprocess.check_output(["git","ls-files","--",str(root)],text=True).splitlines()
    rows=[]
    for name in tracked:
        p=Path(name)
        if not p.is_file():continue
        current=p.read_bytes()
        old=git_bytes(base,name)
        suffix=p.suffix.lower()
        row={"file":name,"size_bytes":len(current),"sha256_source":digest(current),
             "sha256_baseline":digest(old),
             "same_as_baseline":old==current if old is not None else None,
             "present_at_baseline":old is not None}
        if suffix in (".png",".jpg",".jpeg",".webp"):
            row.update(dimensions(p))
        else:row["format"]=suffix.lstrip(".")
        rows.append(row)
    sets=json.loads(Path("assets_manifest.json").read_text())["texture_sets"]
    licensing=[{"id":x["id"],"author":x.get("author"),"url":x["url"],
                "license":x["license"],"sha256_source":x.get("sha256_source",{})}
               for x in sets]
    report={"baseline_commit":base,"head_commit":
            subprocess.check_output(["git","rev-parse","HEAD"],text=True).strip(),
            "tracked_assets":rows,"material_sources":licensing}
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    print("FILE | BYTES | DIMENSIONS | FORMAT | CURRENT_SHA256 | OLD_SHA256 | SAME")
    for row in rows:
        print(" | ".join([row["file"],str(row["size_bytes"]),
            str(row.get("pixels","")),row.get("format",""),row["sha256_source"],
            row["sha256_baseline"] or "NEW",str(row["same_as_baseline"])]))
    duplicates=[row["file"] for row in rows if row["same_as_baseline"]]
    print("AUDIT identical tracked assets:",len(duplicates),duplicates)
    print("AUDIT materials with SHA256-pinned sources:",len(licensing))
    print("AUDIT this is *source* comparison; CI-generated APK assets differ.")
    return report

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--baseline",default="eaca26c8b34d5287c4278e0cecaf8ffd2b7b5084")
    parser.add_argument("--output",type=Path,default=Path("build/audit/source_assets.json"))
    args=parser.parse_args()
    audit(args.baseline,args.output)
