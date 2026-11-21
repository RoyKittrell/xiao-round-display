"""Remembered device facts; names and per-device connection choices are UI-owned.

The coordinator owns this catalog. It never reads or copies Wi-Fi passwords/tokens.
"""
from pathlib import Path
import hashlib,json,time
from display_transport import connection,usb_devices,serial_key

def atomic(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(value,indent=2)+'\n');tmp.chmod(0o600);tmp.replace(path)

def device_id(settings):
    if settings.get('device_id'):return settings['device_id']
    serial=settings.get('usb_serial') or settings.get('bluetooth_usb_serial')
    if serial:return 'usb:'+serial_key(serial)
    if settings.get('wifi_host'):return 'wifi:'+settings['wifi_host']
    if settings.get('bluetooth_id'):return 'ble:'+settings['bluetooth_id'].lower()
    return ''

def selection_key(settings):
    # An epoch distinguishes select-away/select-back from an old queued action.
    return hashlib.sha256(json.dumps(settings,sort_keys=True,separators=(',',':')).encode()).hexdigest()

class SelectionChanged(OSError):pass
class Selection:
    def __init__(self,root,identifier=None):
        self.root=Path(root);self.identifier=identifier
        if identifier:
            from multi_device import settings_for,route_key
            self.settings=settings_for(root,identifier);self.id=identifier;self.key=route_key(self.settings)
        else:self.settings=connection(root);self.id=device_id(self.settings);self.key=selection_key(self.settings)
        self._epoch=self.settings.get('selection_epoch','legacy')
    @property
    def epoch(self):
        if self.identifier:
            from multi_device import settings_for
            return settings_for(self.root,self.identifier).get('selection_epoch','background')
        return self._epoch
    def check(self):
        if self.identifier:
            from multi_device import settings_for,route_key
            key=route_key(settings_for(self.root,self.identifier))
        else:key=selection_key(connection(self.root))
        if key!=self.key:raise SelectionChanged('Device connection changed')
    def verify_usb(self,path):
        self.check();wanted=self.settings.get('usb_serial')
        if not wanted:return
        matches=[p for p in usb_devices() if p['port']==str(path) and serial_key(p['serial'])==serial_key(wanted)]
        if len(matches)!=1:raise SelectionChanged('Selected USB identity is not connected')

    def verify_wireless(self,endpoint):
        self.check()
        if str(endpoint).startswith('wifi://'):
            from wifi_transport import host_from_device
            host=host_from_device(endpoint)
            try:record=json.loads((self.root/'private/wifi'/f'{host}.json').read_text())
            except (OSError,ValueError):raise SelectionChanged('Selected display has no saved Wi-Fi pairing')
            if self.id.startswith('usb:') and 'usb:'+serial_key(record.get('serial',''))!=self.id:raise SelectionChanged('Wi-Fi pairing belongs to another display')
        elif str(endpoint).startswith('ble://') and self.id.startswith('usb:'):
            bound=self.settings.get('bluetooth_usb_serial','')
            if 'usb:'+serial_key(bound)!=self.id:raise SelectionChanged('Pair Bluetooth explicitly for the selected display')

class DeviceRegistry:
    def __init__(self,root):
        self.root=Path(root);self.path=self.root/'device-catalog.json';self.updated=0
        try:self.devices=json.loads(self.path.read_text())['devices']
        except (OSError,ValueError,KeyError,TypeError):self.devices={}
    def refresh(self,settings,profile=None):
        now=time.time();ports=usb_devices()
        for item in self.devices.values():item['usb_present']=False
        for port in ports:
            key='usb:'+serial_key(port['serial']) if port['serial'] else 'port:'+port['port']
            item=self.devices.setdefault(key,{'id':key,'role':'unknown','model':'Unidentified XIAO'})
            item.update(usb_serial=port['serial'],usb_device=port['port'],usb_present=True,last_seen=int(now))
        selected=device_id(settings)
        if selected:
            item=self.devices.setdefault(selected,{'id':selected,'role':'unknown','model':'Unidentified XIAO'})
            for k in ('usb_serial','usb_device','wifi_host','bluetooth_id','bluetooth_usb_serial'):
                if settings.get(k):item[k]=settings[k]
            if profile and profile.get('role') in ('desktop','gym'):
                item.update({k:profile[k] for k in ('role','model','family','firmware','wifi_hardware','wifi_firmware','bluetooth') if k in profile});item['last_seen']=int(now)
        self.save()
        self.updated=now
    def save(self):atomic(self.path,{'schema_version':1,'devices':self.devices,'updated_at':int(time.time())})
