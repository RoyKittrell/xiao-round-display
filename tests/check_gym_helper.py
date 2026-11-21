"""Production Gym routing/state/identity isolation; no hardware/accounts/network."""
from pathlib import Path
import contextlib,json,sys,tempfile,unittest
from unittest.mock import patch,Mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
from gym_control import GymControl,parse_status,commands
from hardware_profile import identify,HardwareProfile
from device_registry import DeviceRegistry,Selection,SelectionChanged,atomic
import display_transport as transport
import display_bridge as bridge
from helper_runtime import Source,Registry
SERIAL='001122334455';ID='usb:'+SERIAL

def settings(root,serial=SERIAL,epoch='one'):
 v={'mode':'usb','usb_serial':serial,'usb_device':'/dev/cu.synthetic','device_id':'usb:'+serial,'selection_epoch':epoch}
 atomic(root/'display-connection.json',v);return v
class Port:
 def __init__(self):self.sent=[];self.lines=[];self.glow={'peak':32,'centre':29,'high_floor':68};self.brightness=77;self.demo=True;self.on_ui=None;self.title=None
 def __enter__(self):return self
 def __exit__(self,*a):pass
 def reset_input_buffer(self):pass
 def flush(self):pass
 def status(self):
  return ([b'TITLE hex='+self.title.encode('ascii').hex().encode()+b'\n'] if self.title is not None else [])+[b'CLOSE,1234,0,1,100\n',b'STATUS firmware=bike-dashboard-r10 chip=554433221100 storage=OK\n',f'DEMO active={int(self.demo)} pulses=10 rate=75.0\n'.encode(),f'GLOW peak={self.glow["peak"]} centre={self.glow["centre"]} high_floor={self.glow["high_floor"]}\n'.encode(),f'BRIGHTNESS level={self.brightness}\n'.encode(),b'UI ready=1 resistance=4 touch_ok=1\n']
 def write(self,data):
  self.sent.append(data)
  if data==b'STATUS\n':self.lines+=self.status()
  elif data==b'?\n':self.lines.append(b'Commands: STATUS, DEMO\n')
  elif data.startswith(b'GLOW '):
   a,b,c=map(int,data.split()[1:]);self.glow={'peak':a,'centre':b,'high_floor':c};self.lines.append(f'GLOW peak={a} centre={b} high_floor={c}\n'.encode())
  elif data.startswith(b'TITLE '):self.title=bytes.fromhex(data.decode().split()[1]).decode('ascii');self.lines.append(data)
  elif data.startswith(b'BRIGHTNESS '):self.brightness=int(data.split()[1]);self.lines.append(f'BRIGHTNESS level={self.brightness}\n'.encode())
  elif data in (b'DEMO ON\n',b'DEMO OFF\n'):self.demo=data==b'DEMO ON\n';self.lines.append(data.strip()+b': display only\n')
  else:raise AssertionError('Unexpected protocol write')
 def readline(self):
  line=self.lines.pop(0) if self.lines else b''
  if line.startswith(b'UI ') and self.on_ui:self.on_ui();self.on_ui=None
  return line
class GymTests(unittest.TestCase):
 def test_title_action_roundtrip_changes_only_title(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root);g=GymControl(root);p=Port();p.title="Bike Lab"
   atomic(root/'gym-actions.json',{ID:{'token':'title','target':ID,'selection':'one','values':{'title':"My Bike Lab"}}})
   g.consumed={};g.service(p,Selection(root))
   self.assertEqual(g.value['title'],"My Bike Lab")
   self.assertEqual(g.value['last_token'],'title')
   self.assertEqual(p.brightness,77);self.assertEqual(p.glow['peak'],32)
 def test_old_firmware_rejects_title_action_without_write(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root);g=GymControl(root);p=Port()
   atomic(root/'gym-actions.json',{ID:{'token':'title','target':ID,'selection':'one','values':{'title':'Bike Lab'}}})
   g.consumed={};g.service(p,Selection(root))
   self.assertIn('firmware',g.value['error']);self.assertEqual(p.sent,[b'STATUS\n'])

 def test_identification_is_positive_and_usb_only(self):
  p=identify(['STATUS firmware=bike-dashboard-r10 chip=554433221100'],'usb')
  self.assertEqual((p['role'],p['chip_serial'],p['family']),('gym',SERIAL,'esp32s3'));self.assertFalse(p['wifi_firmware']);self.assertFalse(p['bluetooth'])
  self.assertEqual(identify(['FIRMWARE unknown'],'usb')['role'],'unknown')
  self.assertEqual(identify(['FIRMWARE esp32s3-scenes-r33-wifi'],'usb')['role'],'desktop')
  with tempfile.TemporaryDirectory() as tmp:
   h=HardwareProfile(tmp);port=Port();v=h.ensure(port,'fake')
   self.assertEqual(v['role'],'gym');self.assertEqual(port.sent,[b'?\n',b'STATUS\n'])
 def test_r11_wireless_capabilities_require_explicit_flags(self):
  base='STATUS firmware=bike-dashboard-r11 chip=554433221100'
  for flags,wifi,ble in [('',False,False),(' wifi_supported=1 ble_supported=1 ble_ready=1',True,True),(' wifi_supported=1 ble_supported=1 ble_ready=0',True,False)]:
   v=identify([base+flags],'usb');self.assertEqual(v['wifi_firmware'],wifi);self.assertEqual(v['bluetooth'],ble);self.assertEqual(v['role'],'gym')
 def test_watch_capability_and_real_sensor_calibration_status(self):
  p=identify(['STATUS firmware=bike-dashboard-r12 chip=554433221100 wifi_supported=1 ble_supported=1 ble_ready=1 watch_csc=1'],'usb')
  self.assertTrue(p['watch_csc']);self.assertEqual(p['role'],'gym')
  lines=[x.decode().strip() for x in Port().status()]+['CSC ready=1 subscribed=1 revolutions=7 event_time=8192 ppr=1']
  v=parse_status(lines);self.assertTrue(v['watch_streaming']);self.assertEqual(v['pulses_per_revolution'],1)
 def test_real_pedal_telemetry_exposes_counts_without_confusing_demo(self):
  p=Port();lines=[x.decode().strip() for x in p.status()]
  lines[1]='STATUS firmware=bike-dashboard-r13 chip=554433221100 closures=10 transitions=20 level=1 storage=OK full=0'
  lines[2]='DEMO active=0 pulses=999 rate=62.5'
  lines += ['CSC ready=1 subscribed=1 revolutions=10 event_time=10240 ppr=1','BLE ready=1 connected=1 encrypted=1','UI ready=1 resistance=6 touch_ok=1 sensor_task=1 drops=0 fps=21.2']
  v=parse_status(lines);self.assertEqual(v['closures'],10);self.assertEqual(v['crank_revolutions'],10);self.assertEqual(v['pulse_rate'],62.5);self.assertFalse(v['demo']);self.assertEqual(v['drops'],0);self.assertTrue(v['watch_link_connected'])
 def test_interleaved_status_and_strict_command_bounds(self):
  p=Port();v=parse_status([x.decode().strip() for x in p.status()]);self.assertTrue(v['ready']);self.assertEqual(v['glow'],p.glow)
  self.assertFalse(parse_status([x.decode().strip() for x in p.status() if not x.startswith(b'GLOW')])['ready'])
  self.assertEqual(commands({'glow':p.glow,'brightness':0,'demo':False}),[b'GLOW 32 29 68\n',b'BRIGHTNESS 0\n',b'DEMO OFF\n'])
  for v in [{'brightness':True},{'demo':1},{'brightness':101},{'glow':{'peak':0,'centre':29,'high_floor':68}},{'glow':{'peak':32,'centre':4,'high_floor':68}},{'erase':True}]:
   with self.assertRaises(ValueError):commands(v)
 def test_device_authority_and_partial_glow_merge(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root);b=Selection(root);g=GymControl(root);g.begin_session(b);p=Port();p.glow['centre']=50
   atomic(root/'gym-actions.json',{ID:{'target':ID,'selection':'one','token':'new','values':{'glow':{'peak':35},'demo':False}}})
   result=g.service(p,b);self.assertIn(b'GLOW 35 50 68\n',p.sent);self.assertFalse(result['demo']);self.assertEqual(result['last_token'],'new');self.assertEqual(result['brightness'],77)
 def test_no_startup_or_reconnect_replay(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root);b=Selection(root)
   action={ID:{'target':ID,'selection':'one','token':'old','values':{'brightness':0}}};atomic(root/'gym-actions.json',action)
   g=GymControl(root);p=Port();g.service(p,b);self.assertEqual(p.sent,[b'STATUS\n']);self.assertEqual(p.brightness,77)
   action[ID]['token']='queued-before-reconnect';atomic(root/'gym-actions.json',action);g.begin_session(b);g.service(p,b);self.assertEqual(p.sent,[b'STATUS\n']*2)
 def test_selection_switch_before_setting_aborts(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root);b=Selection(root);g=GymControl(root);p=Port()
   atomic(root/'gym-actions.json',{ID:{'target':ID,'selection':'one','token':'new','values':{'brightness':0}}})
   p.on_ui=lambda:settings(root,serial='aabbccddeeff',epoch='two')
   with self.assertRaises(SelectionChanged):g.service(p,b)
   self.assertEqual(p.sent,[b'STATUS\n']);self.assertEqual(p.brightness,77)
 def test_wrong_chip_is_rejected_before_any_action(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root,serial='aabbccddeeff');g=GymControl(root);p=Port()
   with self.assertRaises(SelectionChanged):g.service(p,Selection(root))
   self.assertEqual(p.sent,[b'STATUS\n'])
 def test_reused_usb_path_cannot_be_opened(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root);b=Selection(root)
   with patch('device_registry.usb_devices',return_value=[{'port':'/dev/cu.synthetic','serial':'aabbccddeeff'}]),patch('serial.Serial') as serial:
    transport.set_selection_guard(b)
    try:
     with self.assertRaises(SelectionChanged):transport.open_port('/dev/cu.synthetic')
     serial.assert_not_called()
    finally:transport.set_selection_guard(None)
 def test_wireless_pairing_cannot_select_another_board(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);v=settings(root);v.update(wifi_host='xiao-display-ddeeff.local',bluetooth_id='11111111-2222-3333-4444-555555555555',bluetooth_usb_serial='aabbccddeeff');atomic(root/'display-connection.json',v)
   pairing=root/'private/wifi/xiao-display-ddeeff.local.json';atomic(pairing,{'serial':'aabbccddeeff'})
   b=Selection(root)
   with self.assertRaises(SelectionChanged):b.verify_wireless('wifi://xiao-display-ddeeff.local')
   with self.assertRaises(SelectionChanged):b.verify_wireless('ble://'+v['bluetooth_id'])
 def test_guard_aborts_in_progress_multi_packet_transfer(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);settings(root);p=Mock();guard=transport.GuardedPort(p,Selection(root));guard.write(b'first')
   settings(root,serial='aabbccddeeff',epoch='next')
   with self.assertRaises(SelectionChanged):guard.write(b'second')
   p.write.assert_called_once_with(b'first')
 def test_unknown_and_gym_cannot_enter_desktop_dispatch(self):
  reg=Registry([Source('x','X',lambda:None,1,encode=Mock(),transfer=Mock())]);reg.states[0].publish({'value':1})
  for role in ('unknown','gym'):
   d=bridge.Dispatcher(reg,'fake',control=Mock());d.hardware.value={'role':role}
   with patch.object(bridge,'sync_clock') as sync,patch.object(bridge,'open_port') as opened:
    d.small();d.images();d._packets(Mock(),[(reg.states[0],{},1)]);sync.assert_not_called();opened.assert_not_called();d.control.send.assert_not_called()
 def test_reprobe_role_change_stops_desktop_clock_and_control(self):
  reg=Registry([]);control=Mock();control.due.return_value=True;d=bridge.Dispatcher(reg,'fake',control=control);d.hardware.value={'role':'desktop'};d.hardware.ensure=Mock(return_value={'role':'gym'})
  with patch.object(bridge,'usb_lock',return_value=contextlib.nullcontext()),patch.object(bridge,'open_port',return_value=Port()),patch.object(bridge,'sync_clock') as sync:
   d.small();sync.assert_not_called();control.send.assert_not_called()
 def test_registry_retains_offline_identity(self):
  with tempfile.TemporaryDirectory() as tmp:
   r=DeviceRegistry(tmp);v={'usb_serial':SERIAL,'device_id':ID}
   with patch('device_registry.usb_devices',return_value=[{'serial':SERIAL,'port':'/dev/cu.one','name':'USB'}]):r.refresh(v,{'role':'gym','model':'Gym Xiao','family':'esp32s3','firmware':'bike-dashboard-r10'})
   with patch('device_registry.usb_devices',return_value=[]):r.refresh(v)
   d=json.loads((Path(tmp)/'device-catalog.json').read_text())['devices'][ID];self.assertEqual(d['role'],'gym');self.assertFalse(d['usb_present'])
if __name__=='__main__':unittest.main()
