"""Offline generator of the bundled foliage/soil diffuse texture set.
These are detailed procedural diffuse textures, not scanned/photo PBR maps.
Requires: pip install pillow numpy. Android itself requires no Python.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
import numpy as np
import math, random

OUT = Path(__file__).resolve().parents[1] / 'app/src/main/assets'
OUT.mkdir(parents=True,exist_ok=True)
SIZE = 512
rng = np.random.default_rng(20261011)
py = random.Random(50201)


def periodic_noise(res):
    """2D wraparound interpolated value noise, tiled seamlessly."""
    grid = rng.random((res,res))
    coords = (np.arange(SIZE,dtype=np.float32) / SIZE * res)
    i = coords.astype(np.int32)
    u = coords - i
    u = u*u*(3-2*u)
    x0 = i%res; x1 = (i+1)%res
    y0=y_idx=x0; y1=x1
    a = grid[y0[:,None],x0[None,:]]
    b = grid[y0[:,None],x1[None,:]]
    c = grid[y1[:,None],x0[None,:]]
    d = grid[y1[:,None],x1[None,:]]
    u2 = u[None,:]; v2=u[:,None]
    return ((a*(1-u2)+b*u2)*(1-v2)+(c*(1-u2)+d*u2)*v2).astype(np.float32)


def layers(weights):
    result=np.zeros((SIZE,SIZE),np.float32)
    for res,w in weights:result+=(periodic_noise(res)-.5)*w
    return result


def make_rgb(name,base,weights,grain=5):
    detail=layers(weights)
    rough=rng.normal(0,grain,(SIZE,SIZE)).astype(np.float32)
    arr=np.empty((SIZE,SIZE,3),np.uint8)
    for i in range(3):
        arr[:,:,i]=np.clip(base[i]+detail*(.95+(.12*i))+rough,0,255)
    im=Image.fromarray(arr,'RGB')
    return im

# Mixed temperate-forest grassy ground: soil showing through the blades.
grass=make_rgb('grass',(70,94,45),[(4,32),(12,26),(32,19),(96,12),(256,6)],8)
d=ImageDraw.Draw(grass,'RGBA')
for n in range(18000):
    x=py.randrange(SIZE);y=py.randrange(SIZE)
    a=py.random()*math.tau
    ln=py.uniform(1.5,5.0)
    green=py.choice([(88,112,48,120),(45,66,29,150),(109,128,57,105),(156,142,66,80)])
    d.line((x,y,x+ln*math.cos(a),y+ln*math.sin(a)),fill=green,width=1)
grass.save(OUT/'grass.png',optimize=True)

# Brown damp topsoil and scattered pebbles.
soil=make_rgb('soil',(88,76,59),[(4,32),(12,36),(40,19),(128,18),(256,11)],11)
d=ImageDraw.Draw(soil,'RGBA')
for i in range(6600):
    x=py.randrange(SIZE);y=py.randrange(SIZE)
    rx=py.uniform(.3,2.2);ry=py.uniform(.25,1.35)
    tone=py.choice([(29,25,20,125),(153,139,115,80),(43,37,28,95),(117,103,86,110)])
    d.ellipse((x-rx,y-ry,x+rx,y+ry),fill=tone)
soil.save(OUT/'soil.png',optimize=True)

# Velvet moss with intertwined layers.
moss=make_rgb('moss',(49,84,43),[(4,43),(12,26),(32,24),(128,19),(256,5)],8)
d=ImageDraw.Draw(moss,'RGBA')
for i in range(13000):
    x=py.randrange(SIZE);y=py.randrange(SIZE)
    n=py.choice([(117,137,56,55),(135,155,70,100),(20,56,25,90),(59,96,44,120)])
    d.ellipse((x,y,x+py.uniform(.5,2.5),y+py.uniform(.5,2.5)),fill=n)
moss.save(OUT/'moss.png',optimize=True)

# Fiber-rich bark with deep vertical clefts and mottled lichen.
y,x=np.indices((SIZE,SIZE))
noisegrain=layers([(4,18),(16,10),(64,10),(128,7)])
vertical=np.sin(x*.11+np.sin(y*.013)*2.+np.cos(y*.038+x*.029)*.9)
vertical+=np.sin(x*.32+np.sin(y*.025)*2)*.32
cracks=np.maximum(0,np.sin(x*.071+np.cos(y*.012)*.8+np.sin(y*.06+x*.04))-.64)
base=np.stack([100-35*cracks+16*vertical+noisegrain,
               81-33*cracks+12*vertical+noisegrain*.88,
               62-29*cracks+8*vertical+noisegrain*.71],axis=-1)
bark=Image.fromarray(np.uint8(np.clip(base,0,255)),'RGB')
d=ImageDraw.Draw(bark,'RGBA')
for i in range(1500):
    px=py.randrange(SIZE);pyy=py.randrange(SIZE)
    d.line((px,pyy,px+py.uniform(-1,1),pyy+py.uniform(3,25)),
        fill=py.choice([(23,20,16,110),(157,142,111,50),(114,129,83,70)]),width=py.choice([1,1,2]))
bark.save(OUT/'bark.png',optimize=True)

# Canopy material: overlapping small leaves creates spatial high-frequency texture.
canopy=make_rgb('canopy',(52,106,40),[(4,39),(12,20),(32,15),(128,12)],6)
d=ImageDraw.Draw(canopy,'RGBA')
for i in range(11000):
    px=py.randrange(SIZE);pyy=py.randrange(SIZE)
    w=py.uniform(1,4.5);h=w*py.uniform(.6,1.55)
    color=py.choice([(18,57,24,120),(34,85,34,170),(108,153,52,165),
                     (151,164,69,120),(80,121,41,150),(161,183,94,70)])
    d.ellipse((px-w,pyy-h,px+w,pyy+h),fill=color)
canopy.save(OUT/'canopy.png',optimize=True)

# Cutout blade cards for GPU instancing; transparent background, feathered silhouettes.
blades=Image.new('RGBA',(256,512),(0,0,0,0));d=ImageDraw.Draw(blades,'RGBA')
for i in range(150):
    bx=py.uniform(23,235);by=py.uniform(450,530)
    topx=bx+py.uniform(-120,120);topy=py.uniform(45,280)
    thick=py.uniform(1.5,8.0)
    color=py.choice([(73,104,33,220),(92,135,43,235),(127,143,61,235),
                     (52,91,30,230),(165,151,67,215),(108,145,66,220)])
    d.polygon([(bx-thick,by),(topx,topy),(bx+thick,by)],fill=color)
    d.line((bx,by,topx,topy),fill=(184,176,82,55),width=1)
blades.save(OUT/'blades.png',optimize=True)

# Cluster-of-leaves cards for undergrowth bushes.
bush=Image.new('RGBA',(512,512),(0,0,0,0));d=ImageDraw.Draw(bush,'RGBA')
for i in range(85):
    px=py.gauss(256,104);yy=py.gauss(257,85)
    if not 28<px<484 or not 28<yy<484:continue
    sz=py.uniform(10,37)
    ang=py.uniform(-3.14,3.14)
    color=py.choice([(34,86,36,235),(45,113,48,240),(87,144,51,240),
                     (107,159,74,238),(31,71,39,238),(148,164,83,200)])
    dx,dy=sz*math.cos(ang),sz*math.sin(ang)
    normx,normy=-dy*.45,dx*.45
    d.polygon([(px-dx,yy-dy),(px+normx,yy+normy),
               (px+dx,yy+dy),(px-normx,yy-normy)],fill=color)
    d.line((px-dx,yy-dy,px+dx,yy+dy),fill=(168,190,114,120),width=1)
bush.save(OUT/'bush.png',optimize=True)

print('Generated:', [(p.name,round(p.stat().st_size/1024)) for p in OUT.glob('*.png')])
