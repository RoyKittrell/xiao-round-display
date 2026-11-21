"""Bounded Gym r10 status/explicit actions; no cached settings replay."""
from pathlib import Path
import json,re,time,math
from device_registry import atomic,SelectionChanged

def fields(line):return dict(re.findall(r'([a-z_]+)=([^\s]+)',line))
def parse_status(lines):
    value={};seen=set()
    for line in lines:
        if line.startswith('STATUS '):
            f=fields(line)
            if not re.fullmatch(r'bike-dashboard-r\d+',f.get('firmware','')):continue
            value.update(firmware=f['firmware'],chip=f.get('chip',''));seen.add('status')
            for name in ('closures','transitions','level','full','free_heap','max_alloc'):
                try:value[name]=int(f[name])
                except (ValueError,KeyError):pass
            if f.get('storage') in ('OK','ERROR'):value['storage']=f['storage']
        elif line.startswith('TITLE hex='):
            try:
                title=bytes.fromhex(fields(line)['hex']).decode('ascii')
                if valid_title(title):value['title']=title
            except (KeyError,ValueError,UnicodeError):pass
        elif line.startswith('POWER '):
            f=fields(line)
            try:
                value['estimated_watts']=int(f['watts'])
                value['watch_power_streaming']=f.get('subscribed')=='1'
            except (KeyError,ValueError):pass
        elif line.startswith('GLOW '):
            f=fields(line)
            try:
                glow={k:int(f[k]) for k in ('peak','centre','high_floor')}
                if not 1<=glow['peak']<=100 or not 5<=glow['centre']<=100 or not 0<=glow['high_floor']<=100:continue
                value['glow']=glow;seen.add('glow')
            except (ValueError,KeyError):pass
        elif line.startswith('BRIGHTNESS '):
            try:
                n=int(fields(line)['level'])
                if 0<=n<=100:value['brightness']=n;seen.add('brightness')
            except (ValueError,KeyError):pass
        elif line.startswith('DEMO active='):
            f=fields(line)
            if f.get('active') in ('0','1'):value['demo']=f['active']=='1';seen.add('demo')
            try:
                rate=float(f['rate'])
                if math.isfinite(rate) and 0<=rate<=2000:value['pulse_rate']=rate
            except (ValueError,KeyError):pass
        elif line.startswith('UI '):
            f=fields(line)
            for name in ('resistance','touch_ok','sensor_task','drops'):
                try:value[name]=int(f[name])
                except (ValueError,KeyError):pass
            try:
                fps=float(f['fps'])
                if math.isfinite(fps) and 0<=fps<=1000:value['fps']=fps
            except (ValueError,KeyError):pass
            seen.add('ui')
        elif line.startswith('CSC '):
            f=fields(line)
            if f.get('subscribed') in ('0','1'):value['watch_streaming']=f['subscribed']=='1'
            try:value['pulses_per_revolution']=int(f['ppr'])
            except (ValueError,KeyError):pass
            for field,key in [('revolutions','crank_revolutions'),('event_time','crank_event_time')]:
                try:
                    n=int(f[field])
                    if 0<=n<=65535:value[key]=n
                except (ValueError,KeyError):pass
        elif line.startswith('BLE '):
            f=fields(line)
            if f.get('connected') in ('0','1'):value['watch_link_connected']=f['connected']=='1'
            if f.get('encrypted') in ('0','1'):value['watch_link_encrypted']=f['encrypted']=='1'
    value['ready']=seen.issuperset({'status','glow','brightness','demo','ui'})
    return value

def valid_title(value):
    return isinstance(value,str) and 1<=len(value)<=24 and bool(value.strip()) and all(32<=ord(c)<=126 for c in value)

def commands(value):
    if not isinstance(value,dict) or set(value)-{'glow','brightness','demo','title'}:raise ValueError('Unsupported Gym action')
    result=[]
    if 'title' in value:
        if not valid_title(value['title']):raise ValueError('Use 1–24 printable ASCII characters for the display title')
        result.append(b'TITLE '+value['title'].encode('ascii').hex().encode('ascii')+b'\n')
    if 'glow' in value:
        g=value['glow']
        if not isinstance(g,dict) or set(g)!= {'peak','centre','high_floor'}:raise ValueError('Invalid pulse settings')
        for k,lo in [('peak',1),('centre',5),('high_floor',0)]:
            if type(g[k]) is not int or not lo<=g[k]<=100:raise ValueError('Invalid pulse setting')
        result.append(f"GLOW {g['peak']} {g['centre']} {g['high_floor']}\n".encode())
    if 'brightness' in value:
        n=value['brightness']
        if type(n) is not int or not 0<=n<=100:raise ValueError('Invalid screen brightness')
        result.append(f'BRIGHTNESS {n}\n'.encode())
    if 'demo' in value:
        if type(value['demo']) is not bool:raise ValueError('Invalid demo mode')
        result.append(b'DEMO ON\n' if value['demo'] else b'DEMO OFF\n')
    return result

class GymControl:
    def __init__(self,root,actions_root=None):
        self.root=Path(root);self.path=Path(actions_root or root)/'gym-actions.json';self.next_poll=0;self.last_token='';self.value={}
        # Existing queued intents never replay on worker start.
        self.consumed={k:v.get('token') for k,v in self.actions().items() if isinstance(v,dict)}
    def actions(self):
        try:
            v=json.loads(self.path.read_text());return v if isinstance(v,dict) else {}
        except (OSError,ValueError):return {}
    def reset(self):self.next_poll=0;self.value={};self.last_token=''
    def begin_session(self,binding):
        self.reset();self.consumed[binding.id]=self.actions().get(binding.id,{}).get('token')
    def due(self,binding):
        a=self.actions().get(binding.id,{})
        return time.monotonic()>=self.next_poll or (a.get('token')!=self.consumed.get(binding.id) and a.get('selection')==binding.epoch)
    def read_status(self,port,binding):
        binding.check();port.write(b'STATUS\n');port.flush();end=time.monotonic()+2;lines=[]
        while time.monotonic()<end:
            binding.check();line=port.readline().decode('ascii',errors='replace').strip()
            if line:lines.append(line[:400])
            if line.startswith('UI '):break
            if len(lines)>100:raise OSError('Gym response exceeded bound')
        value=parse_status(lines)
        if not value['ready']:raise OSError('Gym status incomplete')
        expected=binding.settings.get('usb_serial','');chip=value.get('chip','')
        if expected and re.fullmatch('[0-9a-fA-F]{12}',chip):
            from display_transport import serial_key
            if serial_key(expected)!=bytes.fromhex(chip)[::-1].hex():raise SelectionChanged('Gym chip identity does not match selected USB serial')
        return value
    def service(self,port,binding):
        # The current physical values win, including touchscreen changes. Merge
        # only fields the user actually edited, never a stale three-slider copy.
        value=self.read_status(port,binding);action=self.actions().get(binding.id,{})
        token=action.get('token');error=None
        if token and token!=self.consumed.get(binding.id):
            self.consumed[binding.id]=token
            if action.get('target')==binding.id and action.get('selection')==binding.epoch:
                try:
                    requested=action.get('values')
                    if not isinstance(requested,dict):raise ValueError('Invalid Gym action')
                    wanted=dict(requested)
                    if 'glow' in wanted:
                        partial=wanted['glow']
                        if not isinstance(partial,dict) or not partial or set(partial)-{'peak','centre','high_floor'}:raise ValueError('Invalid pulse fields')
                        wanted['glow']={**value['glow'],**partial}
                    if 'title' in wanted and 'title' not in value:raise ValueError('This Gym firmware needs the title update over USB first')
                    packets=commands(wanted)
                    sent=False
                    for packet in packets:
                        if packet.startswith(b'GLOW ') and wanted['glow']==value['glow']:continue
                        if packet.startswith(b'BRIGHTNESS ') and wanted['brightness']==value['brightness']:continue
                        if packet.startswith(b'DEMO ') and wanted['demo']==value['demo']:continue
                        binding.check();port.write(packet);port.flush();sent=True
                        prefix=packet.split(b' ',1)[0].decode()+' '
                        deadline=time.monotonic()+2;ack=False
                        while time.monotonic()<deadline:
                            binding.check();line=port.readline().decode('ascii',errors='replace').strip()
                            if line.startswith(prefix):ack=True;break
                        if not ack:raise OSError('Gym setting acknowledgement missing')
                    if sent:value=self.read_status(port,binding)
                    for key,want in wanted.items():
                        if value[key]!=want:raise OSError('Gym setting readback differs')
                    self.last_token=token
                except ValueError as e:error=str(e)
        self.value={**value,'device_id':binding.id,'selection':binding.epoch,'last_token':self.last_token,'error':error,'updated_at':int(time.time())}
        binding.check();atomic(self.root/'gym-status.json',self.value);self.next_poll=time.monotonic()+1
        return self.value
