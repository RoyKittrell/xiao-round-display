"""Per-device, absolute screen orientation through the existing guarded transport."""
from pathlib import Path
import binascii,json,time
from device_registry import atomic

def packet(turns):
    if type(turns) is not int or not 0<=turns<=4:raise ValueError('Invalid screen orientation')
    body=f'O|{turns}'.encode()
    return b'@'+body+b'|'+f'{binascii.crc_hqx(body,0xffff):04x}'.encode()+b'\n'

class Orientation:
    def __init__(self,root):
        self.root=Path(root);self.settings=self.root/'orientation-settings.json';self.status=self.root/'orientation-status.json'
        self.next_poll=0;self.stamp=None
    def reconnect(self):self.next_poll=0
    def due(self):
        stamp=self.settings.stat().st_mtime_ns if self.settings.exists() else None
        return stamp!=self.stamp or time.monotonic()>=self.next_poll
    def exchange(self,port,binding,turns):
        binding.check();port.write(packet(turns));port.flush();end=time.monotonic()+2
        while time.monotonic()<end:
            binding.check();line=port.readline().decode('ascii',errors='replace').strip()
            if line.startswith('ERR orientation'):raise OSError(line)
            if line.startswith('ORIENTATION turns='):
                value=line.removeprefix('ORIENTATION turns=')
                if value in ('0','1','2','3'):return int(value)
        raise TimeoutError('Screen orientation acknowledgement missing')
    def service(self,port,binding):
        # No relative movement is replayed. A saved absolute preference is
        # idempotent, survives reconnects, and is scoped to this device only.
        wanted={};stamp=self.settings.stat().st_mtime_ns if self.settings.exists() else None
        if stamp is not None:
            wanted=json.loads(self.settings.read_text())
            if wanted.get('device_id')!=binding.id or type(wanted.get('turns')) is not int or not 0<=wanted['turns']<=3:raise ValueError('Invalid device orientation preference')
        turns=self.exchange(port,binding,4)
        if wanted and wanted['turns']!=turns:
            turns=self.exchange(port,binding,wanted['turns'])
            if turns!=wanted['turns']:raise OSError('Screen orientation readback differs')
        value={'device_id':binding.id,'selection':binding.epoch,'turns':turns,'token':wanted.get('token',''),'updated_at':int(time.time())}
        binding.check();atomic(self.status,value);self.stamp=stamp;self.next_poll=time.monotonic()+3
        return value
