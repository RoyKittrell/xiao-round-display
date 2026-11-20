import sys,binascii,importlib.util
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('sun_bridge',ROOT/'companion'/'sun_bridge.py');bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)
import serial
pixels=[(i*3571+19)%65536 for i in range(240*240)]
class Board:
 def __init__(self,*a,**k):self.lines=[];self.memory=[None]*(240*240);self.packet_count=0;self.maximum=0;self.wifi=str(a[0] if a else "").startswith("wifi://")
 def __enter__(self):return self
 def __exit__(self,*a):assert self.memory==pixels
 def reset_input_buffer(self):pass
 def flush(self):pass
 def readline(self):return (self.lines.pop(0)+'\n').encode()
 def write(self,data):
  self.maximum=max(self.maximum,len(data));s=data.decode().strip();f=s.split('|')
  if s.startswith('T'):self.lines.append('CLOCK synced');return
  if s.startswith('@B|'):assert len(f)==13;self.lines.append('ACK B');return
  if f[0]=='@P':
   row,col=int(f[1]),int(f[2]);raw=bytes.fromhex(f[3]);assert binascii.crc_hqx(raw,0xffff)==int(f[4],16)
   assert 0<=row<240 and 0<=col<240 and col%32==0 and len(raw)==min(32,240-col)*2
   for i in range(len(raw)//2):self.memory[row*240+col+i]=int.from_bytes(raw[2*i:2*i+2],'big')
   self.packet_count+=1;self.lines.append(f'ACK P {row} {col}');return
  if f[0]=='@R':
   row=int(f[1]);raw=bytes.fromhex(f[2]);assert len(raw)==480 and 0<=row<240
   assert binascii.crc_hqx(raw,0xffff)==int(f[3],16)
   for i in range(240):self.memory[row*240+i]=int.from_bytes(raw[2*i:2*i+2],'big')
   self.packet_count+=1;self.lines.append(f'ACK R {row}');return
  if s=='@E':assert None not in self.memory;assert self.packet_count==(240 if self.wifi else 1920);assert self.maximum<(1000 if self.wifi else 200);self.lines.append('ACK E');return
  raise AssertionError(s)
serial.Serial=Board
meta={k:0 for k in ('image_epoch','fetched_epoch','kp_epoch','scales_epoch','flare_epoch','xray_epoch','kp','g','r','s')};meta.update(xray='B1.0',flare='C1.0')
interleaved=[]
def progress(port):
 assert port.packet_count%16==0
 interleaved.append(port.packet_count)
bridge.send(pixels,meta,'fake',on_progress=progress)
assert interleaved==list(range(16,1921,16))
print('120 auxiliary status/control opportunities verified between solar rows, including before final commit')
print('57,600 RGB565 pixels, complete coverage, CRCs, metadata and bounded packet size verified')

# Wireless image transfer must yield after every acknowledged pixel packet,
# so scene/brightness controls and connection health are not starved.
from unittest.mock import patch
import display_transport
wireless=[]
with patch.object(display_transport,'open_port',Board):
 bridge.send(pixels,meta,'ble://12345678-1234-1234-1234-123456789abc',on_progress=lambda port:wireless.append(port.packet_count))
assert wireless==list(range(1,1921))
print('1920 wireless control/health opportunities verified, one per image packet')

# TLS sends 240 bounded rows instead of 1920 patches, retaining exact coverage,
# checksums, commit sequencing and a control/health opportunity after every row.
lan=[]
with patch.object(display_transport,'open_port',Board):
 bridge.send(pixels,meta,'wifi://xiao-display-123456.local',on_progress=lambda port:lan.append(port.packet_count))
assert lan==list(range(1,241))
print('240 complete Wi-Fi rows verified; control/health serviced after every row')
