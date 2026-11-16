"""Create the README GIF from firmware renderers and synthetic data, offline."""
from pathlib import Path
import argparse,subprocess,tempfile,json
from functools import lru_cache
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]

def extract(source,signature):
    start=source.index(signature);opening=source.index('{',start);depth=0
    for pos in range(opening,len(source)):
        depth+=(source[pos]=='{')-(source[pos]=='}')
        if not depth:return source[start:pos+1]
    raise ValueError(signature)

@lru_cache(maxsize=32)
def font(size,bold=False):
    candidates=(["/System/Library/Fonts/Supplemental/Arial Bold.ttf","DejaVuSans-Bold.ttf"] if bold else ["/System/Library/Fonts/Supplemental/Arial.ttf","DejaVuSans.ttf"])
    for name in candidates:
        try:return ImageFont.truetype(name,size)
        except OSError:pass
    try:return ImageFont.load_default(size=size)
    except TypeError:return ImageFont.load_default()

def wrap(draw,value,face,width):
    lines=[];line=''
    for word in value.split():
        trial=(line+' '+word).strip()
        if line and draw.textlength(trial,font=face)>width:lines.append(line);line=word
        else:line=trial
    if line:lines.append(line)
    return lines

def compose(raster,chapter,group,title,detail,total):
    im=Image.new('RGB',(960,580),(12,18,24));d=ImageDraw.Draw(im)
    d.rounded_rectangle((24,24,936,548),radius=26,fill=(16,24,31),outline=(34,47,56),width=1)
    d.text((48,52),'SMALL SCREEN. MANY WORLDS.',font=font(12,True),fill=(111,203,183))
    d.text((46,89),'XIAO',font=font(48,True),fill=(239,242,237))
    d.text((46,143),'Round Display',font=font(36),fill=(239,242,237))
    d.text((48,220),group,font=font(12,True),fill=(111,203,183))
    y=251
    for line in wrap(d,title,font(29,True),332):d.text((47,y),line,font=font(29,True),fill=(241,233,216));y+=35
    y+=16
    for line in wrap(d,detail,font(17),330):d.text((48,y),line,font=font(17),fill=(156,176,184));y+=24
    if group=='LIVE-DATA SCENES':d.text((48,y+18),'ILLUSTRATIVE DATA',font=font(11,True),fill=(205,168,112))
    d.text((48,459),'240 × 240  /  Touchscreen',font=font(14),fill=(156,176,184))
    d.text((48,482),'Scenes • Profiles • Carousel',font=font(13),fill=(113,137,147))
    for k in range(total):
        x=48+k*23;d.rounded_rectangle((x,520,x+16,523),radius=1,fill=(111,203,183) if k==chapter else (51,69,78))
    # A stylised bezel around the real240px raster, enlarged exactly2×.
    cx,cy=674,286
    d.ellipse((cx-253,cy-253,cx+253,cy+253),fill=(4,8,12),outline=(55,65,72),width=2)
    d.ellipse((cx-244,cy-244,cx+244,cy+244),outline=(22,31,38),width=4)
    screen=raster.resize((480,480),Image.Resampling.NEAREST)
    mask=Image.new('L',(960,960));ImageDraw.Draw(mask).ellipse((0,0,959,959),fill=255);mask=mask.resize((480,480),Image.Resampling.LANCZOS)
    im.paste(screen,(cx-240,cy-240),mask)
    d=ImageDraw.Draw(im)
    d.text((480,559),'Firmware renderer preview · sample data · accelerated time',anchor='mm',font=font(11),fill=(132,151,158))
    return im

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=ROOT/'docs/media/xiao-display-showcase.gif');args=ap.parse_args()
    if (ROOT/'XiaoPlayground/UserSettings.local.h').exists():raise SystemExit('Build the public showcase from an unconfigured checkout')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'XiaoPlayground/XiaoPlayground.ino').read_text()
    with tempfile.TemporaryDirectory(prefix='xiao-showcase-',dir='/tmp') as temp:
        tmp=Path(temp)
        (tmp/'showcase_scenes.inc').write_text('\n'.join(extract(source,s) for s in ('void analemma()','void moon()','void solarPoint(','void solarSystem()')))
        common=['-std=c++17','-O2','-DARDUINO=100','-I'+str(ROOT/'tools/showcase_stubs'),'-I'+str(ROOT/'libraries/Adafruit_GFX_Library'),'-I'+str(tmp)]
        subprocess.run(['clang','-O2','-c',str(ROOT/'XiaoPlayground/src/astronomy/astronomy.c'),'-o',str(tmp/'astronomy.o')],check=True)
        subprocess.run(['clang++',*common,str(ROOT/'tools/showcase.cpp'),str(ROOT/'libraries/Adafruit_GFX_Library/Adafruit_GFX.cpp'),str(tmp/'astronomy.o'),'-o',str(tmp/'preview')],check=True)
        subprocess.run([str(tmp/'preview'),str(tmp)],check=True)
        rows=[line.split('\t') for line in (tmp/'frames.tsv').read_text().splitlines()];total=max(int(r[0]) for r in rows)+1
        frames=[compose(Image.open(tmp/r[4]).convert('RGB'),int(r[0]),*r[1:4],total) for i,r in enumerate(rows)]
        # One palette avoids frame-to-frame colour flicker; no dither noise.
        samples=Image.new('RGB',(320*4,194*4),(12,18,24))
        for k,index in enumerate(range(0,len(frames),max(1,len(frames)//16))):
            if k>=16:break
            samples.paste(frames[index].resize((320,194)),((k%4)*320,(k//4)*194))
        palette=samples.quantize(colors=256,method=Image.Quantize.MEDIANCUT)
        encoded=[f.quantize(palette=palette,dither=Image.Dither.NONE) for f in frames]
        encoded[0].save(args.output,save_all=True,append_images=encoded[1:],duration=100,loop=0,optimize=True,disposal=1)
        frames[0].save('/tmp/xiao-showcase-poster.png')
        # Write a representative contact sheet for visual review, outside the repository.
        review=Image.new('RGB',(960*3,580*5),(12,18,24));seen=set();n=0
        for row,frame in zip(rows,frames):
            if row[0] in seen:continue
            seen.add(row[0]);review.paste(frame,((n%3)*960,(n//3)*580));n+=1
        review.resize((1440,1450)).save('/tmp/xiao-showcase-contact.png')
        print(json.dumps({'path':str(args.output),'frames':len(frames),'duration_seconds':len(frames)/10,'bytes':args.output.stat().st_size,'chapters':total}))
if __name__=='__main__':main()
