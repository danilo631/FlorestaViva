"""Build-time human-readable material preview, not an in-game screenshot."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from PIL import ImageOps

root=Path("app/src/main/assets")
out=Path("build")
out.mkdir(parents=True,exist_ok=True)
samples=[
    ("SOLO DA FLORESTA CC0","grass.png",False),
    ("TERRA/RESTOS CC0","soil.png",False),
    ("CASCA CC0","bark.png",False),
    ("FOLHAS FOTOGRAFADAS","leaves_atlas.png",True),
    ("ATLAS DE VEGETACAO","foliage_color.png",True),
    ("CASCA PINHEIRO CC0","pine_bark.png",False)
]
w,h=1200,840
board=Image.new("RGB",(w,h),(21,28,26))
draw=ImageDraw.Draw(board)
for i,(name,file,check) in enumerate(samples):
    col=i%3
    row=i//3
    x=18+col*397
    y=18+row*412
    img=Image.open(root/file).convert("RGBA")
    if check:
        back=Image.new("RGB",img.size,(72,81,72))
        bg=ImageDraw.Draw(back)
        grid=max(16,img.width//24)
        for a in range(0,img.width,grid):
            for b in range(0,img.height,grid):
                if (a//grid+b//grid)%2==0:
                    bg.rectangle((a,b,a+grid,b+grid),fill=(110,118,106))
        back.paste(img,mask=img.getchannel("A"))
    else: back=img.convert("RGB")
    thumb=ImageOps.fit(back,(370,370),method=Image.Resampling.LANCZOS)
    board.paste(thumb,(x,y))
    draw.text((x,y+374),name,fill=(230,243,229))
board.save(out/"Graphics7_Materials_Preview.jpg",quality=92,optimize=True)
print("Saved materials preview:",out/"Graphics7_Materials_Preview.jpg")
