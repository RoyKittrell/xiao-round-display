"""Bounded BLE stream adapter for the existing acknowledged display protocol.

USB is the default. BLE connects only to a device explicitly selected by its
macOS UUID; scanning never silently picks a nearby display. Bleak is lazy-loaded
so the USB-only helper and offline tests do not require Bluetooth permission.
"""
from __future__ import annotations
import asyncio,json,threading,time,uuid,os
from pathlib import Path

SERVICE='82a64c00-1e4f-4f34-b9e9-7c0191a0f725'
RX='82a64c01-1e4f-4f34-b9e9-7c0191a0f725'
TX='82a64c02-1e4f-4f34-b9e9-7c0191a0f725'
def phase(value):
    print('Bluetooth: '+value,flush=True)
    root=os.environ.get('XIAO_DISPLAY_DATA_DIR')
    if root:
        try:
            path=Path(root)/'bluetooth-status.json';tmp=path.with_suffix('.tmp')
            tmp.write_text(json.dumps({'phase':value,'updated_at':int(time.time())}));tmp.replace(path)
        except OSError:pass


class ReceiveBuffer:
    def __init__(self,limit=32768):
        self.data=bytearray();self.limit=limit;self.condition=threading.Condition();self.disconnected=False
    def feed(self,value):
        with self.condition:
            if len(self.data)+len(value)>self.limit:
                self.data.clear();self.disconnected=True
            else:self.data.extend(value)
            self.condition.notify_all()
    def lost(self):
        with self.condition:self.disconnected=True;self.condition.notify_all()
    def reset(self):
        with self.condition:self.data.clear();self.disconnected=False
    def readline(self,timeout):
        end=time.monotonic()+timeout
        with self.condition:
            while True:
                if self.disconnected:raise OSError('Bluetooth disconnected or receive buffer exceeded')
                pos=self.data.find(b'\n')
                if pos>=0:
                    line=bytes(self.data[:pos+1]);del self.data[:pos+1];return line
                remaining=end-time.monotonic()
                if remaining<=0:return b'' # Preserve a fragmented line for the next read.
                self.condition.wait(remaining)

class BluetoothSession:
    def __init__(self,address):
        self.address=address;self.buffer=ReceiveBuffer();self.client=None
        self.loop=asyncio.new_event_loop();self.thread=threading.Thread(target=self.loop.run_forever,daemon=True,name='XIAO Bluetooth');self.thread.start()
    def run(self,coro,timeout=70):
        future=asyncio.run_coroutine_threadsafe(coro,self.loop)
        try:return future.result(timeout)
        except TimeoutError:
            future.cancel();self.buffer.lost();raise TimeoutError('Bluetooth operation timed out')
    async def connect(self):
        from bleak import BleakClient,BleakScanner
        if self.ready():return
        if self.client:
            try:await self.client.disconnect()
            except Exception:pass
        phase('Finding selected display')
        found=await BleakScanner.find_device_by_address(self.address,timeout=8,service_uuids=[SERVICE])
        if found is None:raise OSError('Selected Bluetooth display not found')
        self.buffer.reset()
        client=BleakClient(found,disconnected_callback=lambda _:self.buffer.lost(),timeout=60)
        try:
            phase('Connecting to display')
            await client.connect()
            phase('Enabling display replies')
            await client.start_notify(TX,lambda _,data:self.buffer.feed(data))
            # The protected RX write triggers macOS passkey pairing if needed.
            phase('Pairing — check display and macOS prompt')
            await client.read_gatt_char(RX)
            await client.write_gatt_char(RX,b'\n',response=True)
            phase('Connected')
        except BaseException:
            # Cancellation must not strand an authenticated or half-paired peer.
            try:await asyncio.wait_for(client.disconnect(),timeout=3)
            except Exception:pass
            raise
        self.client=client
    async def write(self,data):
        if not self.client or not self.client.is_connected:raise OSError('Bluetooth disconnected')
        # Write responses provide backpressure. Small chunks also work at MTU23.
        for offset in range(0,len(data),20):
            await self.client.write_gatt_char(RX,data[offset:offset+20],response=True)
    def ready(self):return bool(self.client and self.client.is_connected and not self.buffer.disconnected)
    def close(self):
        if self.client:
            try:self.run(self.client.disconnect(),5)
            except Exception:pass
        self.loop.call_soon_threadsafe(self.loop.stop);self.thread.join(timeout=2)
        self.loop.close()

_sessions={}
def close_bluetooth():
    for session in _sessions.values():session.close()
    _sessions.clear()
    from wifi_transport import close_wifi
    close_wifi()

class BluetoothPort:
    def __init__(self,device,baudrate=115200,timeout=.2,write_timeout=5):
        self.address=str(uuid.UUID(device.removeprefix('ble://')))
        self.session=_sessions.setdefault(self.address,None)
        if self.session is None:self.session=_sessions[self.address]=BluetoothSession(self.address)
        self.timeout=timeout;self.write_timeout=max(write_timeout,10)
        try:self.session.run(self.session.connect())
        except Exception as error:
            phase("Connection failed: "+type(error).__name__+": "+str(error))
            raise OSError("Bluetooth connection failed") from error
    def __enter__(self):return self
    def __exit__(self,*args):return False # Shared connection survives packet batches.
    def write(self,data):
        try:self.session.run(self.session.write(bytes(data)),self.write_timeout)
        except Exception as e:raise OSError('Bluetooth write failed: '+type(e).__name__) from e
        return len(data)
    def flush(self):pass # Writes already wait for GATT acknowledgement.
    def readline(self):return self.session.buffer.readline(self.timeout)
    def reset_input_buffer(self):
        with self.session.buffer.condition:self.session.buffer.data.clear()

def open_port(device,*args,**kwargs):
    if str(device).startswith('wifi://'):
        from wifi_transport import WifiPort
        return WifiPort(device,*args,**kwargs)
    if str(device).startswith('ble://'):return BluetoothPort(device,*args,**kwargs)
    import serial
    return serial.Serial(device,*args,**kwargs)

def connection(root):
    try:
        value=json.loads((Path(root)/'display-connection.json').read_text())
        if value.get('mode') not in ('usb','auto','bluetooth','wifi'):return {'mode':'usb'}
        if value.get('bluetooth_id'):value['bluetooth_id']=str(uuid.UUID(value['bluetooth_id']))
        if value.get('wifi_host'):
            from wifi_transport import host_from_device
            value['wifi_host']=host_from_device(value['wifi_host'])
        return value
    except (OSError,ValueError,TypeError,AttributeError):return {'mode':'usb'}

def usb_devices():
    from serial.tools import list_ports
    return [{'port':p.device,'serial':p.serial_number or '',
             'name':p.description or p.manufacturer or 'USB board'}
            for p in list_ports.comports() if p.vid in (0x2886,0x303a) and p.device.startswith('/dev/cu.usb')]

def serial_key(value):return ''.join(c.lower() for c in str(value) if c.isalnum())

_usb_cache=(0,[])
def selected_usb(settings,fallback):
    global _usb_cache
    wanted=settings.get('usb_serial')
    if not wanted:return settings.get('usb_device') or fallback
    now=time.monotonic()
    if not _usb_cache[0] or now-_usb_cache[0]>1:_usb_cache=(now,usb_devices())
    for device in _usb_cache[1]:
        if serial_key(device['serial'])==serial_key(wanted):return device['port']
    # USB paths may be reused by a different board. Remain disconnected when
    # the chosen serial is absent rather than opening that reused path.
    return '/dev/cu.xiao-missing-'+serial_key(wanted)

def endpoint(root,usb_device):
    settings=connection(root);mode=settings['mode'];identifier=settings.get('bluetooth_id')
    if mode=='wifi' and settings.get('wifi_host'):
        from wifi_transport import host_from_device
        return 'wifi://'+host_from_device(settings['wifi_host'])
    usb_device=selected_usb(settings,usb_device)
    if mode=='auto' and (not usb_device or not Path(usb_device).exists()):
        if settings.get('wifi_host'):
            from wifi_transport import host_from_device
            return 'wifi://'+host_from_device(settings['wifi_host'])
        binding=settings.get('bluetooth_usb_serial')
        if identifier and (not settings.get('usb_serial') or (binding and serial_key(binding)==serial_key(settings['usb_serial']))):return 'ble://'+identifier
    if identifier and mode=='bluetooth':return 'ble://'+identifier
    return usb_device

def is_connected(device):
    if not device:return False
    if str(device).startswith('wifi://'):
        from wifi_transport import is_connected as wifi_connected
        return wifi_connected(device)
    if str(device).startswith('ble://'):
        session=_sessions.get(str(uuid.UUID(device[6:])))
        return session is not None and session.ready()
    return Path(device).exists()

async def scan():
    from bleak import BleakScanner
    found=await BleakScanner.discover(timeout=7,service_uuids=[SERVICE],return_adv=True)
    return [{'id':d.address,'name':(a.local_name or d.name or 'XIAO Round Display')[:64]} for d,a in found.values() if SERVICE in [s.lower() for s in a.service_uuids]][:32]

if __name__=='__main__':
    import sys
    try:print(json.dumps({'devices':usb_devices() if '--usb' in sys.argv else asyncio.run(scan())}))
    except Exception as error:print(json.dumps({'error':type(error).__name__+': '+str(error)}))
