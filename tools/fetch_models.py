#!/usr/bin/env python3
"""Import actual CC0 Poly Haven glTF meshes and create 3 explicit Android LODs.

Scanned geometry is downloaded from the provider, checksum checked, and
simplified with trimesh/fast-simplification. No Blender installation required.
The game runtime reads the resulting compact FVME binary; glTF is the source
format at build time (NOT loaded directly by the current GLES runtime).
"""
import argparse
import hashlib
import json
import struct
import time
from pathlib import Path,PurePosixPath
from urllib.parse import urlparse
from urllib.request import Request,urlopen
import numpy as np
import trimesh

MAX_BYTES=200*1024*1024
ALLOW={"api.polyhaven.com","dl.polyhaven.org"}
ASSETS=[
 ("tree_stump_01","scanned_stump",[4800,1700,480],1.85),
 ("dead_tree_trunk_02","scanned_deadlog",[6000,1900,540],3.20),
]
def fetch(url,md5=None):
    p=urlparse(url)
    if p.scheme!="https" or p.hostname not in ALLOW:
        raise RuntimeError("unexpected download host "+url)
    error=None
    for attempt in range(3):
        try:
            with urlopen(Request(url,headers={"User-Agent":"FlorestaViva-CC0-glTF/1.0"}),
                        timeout=100) as response:
                raw=response.read(MAX_BYTES+1)
            if not raw or len(raw)>MAX_BYTES:raise RuntimeError("empty/oversize")
            if not md5 or len(md5)!=32:
                raise RuntimeError("no trusted provider checksum")
            actual=hashlib.md5(raw).hexdigest()
            if actual.lower()!=md5.lower():raise RuntimeError("MD5 mismatch")
            return raw,hashlib.sha256(raw).hexdigest()
        except Exception as exc:
            error=exc
            if attempt<2:time.sleep(attempt+1)
    raise RuntimeError(f"Fetch failed {url}: {error}")
def index(slug):
    with urlopen(Request(f"https://api.polyhaven.com/files/{slug}",
                         headers={"User-Agent":"FlorestaViva-CC0-glTF/1.0"}),timeout=60) as resp:
        return json.loads(resp.read())
def safe_path(root,key):
    path=PurePosixPath(key.replace("\\","/"))
    if path.is_absolute() or ".." in path.parts or ":" in key:
        raise RuntimeError("Unsafe included glTF path: "+key)
    result=root.joinpath(*path.parts)
    result.parent.mkdir(parents=True,exist_ok=True)
    return result
def ingest(slug,source_root):
    files=index(slug)
    available=files.get("gltf",{})
    res=next((k for k in ("1k","2k","4k") if isinstance(available.get(k),dict)
             and "gltf" in available[k]),None)
    if res is None:raise RuntimeError("No glTF model from provider: "+slug)
    entry=available[res]["gltf"]
    include=entry.get("include",{})
    if not isinstance(include,dict):
        raise RuntimeError("Invalid included glTF assets")
    folder=source_root/slug
    folder.mkdir(parents=True,exist_ok=True)
    uri=urlparse(entry["url"]).path.rsplit("/",1)[-1]
    primary=safe_path(folder,uri)
    downloaded=[]
    for name,info in [(uri,entry),*include.items()]:
        dest=safe_path(folder,name)
        raw,sha=fetch(info["url"],info.get("md5"))
        dest.write_bytes(raw)
        downloaded.append({"file":name,"sha256":sha,"url":info["url"]})
    if primary.suffix.lower() not in {".gltf",".glb"}:
        raise RuntimeError("Provider model is not glTF: "+str(primary))
    if not primary.is_file():raise RuntimeError("GLTF source missing")
    return primary,downloaded

def write_lod(model,path,triangles,span):
    target=min(int(triangles),len(model.faces))
    if target<90:
        raise ValueError("LOD too small for safe scanned mesh")
    if len(model.faces)>target:
        mesh=model.simplify_quadric_decimation(face_count=target)
    else:mesh=model.copy()
    mesh.remove_unreferenced_vertices()
    if mesh.faces.size<90 or not np.isfinite(mesh.vertices).all():
        raise RuntimeError("Invalid scanned mesh after simplification")
    points=np.asarray(mesh.vertices,dtype=np.float64)
    ext=points.max(axis=0)-points.min(axis=0)
    # Keep height and width plausible for real object; preserve original proportions.
    full=max(ext[0],ext[1],ext[2],1e-6)
    factor=span/full
    points=(points-np.array([(points[:,0].min()+points[:,0].max())*.5,
                            points[:,1].min(),
                            (points[:,2].min()+points[:,2].max())*.5]))*factor
    # Recompute area-weighted vertex normals with NumPy, avoiding SciPy
    # dependency and ensuring normals survive mesh simplification.
    face=np.asarray(mesh.faces,dtype=np.int64)
    v0,v1,v2=points[face[:,0]],points[face[:,1]],points[face[:,2]]
    faceNormals=np.cross(v1-v0,v2-v0)
    normals=np.zeros_like(points)
    for c in range(3):
        np.add.at(normals,face[:,c],faceNormals)
    magnitude=np.linalg.norm(normals,axis=1)
    normals/=np.maximum(magnitude[:,None],1e-8)
    if normals.shape!=points.shape:raise RuntimeError("Missing vertex normals")
    tex=np.zeros((len(points),2),dtype=np.float64)
    try:
        uv=np.asarray(mesh.visual.uv)
        if uv.shape==tex.shape and np.isfinite(uv).all():tex=uv
    except AttributeError:pass
    # Fallback triplanar-ish cylindrical UV if decimation discarded original UVs.
    if not np.any(tex):
        tex[:,0]=np.arctan2(points[:,2],points[:,0])/(2*np.pi)+.5
        tex[:,1]=points[:,1]/max(span,.1)
    data=np.zeros((len(points),12),dtype="<f4")
    data[:,:3]=points
    data[:,3:6]=normals
    data[:,6:8]=tex
    face=np.asarray(mesh.faces,dtype="<u4").reshape(-1)
    if len(face)%3 or face.max()>=len(data):raise RuntimeError("Bad mesh indices")
    blob=b"FVME0001"+struct.pack("<II",len(data),len(face))+data.tobytes()+face.tobytes()
    path.write_bytes(blob)
    print("SCANNED_MODEL",path.name,"triangles",len(face)//3,
          "vertices",len(data),"bytes",len(blob),flush=True)
    return {"triangles":len(face)//3,"vertices":len(data),
            "sha256":hashlib.sha256(blob).hexdigest()}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--assets",type=Path,default=Path("app/src/main/assets"))
    p.add_argument("--source",type=Path,default=Path("build/scanned-gltf-sources"))
    args=p.parse_args()
    args.assets.mkdir(parents=True,exist_ok=True)
    results=[]
    for slug,prefix,lods,span in ASSETS:
        path,downloads=ingest(slug,args.source)
        scene=trimesh.load(path,force="scene",process=False)
        # Dump applies node transforms to meshes and handles multiple scene objects.
        pieces=[m for m in scene.dump() if isinstance(m,trimesh.Trimesh)
                and len(m.faces)>0]
        if not pieces:raise RuntimeError("No glTF triangle geometry "+slug)
        merged=trimesh.util.concatenate(pieces)
        if len(merged.faces)>5000000:
            raise RuntimeError("Source too complex for mobile converter "+slug)
        records=[]
        for idx,n in enumerate(lods):
            filename=f"{prefix}_lod{idx}.fvmesh"
            record=write_lod(merged,args.assets/filename,n,span)
            records.append({"filename":filename,"lod":idx,**record})
        results.append({"slug":slug,"license":"CC0-1.0",
                        "page":"https://polyhaven.com/a/"+slug,
                        "downloaded":downloads,"converted":records})
    (args.assets/"scanned_models_manifest.json").write_text(
        json.dumps(results,indent=2)+"\n")
    print("PASS",len(results),"real CC0 glTF objects converted to 3 LODs",flush=True)
if __name__=="__main__":main()
