"""Read-only firmware identification, scoped to the currently selected display.

No probing, serial enumeration or network work runs on the menu thread. Older
firmware is recognized by its existing FIRMWARE response; unknown stays unknown.
"""
import json,time
from pathlib import Path

MODELS={
 'esp32s3':('XIAO ESP32-S3',True,True),
 'esp32c6':('XIAO ESP32-C6',True,True),
 'nrf54':('XIAO nRF54LM20A Sense',False,True),
}

def identify(lines,endpoint):
    firmware='';family='unknown';wifi_line=False
    for line in lines:
        if line.startswith('FIRMWARE '):firmware=line[9:].strip()[:100]
        if line.startswith('WIFI '):wifi_line=True
    for key in MODELS:
        if firmware.startswith(key+'-'):family=key;break
    legacy=firmware.startswith('public-template-r26')
    if legacy:family='nrf54' # Legacy public build.
    name,wifi,ble=MODELS.get(family,('Unidentified XIAO',False,False))
    # The live transport itself confirms that connection type; no USB PID guess
    # can reliably tell an S3 from a C6 because they share native CDC IDs.
    ble=ble or str(endpoint).startswith('ble://')
    wifi=wifi or wifi_line or str(endpoint).startswith('wifi://')
    wifi_firmware=wifi and (wifi_line or '-wifi' in firmware)
    return {'family':family,'model':name,'firmware':firmware,'identified':family!='unknown',
            'usb':True,'bluetooth':ble,'wifi_hardware':wifi,'wifi_firmware':wifi_firmware,
            'wifi_rows':wifi_firmware,'control_version':1 if legacy else 2,'scene_count':9 if legacy else 10,
            'clock_filters':not legacy,'firmware_update_recommended':legacy,'usb_setup_chunk':32,'usb_setup_pause_ms':15,
            'endpoint':str(endpoint),'updated_at':int(time.time())}

class HardwareProfile:
    def __init__(self,root):self.path=Path(root)/'hardware-status.json';self.target='';self.value={};self.probed=False;self.boot_id=0
    def save(self):
        tmp=self.path.with_suffix('.tmp');tmp.write_text(json.dumps(self.value,indent=2)+'\n');tmp.replace(self.path)
    def reset(self,target):
        self.target=str(target);self.value=identify([],target);self.probed=False;self.boot_id=0;self.save()
    def ensure(self,port,target):
        if self.target!=str(target):self.reset(target)
        if self.probed:return self.value
        port.write(b'?');port.flush();end=time.monotonic()+1.2;lines=[]
        while time.monotonic()<end:
            line=port.readline().decode('ascii',errors='replace').strip()
            if not line:continue
            if line.startswith(('FIRMWARE ','WIFI ')):lines.append(line[:300])
            if line.startswith('SUN rows='):break
        self.value=identify(lines,target);self.probed=bool(self.value['firmware']);self.save()
        return self.value
    def observe_boot(self,value):
        if not value:return
        if self.boot_id and value!=self.boot_id:self.probed=False
        self.boot_id=value
