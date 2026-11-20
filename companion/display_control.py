"""Saved menu preferences and the sole USB control sender.

Brightness is an explicit, ephemeral action. Cached preferences never restore
brightness automatically after helper startup, reconnect or device reset.
"""
from pathlib import Path
import binascii,json,time
DEFAULTS={'schema_version':1,'revision':1,'enabled_mask':1023,'carousel':True,'interval_seconds':900,
          'day_brightness':60,'night_brightness':60,'appearance':0,'power':0,
          'selected_scene':-1,'selection_token':0,'brightness_token':0,'clock_mask':1023}

def validate(value):
    if not isinstance(value,dict):raise ValueError('Preferences must be an object')
    v={**DEFAULTS,**value}
    ranges={'schema_version':(1,1),'revision':(1,4294967295),'enabled_mask':(0,1023),
            'interval_seconds':(60,3600),'day_brightness':(0,100),'night_brightness':(0,100),
            'appearance':(0,2),'power':(0,2),'selection_token':(0,4294967295),'brightness_token':(0,4294967295),'clock_mask':(0,1023)}
    for k,(lo,hi) in ranges.items():
        if type(v[k]) is not int or not lo<=v[k]<=hi:raise ValueError('Invalid '+k)
    if type(v['carousel']) is not bool:raise ValueError('Invalid carousel')
    if type(v['selected_scene']) is not int or v['selected_scene'] not in [-1,*range(2,12)]:raise ValueError('Invalid selected scene')
    return {k:v[k] for k in DEFAULTS}

def packet(value,apply_brightness=False,target_boot=0,version=2):
    p=validate(value)
    if version not in (1,2):raise ValueError('Unsupported control version')
    if version==1:
        p['enabled_mask'] &= 511
        if p['selected_scene']==11:p['selected_scene']=-1
    parts=['C',str(version),str(p['revision']),str(p['enabled_mask']),str(int(p['carousel'])),str(p['interval_seconds']),
           str(p['day_brightness']),str(p['night_brightness']),str(p['appearance']),str(p['power']),
           str(p['selected_scene']),str(p['selection_token']),str(int(apply_brightness)),str(target_boot)]
    if version==2:parts.append(str(p['clock_mask']))
    body='|'.join(parts).encode();return b'@'+body+b'|'+f'{binascii.crc_hqx(body,0xffff):04x}'.encode()+b'\n'

def atomic(path,value):
    temp=path.with_suffix('.tmp');temp.write_text(json.dumps(value,indent=2)+'\n');temp.replace(path)

class Control:
    def __init__(self,root):
        self.root=Path(root);self.path=self.root/'display-preferences.json';self.status_path=self.root/'control-status.json'
        self.value=dict(DEFAULTS);self.mtime=None;self.next_send=0;self.pending_brightness=False;self.error=None;self.boot_id=0;self.protocol_version=2
        if not self.path.exists():atomic(self.path,self.value)
        self.poll();self.consumed_brightness=self.value['brightness_token'];self.pending_brightness=False
    def poll(self):
        try:
            stamp=self.path.stat().st_mtime_ns
            if stamp==self.mtime:return
            new=validate(json.loads(self.path.read_text()))
            if hasattr(self,'consumed_brightness') and new['brightness_token']!=self.consumed_brightness:self.pending_brightness=True
            self.value=new;self.mtime=stamp;self.next_send=0;self.error=None
        except (OSError,ValueError,KeyError,TypeError) as error:self.error=type(error).__name__+': '+str(error)
    def reconnect(self):
        self.poll();self.consumed_brightness=self.value['brightness_token'];self.pending_brightness=False;self.next_send=0
    def due(self,force=False):self.poll();return force or time.monotonic()>=self.next_send
    def exchange(self,port,value,apply=False,target=0):
        port.write(packet(value,apply,target,self.protocol_version));port.flush();end=time.monotonic()+3
        while time.monotonic()<end:
            line=port.readline().decode(errors='replace').strip()
            if line.startswith('ERR '):raise RuntimeError(line)
            if not line.startswith('ACK C|'):continue
            fields=line.split('|')
            if len(fields) not in (12,13):raise ValueError('Invalid control acknowledgement')
            numbers=list(map(int,fields[1:]))
            if numbers[0]!=value['revision']:continue
            return dict(zip(('revision','scene','enabled_mask','carousel','interval_seconds','day_brightness','night_brightness','appearance','sleeping','power','boot_id','clock_mask'),numbers))
        raise TimeoutError('Display control acknowledgement missing')
    def send(self,port):
        try:
            if not self.boot_id:
                # Learn the current session without consuming an in-flight slider
                # request's revision. No brightness or scene selection in this probe.
                probe={**self.value,'revision':(self.value['revision']^0x80000000) or 0x7fffffff,
                       'selected_scene':-1,'selection_token':0}
                self.boot_id=self.exchange(port,probe)['boot_id']
            result=self.exchange(port,self.value,self.pending_brightness,self.boot_id)
            self.boot_id=result['boot_id'];result.update(updated_at=int(time.time()),error=None)
            atomic(self.status_path,result)
            self.consumed_brightness=self.value['brightness_token'];self.pending_brightness=False;self.error=None;self.next_send=time.monotonic()+5
            return result
        except Exception as error:
            self.error=type(error).__name__+': '+str(error);self.next_send=time.monotonic()+10
            atomic(self.status_path,{'updated_at':int(time.time()),'error':self.error})
            return None
