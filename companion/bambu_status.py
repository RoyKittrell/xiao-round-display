#!/usr/bin/env python3
"""Read-only P2S MQTT subscription; no print/control messages are published.

Credentials stay in a private Mac data file. The display receives only status,
progress, remaining time, estimated finish time and an ASCII print title.
"""
from __future__ import annotations
import argparse,binascii,copy,hashlib,json,math,os,ssl,sys,threading,time,unicodedata
from pathlib import Path
from runtime_config import DATA_ROOT
sys.path.insert(0,str(Path(__file__).resolve().parent/'vendor'))
OFFLINE,IDLE,PRINTING,PAUSED,PREPARING,FINISHED,ERROR,CONNECTING=range(8)
STATES={'IDLE':IDLE,'RUNNING':PRINTING,'PAUSE':PAUSED,'PAUSED':PAUSED,'PREPARE':PREPARING,'FINISH':FINISHED,'FAILED':ERROR,'SLICING':PREPARING,'INIT':PREPARING}
DEFAULT_DATA=DATA_ROOT

def title(value):
 value=unicodedata.normalize('NFKD',str(value)).encode('ascii','ignore').decode()
 value=''.join(c if 32<=ord(c)<=126 else ' ' for c in value).replace('_',' ')
 for suffix in ('.gcode.3mf','.3mf','.gcode'):
  if value.lower().endswith(suffix):value=value[:-len(suffix)];break
 return ' '.join(value.split())[:95]

def integer(value,lo,hi):
 if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or value<lo or value>hi:return None
 return int(value)

class Model:
 def __init__(self):self.reset()
 def reset(self):
  self.fields={};self.observed=0;self.finish=0;self.remaining_at=0;self.remaining_value=None
 def merge(self,message,now=None):
  now=int(time.time() if now is None else now)
  data=message.get('print') if isinstance(message,dict) else None
  if not isinstance(data,dict):return False
  relevant={'gcode_state','mc_percent','mc_remaining_time','subtask_name','task_id','subtask_id','gcode_file'}
  if not relevant.intersection(data):return False
  # A new job must not inherit an old job's100% or filename/ETA.
  changed=any(k in data and str(data[k]) not in ('','0') and k in self.fields and str(self.fields[k])!=str(data[k]) for k in ('task_id','subtask_id'))
  if changed:self.reset()
  old_state=self.fields.get('gcode_state')
  state=data.get('gcode_state',old_state)
  if state not in STATES:return False
  if state in ('PREPARE','RUNNING') and old_state in ('IDLE','FINISH','FAILED'):
   self.fields={};self.finish=0;self.remaining_value=None
  self.fields.update({k:v for k,v in data.items() if k in relevant})
  self.observed=now
  remaining=integer(self.fields.get('mc_remaining_time'),0,43200)
  if state in ('RUNNING','PREPARE','SLICING','INIT'):
   if remaining is not None:
    if remaining!=self.remaining_value or not self.finish or old_state in ('PAUSE','PAUSED'):
     self.finish=now+remaining*60;self.remaining_at=now;self.remaining_value=remaining
   else:self.finish=0
  else:self.finish=0
  return True
 def snapshot(self,now=None):
  if not self.observed:return None
  now=int(time.time() if now is None else now);state=STATES[self.fields['gcode_state']]
  progress=integer(self.fields.get('mc_percent'),0,100)
  minutes=integer(self.fields.get('mc_remaining_time'),0,43200)
  remaining=-1 if minutes is None else minutes*60
  if self.finish:remaining=max(0,self.finish-now)
  name=self.fields.get('subtask_name') or self.fields.get('gcode_file','').rsplit('/',1)[-1]
  if state==IDLE:progress,remaining,name=-1,-1,''
  if state==FINISHED:progress,remaining=100,0
  return {'observed_at':self.observed,'state':state,'progress':-1 if progress is None else progress,
          'remaining_secs':remaining,'finish_at':self.finish if state in (PRINTING,PREPARING) else 0,'name':title(name)}

def packet(snapshot):
 fields=['M',str(snapshot['observed_at']),str(snapshot['state']),str(snapshot['progress']),str(snapshot['remaining_secs']),str(snapshot['finish_at']),title(snapshot['name']).encode('ascii').hex()]
 body='|'.join(fields).encode('ascii');return b'@'+body+b'|'+f'{binascii.crc_hqx(body,0xffff):04x}'.encode()+b'\n'

class Monitor:
 def __init__(self,root):
  self.root=Path(root);self.lock=threading.Lock();self.model=Model();self.connected=False;self.ever_connected=False;self.client=None;self.started=int(time.time());self.disconnected_at=self.started
  path=self.root/'private/bambu.json'
  if not path.exists():return
  import paho.mqtt.client as mqtt
  config=json.loads(path.read_text());serial=config['serial']
  context=ssl.create_default_context(cafile=str(self.root/'private/bambu.pem'))
  # This exact printer leaf is trusted (pin), rather than disabling cert trust.
  # Its CN is the serial, not the IP; serial was checked at pairing.
  context.check_hostname=False;context.verify_flags|=ssl.VERIFY_X509_PARTIAL_CHAIN
  class StatusClient(mqtt.Client):
   def reconnect(self):
    try:return super().reconnect()
    except (OSError,ssl.SSLError) as error:
     print('P2S network connection failed: '+type(error).__name__+' '+str(error),file=sys.stderr,flush=True)
     raise
  client=StatusClient(mqtt.CallbackAPIVersion.VERSION2,client_id='xiao_display_'+os.urandom(5).hex(),protocol=mqtt.MQTTv311)
  client.username_pw_set('bblp',config['access_code']);client.tls_set_context(context)
  client.reconnect_delay_set(1,30)
  def on_connect(c,userdata,flags,reason,properties):
   with self.lock:
    self.connected=reason==0
    if self.connected:self.ever_connected=True;self.model.reset()
    else:self.disconnected_at=int(time.time())
   if reason==0:c.subscribe('device/'+serial+'/report',qos=0)
   else:print('P2S MQTT connection rejected: '+str(reason),file=sys.stderr,flush=True)
  def on_disconnect(c,userdata,flags,reason,properties):
   with self.lock:self.connected=False;self.disconnected_at=int(time.time())
  def on_message(c,userdata,message):
   try:
    if len(message.payload)>262144:return
    data=json.loads(message.payload)
    with self.lock:self.model.merge(data)
   except (ValueError,TypeError,KeyError):pass
  client.on_connect=on_connect;client.on_disconnect=on_disconnect;client.on_message=on_message
  client.connect_async(config['host'],8883,30);client.loop_start();self.client=client
 def snapshot(self):
  if not self.client:return None
  with self.lock:
   now=int(time.time())
   if self.connected:
    value=self.model.snapshot(now)
    if value:return value
    state=CONNECTING
   else:state=OFFLINE if self.ever_connected or now-self.started>=15 else CONNECTING
   return {'observed_at':now,'state':state,'progress':-1,'remaining_secs':-1,'finish_at':0,'name':''}
 def close(self):
  if self.client:self.client.disconnect();self.client.loop_stop()

if __name__=='__main__':
 raise SystemExit('Use pair_printer.py for explicit local setup; account files are never imported automatically.')
