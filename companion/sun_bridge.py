#!/usr/bin/env python3
"""Fetch official NASA/NOAA data and optionally feed the USB-connected XIAO."""
from __future__ import annotations
import argparse, binascii, concurrent.futures, fcntl, io, json, os, re, ssl, sys, time
from datetime import datetime, timezone, timedelta
from pathlib import Path
from urllib.request import Request, urlopen
from runtime_config import DATA_ROOT,civil_zone,load
ROOT=DATA_ROOT
sys.path.insert(0,str(Path(__file__).resolve().parent/'vendor'))
from PIL import Image
URLS={
 'image':'https://umbra.nascom.nasa.gov/images/latest_aia_304.gif',
 'index':'https://umbra.nascom.nasa.gov/images/latest.html',
 'kp':'https://services.swpc.noaa.gov/json/planetary_k_index_1m.json',
 'flares':'https://services.swpc.noaa.gov/json/goes/primary/xray-flares-latest.json',
 'scales':'https://services.swpc.noaa.gov/products/noaa-scales.json',
}
EPOCH=datetime(2000,1,1,tzinfo=civil_zone(ROOT))
def epoch(text):
 if not text:return 0
 d=datetime.fromisoformat(text.replace('Z','+00:00'))
 if d.tzinfo is None:d=d.replace(tzinfo=timezone.utc)
 return int((d-EPOCH).total_seconds())
def fetch(url):
 request=Request(url,headers={'User-Agent':'XIAO-Solar-Scenes/1.0 (NASA/NOAA educational display)'})
 with urlopen(request,timeout=30) as response:return response.read()
def build_frame(raw):
 image=Image.open(io.BytesIO(raw)).convert('RGB').resize((240,240),Image.Resampling.LANCZOS)
 # Retain the full image; mask only the glass's invisible square corners.
 values=[]
 for y in range(240):
  for x in range(240):
   r,g,b=image.getpixel((x,y))
   if (x-119.5)**2+(y-119.5)**2>119.5**2:r=g=b=0
   values.append(((r>>3)<<11)|((g>>2)<<5)|(b>>3))
 return values,image

def download():
 out={};errors={}
 with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:
  future={pool.submit(fetch,url):key for key,url in URLS.items()}
  for f in concurrent.futures.as_completed(future):
   key=future[f]
   try:out[key]=f.result()
   except Exception as e:errors[key]=str(e)
 if 'image' not in out or 'index' not in out:raise RuntimeError('NASA image/date unavailable: '+str(errors))
 index=out['index'].decode(errors='replace')
 found=re.search(r'<strong>He II 304.*?</tr>\s*<tr[^>]*>\s*<td>\s*(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2})',index,re.S)
 if not found:raise RuntimeError('NASA observation timestamp missing; refusing an undated image')
 observed=epoch(found.group(1).replace('/','-'))
 kp=json.loads(out['kp'])[-1] if 'kp' in out else {}
 flares=json.loads(out['flares'])[-1] if 'flares' in out else {}
 scales=json.loads(out['scales']).get('0',{}) if 'scales' in out else {}
 stamp=epoch(str(scales.get('DateStamp',''))+'T'+str(scales.get('TimeStamp',''))) if scales.get('DateStamp') else 0
 getscale=lambda key:int(scales.get(key,{}).get('Scale') or 0) if key in scales else -1
 meta={'image_epoch':observed,'fetched_epoch':epoch(datetime.now(timezone.utc).isoformat()),
       'kp':float(kp.get('estimated_kp',-1)),'kp_epoch':epoch(kp.get('time_tag')),
       'g':getscale('G'),'r':getscale('R'),'s':getscale('S'),'scales_epoch':stamp,
       'xray':str(flares.get('current_class') or '--')[:11],'flare':str(flares.get('max_class') or '--')[:11],
       'flare_epoch':epoch(flares.get('max_time')),'xray_epoch':epoch(flares.get('time_tag'))}
 pixels,image=build_frame(out['image'])
 cache=ROOT/'assets'/'sun';cache.mkdir(parents=True,exist_ok=True)
 image.save(cache/'sun_304_preview.png')
 (cache/'current_meta.json').write_text(json.dumps(meta,indent=2)+'\n')
 (cache/'sun_304.rgb565').write_bytes(b''.join(n.to_bytes(2,'big') for n in pixels))
 if errors:print('Some NOAA fields unavailable: '+str(errors),flush=True)
 return pixels,meta

def generate_header(pixels,meta):
 path=ROOT/'XiaoPlayground'/'SunSnapshot.h'
 with path.open('w') as f:
  f.write('#pragma once\n// NASA SDAC AIA He II 304 A; official NOAA SWPC measurements.\n')
  f.write('static const uint16_t sunSnapshot[240*240]={\n')
  for i in range(0,len(pixels),24):f.write(','.join(hex(x) for x in pixels[i:i+24])+',\n')
  f.write('};\nstatic const SunMeta sunSnapshotMeta={'+','.join([str(meta[k]) for k in ('image_epoch','fetched_epoch','kp_epoch','scales_epoch','flare_epoch','xray_epoch')])+','+str(meta['kp'])+'f,'+','.join(str(meta[k]) for k in ('g','r','s'))+','+json.dumps(meta['xray'])+','+json.dumps(meta['flare'])+'};\n')
 print('Generated dated NASA/NOAA snapshot: '+str(meta),flush=True)

def wait_for(port,prefix,timeout=5):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  line=port.readline().decode('ascii',errors='replace').strip()
  if line.startswith('ERR '):print('Device: '+line,flush=True)
  if (line==prefix if prefix.startswith('ACK') else line.startswith(prefix)):return line
 raise TimeoutError('No device acknowledgement: '+prefix)

def send(pixels,meta,device,on_progress=None):
 from display_transport import open_port
 with open_port(device,115200,timeout=.25,write_timeout=5) as port:
  port.reset_input_buffer()
  now=datetime.now(civil_zone(ROOT))
  port.write(('T'+now.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]+'\n').encode());port.flush()
  wait_for(port,'CLOCK ')
  fields=['@B']+[str(meta[k]) for k in ('image_epoch','fetched_epoch','kp_epoch','scales_epoch','flare_epoch','xray_epoch','kp','g','r','s','xray','flare')]
  port.write(('|'.join(fields)+'\n').encode());port.flush();wait_for(port,'ACK B')
  for row in range(240):
   for col in range(0,240,32):
    data=b''.join(x.to_bytes(2,'big') for x in pixels[row*240+col:row*240+min(col+32,240)])
    packet=f'@P|{row}|{col}|{data.hex()}|{binascii.crc_hqx(data,0xffff):04x}\n'.encode()
    for attempt in range(3):
     port.write(packet);port.flush()
     try:wait_for(port,f'ACK P {row} {col}',3);break
     except TimeoutError:
      if attempt==2:raise
    if on_progress and str(device).startswith('ble://'):on_progress(port)
   if on_progress and not str(device).startswith('ble://') and row%2==1:on_progress(port)
   if row%60==59:print(f'Sun transfer {row+1}/240 rows',flush=True)
  port.write(b'@E\n');port.flush();wait_for(port,'ACK E')
  print('Updated XIAO: dated Sun image, NOAA observations, and RTC',flush=True)

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--generate',action='store_true');parser.add_argument('--device',default=load(ROOT)['device']);parser.add_argument('--watch',action='store_true');parser.add_argument('--once',action='store_true');parser.add_argument('--initial-delay',type=int,default=0);args=parser.parse_args()
 if args.watch:
  lock=(ROOT/'companion'/'sun_bridge.lock').open('w')
  try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  except BlockingIOError:print('Sun update helper is already running',flush=True);return
  lock.write(str(os.getpid()));lock.flush()
  print('Sun update helper active; checks every 15 minutes while the Mac and XIAO are connected.',flush=True)
  if args.initial_delay:time.sleep(args.initial_delay)
 while True:
  try:
   pixels,meta=download()
   if args.generate:generate_header(pixels,meta)
   if args.once or args.watch:send(pixels,meta,args.device)
  except Exception as e:
   print('Solar refresh failed; a dated fallback remains available: '+str(e),file=sys.stderr,flush=True)
   if not args.watch:raise
  if not args.watch:break
  time.sleep(15*60)
if __name__=='__main__':main()
