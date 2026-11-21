"""Exercise BLE stream semantics with fake GATT; no real radio or accounts."""
from pathlib import Path
import asyncio,json,sys,tempfile,unittest
from unittest.mock import patch
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
from display_transport import ReceiveBuffer,BluetoothSession,endpoint,connection
import display_transport as transport
class Transport(unittest.TestCase):
 def test_gym_auto_prefers_usb_then_wifi_and_reserves_bluetooth(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);usb=root/'usb';prefs=root/'display-connection.json'
   v={'mode':'auto','bluetooth_for_workout':'1','bluetooth_id':'12345678-1234-1234-1234-123456789abc','wifi_host':'xiao-display-123456.local'}
   prefs.write_text(json.dumps(v));usb.touch();self.assertEqual(endpoint(root,str(usb)),str(usb))
   usb.unlink();self.assertEqual(endpoint(root,str(usb)),'wifi://xiao-display-123456.local')
   del v['wifi_host'];prefs.write_text(json.dumps(v));self.assertEqual(endpoint(root,str(usb)),str(usb))
   v.pop('bluetooth_for_workout');prefs.write_text(json.dumps(v));self.assertTrue(endpoint(root,str(usb)).startswith('ble://'))

 def test_empty_template_has_no_connected_device(self):
  self.assertFalse(transport.is_connected(""))
 def test_usb_serial_selection_and_reused_port(self):
  settings={'usb_serial':'A0:F2:62:77:67:AC','usb_device':'/dev/cu.usbmodemOld'}
  with patch.object(transport,'_usb_cache',(0,[])),patch.object(transport,'usb_devices',return_value=[{'port':'/dev/cu.usbmodemNew','serial':'a0f2627767ac','name':'C6'}]):
   self.assertEqual(transport.selected_usb(settings,'/dev/cu.original'),'/dev/cu.usbmodemNew')
  with patch.object(transport,'_usb_cache',(0,[])),patch.object(transport,'usb_devices',return_value=[{'port':'/dev/cu.usbmodemOld','serial':'another-board','name':'Other'}]):
   self.assertTrue(transport.selected_usb(settings,'/dev/cu.original').startswith('/dev/cu.xiao-missing-'))
 def test_fragmented_lines_and_timeout_preserve_data(self):
  b=ReceiveBuffer();b.feed(b'ACK C|1|');self.assertEqual(b.readline(.001),b'')
  b.feed(b'2\nACK U\n');self.assertEqual(b.readline(.001),b'ACK C|1|2\n');self.assertEqual(b.readline(.001),b'ACK U\n')
 def test_disconnect_and_overflow_fail_closed(self):
  b=ReceiveBuffer(4);b.feed(b'12345')
  with self.assertRaises(OSError):b.readline(.001)
  b.reset();b.feed(b'OK\n');self.assertEqual(b.readline(.001),b'OK\n');b.lost()
  with self.assertRaises(OSError):b.readline(.001)
 def test_explicit_endpoint_and_usb_preference(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);usb=root/'usb';prefs=root/'display-connection.json';identifier='12345678-1234-1234-1234-123456789abc'
   self.assertEqual(endpoint(root,str(usb)),str(usb))
   prefs.write_text(json.dumps({'mode':'auto','bluetooth_id':identifier}))
   self.assertEqual(endpoint(root,str(usb)),'ble://'+identifier)
   self.assertEqual(endpoint(root,''),'ble://'+identifier)
   usb.touch();self.assertEqual(endpoint(root,str(usb)),str(usb))
   prefs.write_text(json.dumps({'mode':'bluetooth','bluetooth_id':identifier}));self.assertEqual(endpoint(root,str(usb)),'ble://'+identifier)
   prefs.write_text('{bad');self.assertEqual(connection(root),{'mode':'usb'})
   prefs.write_text(json.dumps({'mode':'bluetooth','bluetooth_id':'bad'}));self.assertEqual(connection(root),{'mode':'usb'})
 def test_chunking_and_gatt_response_backpressure(self):
  class Client:
   is_connected=True
   def __init__(self):self.chunks=[]
   async def write_gatt_char(self,characteristic,data,response):self.chunks.append((characteristic,data,response))
  s=object.__new__(BluetoothSession);s.client=Client();data=bytes(range(250))
  asyncio.run(s.write(data));self.assertEqual(b''.join(p[1] for p in s.client.chunks),data)
  self.assertTrue(all(len(p[1])<=20 and p[2] for p in s.client.chunks));s.client.is_connected=False
  with self.assertRaises(OSError):asyncio.run(s.write(b'abc'))
 def test_overflow_reconnects_and_cancelled_connect_cleans_up(self):
  clients=[]
  class Client:
   def __init__(self,*a,**kw):self.is_connected=False;self.closed=0;self.block=False;clients.append(self)
   async def connect(self):self.is_connected=True
   async def disconnect(self):self.closed+=1;self.is_connected=False
   async def start_notify(self,*a):pass
   async def read_gatt_char(self,*a):return b''
   async def write_gatt_char(self,*a,**kw):pass
  class Scanner:
   @staticmethod
   async def find_device_by_address(*a,**kw):return 'synthetic display'
  s=object.__new__(BluetoothSession);s.address='synthetic';s.buffer=ReceiveBuffer();old=Client();old.is_connected=True;s.client=old;s.buffer.lost()
  with patch.dict(sys.modules,{'bleak':SimpleNamespace(BleakClient=Client,BleakScanner=Scanner)}):
   asyncio.run(s.connect());self.assertEqual(old.closed,1);self.assertTrue(s.ready())
   async def cancelled_connect():
    s.client=None
    async def stalled(client):await asyncio.Future()
    with patch.object(Client,'connect',stalled):
     task=asyncio.create_task(s.connect());await asyncio.sleep(.01);task.cancel()
     with self.assertRaises(asyncio.CancelledError):await task
    self.assertEqual(clients[-1].closed,1)
   asyncio.run(cancelled_connect())
if __name__=='__main__':unittest.main()
