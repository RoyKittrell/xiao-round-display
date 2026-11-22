"""Read-only firmware identification, scoped to the currently selected display.

No probing, serial enumeration or network work runs on the menu thread. Older
firmware is recognized by its existing FIRMWARE response; unknown stays unknown.
"""
import json,time,re
from pathlib import Path

MODELS={
 'esp32s3':('XIAO ESP32-S3',True,True),
 'esp32c6':('XIAO ESP32-C6',True,True),
 'nrf54':('XIAO nRF54LM20A Sense',False,True),
}

def identify(lines,endpoint):
    firmware='';family='unknown';wifi_line=False;gym_chip='';gym_status=''
    for line in lines:
        if line.startswith('STATUS firmware=bike-dashboard-'):
            match=re.search(r'firmware=(bike-dashboard-r[0-9]+) chip=([0-9a-fA-F]{12})',line)
            if match:firmware=match[1];gym_chip=bytes.fromhex(match[2])[::-1].hex();gym_status=line
    ota={}
    for line in lines:
        if line.startswith('OTA '):
            f=dict(re.findall(r'([a-z_]+)=([^\s]+)',line))
            ota['ota_supported']=f.get('supported')=='1'
            if re.fullmatch('[0-9a-f]{64}',f.get('elf','')):ota['ota_elf']=f['elf']
            try:ota['ota_address']=int(f['address'],0)
            except (KeyError,ValueError):pass
    if gym_chip:
        caps=dict(re.findall(r'([a-z_]+)=([^\s]+)',gym_status))
        wifi=caps.get('wifi_supported')=='1';ble=caps.get('ble_supported')=='1' and caps.get('ble_ready')=='1'
        return {**ota,'role':'gym','family':'esp32s3','model':'Gym Xiao · ESP32-S3','firmware':firmware,'identified':True,'chip_serial':gym_chip,'usb':True,'bluetooth':ble,'watch_csc':caps.get('watch_csc')=='1','wifi_hardware':True,'wifi_firmware':wifi,'usb_setup_chunk':32,'usb_setup_pause_ms':15,'control_version':0,'scene_count':0,'clock_filters':False,'endpoint':str(endpoint),'updated_at':int(time.time())}
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
    return {**ota,'role':'desktop' if family!='unknown' else 'unknown','family':family,'model':name,'firmware':firmware,'identified':family!='unknown',
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
        # Both protocols accept newline-terminated diagnostics. Never send a
        # desktop control/time/data packet until a positive desktop identity.
        port.write(b'?\n');port.flush();end=time.monotonic()+.6;lines=[]
        while time.monotonic()<end:
            line=port.readline().decode('ascii',errors='replace').strip()
            if line.startswith(('FIRMWARE ','WIFI ','STATUS ','OTA ')):lines.append(line[:400])
            if line.startswith(('SUN rows=','UI ')):break
        if identify(lines,target).get('role')=='unknown':
            port.write(b'STATUS\n');port.flush();end=time.monotonic()+1.5
            while time.monotonic()<end:
                line=port.readline().decode('ascii',errors='replace').strip()
                if line.startswith(('STATUS ','OTA ')):lines.append(line[:400])
                if line.startswith('UI '):break
        self.value=identify(lines,target);self.probed=bool(self.value['firmware']);self.save()
        return self.value
    def observe_boot(self,value):
        if not value:return
        if self.boot_id and value!=self.boot_id:self.probed=False
        self.boot_id=value
