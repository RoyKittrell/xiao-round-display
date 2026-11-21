"""Read-only hardware identification, reconnect and transport-scoping tests."""
from pathlib import Path
import json,sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
from hardware_profile import identify,HardwareProfile
import display_transport as transport
class Hardware(unittest.TestCase):
 def test_firmware_profiles_and_legacy(self):
  for tag,family,name,wifi in [('esp32s3-scenes-r33-wifi','esp32s3','XIAO ESP32-S3',True),('esp32c6-scenes-r33-wifi','esp32c6','XIAO ESP32-C6',True),('nrf54-scenes-r30','nrf54','XIAO nRF54LM20A Sense',False),('public-template-r26','nrf54','XIAO nRF54LM20A Sense',False)]:
   v=identify(['FIRMWARE '+tag],'usb');self.assertEqual(v['family'],family);self.assertEqual(v['model'],name);self.assertEqual(v['wifi_hardware'],wifi);self.assertTrue(v['bluetooth'])
  old=identify(['FIRMWARE esp32c6-scenes-r30'],'usb');self.assertTrue(old['wifi_hardware']);self.assertFalse(old['wifi_firmware'])
 def test_legacy_control_packet_is_adapted_without_changing_preferences(self):
  import display_control as c
  profile=identify(['FIRMWARE public-template-r26'],'usb')
  self.assertEqual((profile['control_version'],profile['scene_count'],profile['clock_filters']),(1,9,False))
  prefs={**c.DEFAULTS,'selected_scene':11,'enabled_mask':1023}
  v1=c.packet(prefs,version=1)[1:].decode().strip().split('|')
  self.assertEqual((v1[1],v1[3],v1[10]),('1','511','-1'));self.assertEqual(len(v1),15)
  self.assertEqual(prefs['selected_scene'],11);self.assertEqual(prefs['enabled_mask'],1023)
  v2=c.packet(prefs)[1:].decode().strip().split('|');self.assertEqual((v2[1],v2[3],v2[10]),('2','1023','11'));self.assertEqual(len(v2),16)
 def test_unknown_is_conservative_not_guessed_from_usb_pid(self):
  v=identify(['BOARD something else','FIRMWARE other-project'],'usb');self.assertFalse(v['identified']);self.assertFalse(v['wifi_hardware']);self.assertFalse(v['bluetooth'])
  self.assertTrue(identify([], 'ble://selected')['bluetooth'])
 def test_probe_once_and_reprobe_after_device_or_boot_change(self):
  class Port:
   def __init__(self):self.writes=[];self.lines=[]
   def write(self,data):self.writes.append(data);self.lines=[b'FIRMWARE esp32s3-scenes-r33-wifi\n',b'WIFI configured=1 connected=1\n',b'SUN rows=0\n']
   def flush(self):pass
   def readline(self):return self.lines.pop(0) if self.lines else b''
  with tempfile.TemporaryDirectory() as tmp:
   h=HardwareProfile(tmp);p=Port();h.ensure(p,'wifi://one');h.ensure(p,'wifi://one');self.assertEqual(p.writes,[b'?\n'])
   h.observe_boot(1);h.ensure(p,'wifi://one');self.assertEqual(len(p.writes),1)
   h.observe_boot(2);h.ensure(p,'wifi://one');self.assertEqual(len(p.writes),2)
   h.ensure(p,'usb-two');self.assertEqual(len(p.writes),3)
   saved=json.loads((Path(tmp)/'hardware-status.json').read_text());self.assertEqual(saved['endpoint'],'usb-two');self.assertEqual(saved['family'],'esp32s3')
 def test_legacy_weather_is_not_sent_to_unsupported_firmware(self):
  from types import SimpleNamespace
  import threading
  from display_bridge import Dispatcher
  with tempfile.TemporaryDirectory() as tmp:
   with patch('display_bridge.ROOT',Path(tmp)):
    dispatcher=Dispatcher(None,'usb');dispatcher.hardware.value={'role':'desktop','scene_count':9}
    state=SimpleNamespace(source=SimpleNamespace(id='weather'),lock=threading.Lock(),usb_error=None,next_send=0)
    dispatcher._packets(None,[(state,{'unused':'data'},1)])
    self.assertEqual(state.usb_error,'Weather needs a firmware update');self.assertGreater(state.next_send,0)
 def test_auto_never_falls_back_to_another_boards_old_bluetooth_pair(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);path=root/'display-connection.json';uid='11111111-2222-3333-4444-555555555555'
   s={'mode':'auto','usb_serial':'aabbccddeeff','bluetooth_id':uid,'bluetooth_usb_serial':'001122334455'}
   with patch.object(transport,'selected_usb',return_value='/dev/cu.absent'):
    path.write_text(json.dumps(s));self.assertEqual(transport.endpoint(root,'unused'),'/dev/cu.absent')
    s['bluetooth_usb_serial']=s['usb_serial'];path.write_text(json.dumps(s));self.assertEqual(transport.endpoint(root,'unused'),'ble://'+uid)
    s['wifi_host']='xiao-display-ddeeff.local';path.write_text(json.dumps(s));self.assertEqual(transport.endpoint(root,'unused'),'wifi://'+s['wifi_host'])
if __name__=='__main__':unittest.main()
