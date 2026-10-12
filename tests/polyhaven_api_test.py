"""Offline API shape tests; no network access needed for this regression."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from polyhaven_api_bake import flatten,find_file,KINDS
mock={
 "Diffuse":{"2k":{"jpg":{"url":"https://dl.polyhaven.org/file/a_diff_2k.jpg",
                             "size":1024,"md5":"a"*32}}},
 "nor_gl":{"4k":{"jpg":{"url":"https://dl.polyhaven.org/file/a_nor_gl_4k.jpg"}} ,
            "2k":{"jpg":{"url":"https://dl.polyhaven.org/file/a_nor_gl_2k.jpg"}}},
 "rough":{"2k":{"jpg":{"url":"https://dl.polyhaven.org/file/a_rough_2k.jpg"}}}
}
assert len(list(flatten(mock)))==4
for key in KINDS:
    url,metadata=find_file(mock,key)
    assert url and f"_{key}_2k.jpg" in url,(key,url)
    assert metadata and metadata["url"]==url
print("PASS Poly Haven API metadata traversal, file selection and CC0 map completeness")
