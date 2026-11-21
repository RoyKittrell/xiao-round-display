"""Purpose installation contract, using synthetic firmware/USB and real installer."""
from pathlib import Path
import hashlib,json,sys,tempfile,unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'companion'))
import device_profiles as d
from multi_device import device_root
class Profiles(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);self.package=self.root/'package';self.package.mkdir()
  (self.package/'catalog.json').write_text(json.dumps({'schema_version':1,'profiles':[{'id':'gym','installer_families':['esp32s3']}]}))
  self.folder=self.package/'build/esp32s3/gym';self.folder.mkdir(parents=True)
  for name,data in [('application.bin',b'x'*2048),('partitions.bin',b'p'*3072)]: (self.folder/name).write_bytes(data)
  (self.folder/'manifest.json').write_text(json.dumps({'schema_version':1,'profile':'gym','family':'esp32s3','chip':'esp32s3','offset':65536,'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in self.folder.iterdir()}}))
  (self.package/'tools').mkdir();(self.package/'tools/esptool').touch();(self.package/'tools/esptool').chmod(0o755)
  self.id='usb:001122334455';self.calls=[];self.bad_layout=False;self.mac='00:11:22:33:44:55'
 def rom(self,tool,serial,args,reset='no-reset'):
  self.calls.append((args,reset));self.assertEqual(serial,'001122334455')
  if args[0]=='read-mac':return 'MAC: '+self.mac+'\n'
  if args[0]=='read-flash':
   data=(b'bad' if self.bad_layout else b'p'*3072+b'\xff'*1024) if args[1]=='0x8000' else b'old application'
   Path(args[-1]).write_bytes(data)
  return 'Hash of data verified.'
 def install(self,probes=None):
  with patch.object(d,'port_for',return_value='/dev/cu.synthetic'),patch.object(d,'probe',side_effect=probes or [{'identified':True,'family':'esp32s3','role':'desktop'},{'identified':True,'family':'esp32s3','role':'gym'}]),patch.object(d,'run_tool',side_effect=self.rom):
   return d.install(self.id,'gym',self.root,self.package)
 def test_valid_application_only_install_and_recovery(self):
  peer=device_root(self.root,'usb:aabbccddeeff');peer.mkdir(parents=True);(peer/'settings').write_text('unchanged')
  out=self.install();self.assertTrue(out['verified'])
  writes=[a for a,r in self.calls if a[0]=='write-flash'];self.assertEqual(len(writes),1);self.assertEqual(writes[0][1],'0x10000')
  backup=list(device_root(self.root,self.id).glob('private/profile-recovery/*/application.bin'));self.assertEqual(backup[0].read_bytes(),b'old application')
  self.assertEqual((peer/'settings').read_text(),'unchanged')
  self.assertEqual(json.loads((device_root(self.root,self.id)/'device-profile.json').read_text())['profile'],'gym')
 def test_wrong_mac_never_writes_and_restores_boot(self):
  self.mac='aa:bb:cc:dd:ee:ff'
  with self.assertRaisesRegex(ValueError,'ROM MAC'):self.install()
  self.assertFalse(any(a[0]=='write-flash' for a,r in self.calls));self.assertEqual(self.calls[-1][1],'hard-reset')
 def test_wrong_partition_never_writes_or_records_success(self):
  self.bad_layout=True
  with self.assertRaisesRegex(ValueError,'partition layout'):self.install()
  self.assertFalse(any(a[0]=='write-flash' for a,r in self.calls));self.assertEqual(self.calls[-1][1],'hard-reset')
  self.assertFalse((device_root(self.root,self.id)/'device-profile.json').exists())
 def test_checksum_failure_precedes_usb_access(self):
  (self.folder/'application.bin').write_bytes(b'tampered')
  with patch.object(d,'port_for') as usb:
   with self.assertRaisesRegex(ValueError,'checksum'):self.install()
   usb.assert_not_called()
 def test_unknown_firmware_never_flashes(self):
  with self.assertRaisesRegex(ValueError,'positively identify'):self.install([{'identified':False,'family':'unknown','role':'unknown'}])
  self.assertFalse(self.calls)
 def test_same_profile_is_noop(self):
  self.assertFalse(self.install([{'identified':True,'family':'esp32s3','role':'gym'}])['changed']);self.assertFalse(self.calls)
 def test_invalid_identity_and_unsupported_profile(self):
  with self.assertRaises(ValueError):d.install('wifi:any','gym',self.root,self.package)
  with self.assertRaises(ValueError):d.module('unregistered','esp32s3',self.package)
  with self.assertRaises(ValueError):d.module('gym','nrf54',self.package)
if __name__=='__main__':unittest.main()
