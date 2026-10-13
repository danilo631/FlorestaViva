"""Compile embedded OpenGL ES GLSL shader pairs to catch errors before APK release.

GitHub CI installs glslangValidator; this verifies syntax/static stage semantics.
It does not substitute for a runtime GPU/driver validation.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import sys
shader_paths=[
    "app/src/main/cpp/game.cpp",
    "app/src/main/cpp/forest_river.hpp",
    "app/src/main/cpp/terrain/terrain_shaders.cpp",
    "app/src/main/cpp/vegetation/vegetation_shaders.cpp",
    "app/src/main/cpp/sky/sky_shaders.cpp",
    "app/src/main/cpp/post/post_shaders.cpp",
    "app/src/main/cpp/renderer/shadow_shaders.cpp",
]
source="\n".join(Path(path).read_text() for path in shader_paths)
pattern=r'(?:static )?const char\*(?: const)? ([A-Z0-9_]+)=R"GLSL\((.*?)\)GLSL";'
shaders=re.findall(pattern,source,re.S)
if not any(n=="RIVER_VS" for n,_ in shaders) or not any(n=="RIVER_FS" for n,_ in shaders):
    raise SystemExit("river shaders absent from validator")
if len(shaders)<12:
    raise SystemExit("missing embedded GLES shaders")
with tempfile.TemporaryDirectory() as dirname:
    base=Path(dirname)
    for name,text in shaders:
        stage="vert" if name.endswith("_VS") else "frag" if name.endswith("_FS") else None
        if not stage: raise SystemExit(f"unknown GLSL stage: {name}")
        path=base/(name+"."+stage)
        path.write_text(text)
        result=subprocess.run(["glslangValidator","-S",stage,str(path)],
             capture_output=True,text=True)
        if result.returncode:
            print(f"FAILED {name}: {result.stdout} {result.stderr}",file=sys.stderr)
            raise SystemExit(1)
        print("GLES shader passed:",name)
print("All",len(shaders),"embedded shaders validated")
