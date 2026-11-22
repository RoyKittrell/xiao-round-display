"""Exercise host updater against a bounded fake transport, no boards or accounts."""
from pathlib import Path
from unittest.mock import patch
import hashlib,json,sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'companion'))
import firmware_update as f
from multi_device import device_root,maintenance_active
class Test(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);self.package=self.root/'package';self.package.mkdir();self.folder=self.package/'build/esp32s3/gym';self.folder.mkdir(parents=True)
  (self.package/'catalog.json').write_text(json.dumps({'schema_version':1,'profiles':[{'id':'gym','installer_families':['esp32s3']}]}))
  image=bytearray(30000);image[0]=0xe9;image[12:14]=(9).to_bytes(2,'little');image[32:36]=bytes.fromhex('3254cdab');image[176:208]=b'\x42'*32;image[500:524]=b'XIAO-PROFILE:gym:v1'+b' '*5
  self.data=bytes(image);self.elf=self.data[176:208].hex();self.ph=hashlib.sha256(b'p'*3072).hexdigest()
  for n,v in [('application.bin',self.data),('partitions.bin',b'p'*3072)]: (self.folder/n).write_bytes(v)
  (self.folder/'manifest.json').write_text(json.dumps({'schema_version':1,'profile':'gym','family':'esp32s3','chip':'esp32s3','offset':65536,'ota':1,'elf_sha256':self.elf,'files':{n:hashlib.sha256((self.folder/n).read_bytes()).hexdigest() for n in ('application.bin','partitions.bin')}}))
  self.id='usb:001122334455';self.written=[];self.connections=0;self.bad=None;self.committed=False;self.delay_reboot=False;self.same_image=False
 def session(self,*args,**kwargs):
  owner=self;owner.connections+=1;number=owner.connections
  class Session:
   def connect(self):pass
   def close(self):pass
   def write(self,data,timeout):
    owner.written.append(data)
    if not data.startswith(b'UPDATE '):owner.committed=True
   def readline(self,timeout):
    if not hasattr(self,'hello'):
     self.hello=True;serial='deadbeef0000' if owner.bad=='identity' else owner.id[4:];part='0'*64 if owner.bad=='layout' else owner.ph
     old=number==1 or (owner.delay_reboot and number==2)
     elf=owner.elf if not old or owner.same_image else '0'*64
     slot='app0' if old else 'app1'
     return f'OTA version=1 family=esp32s3 profile=gym serial={serial} elf={elf} partitions={part} capacity=3342336 slot={slot}\n'.encode()
    if not hasattr(self,'ready'):self.ready=True;return b'READY\n'
    return b'ERR hash\n' if owner.bad=='commit' else b'COMMITTED\n'
  return Session()
 def update(self,force=False):
  with patch.object(f,'paired_host',return_value='xiao-display-334455.local'),patch.object(f.time,'sleep'):
   return f.update(self.id,'gym',self.root,self.package,force=force,session_factory=self.session)
 def test_upload_then_verify_elf_and_cleanup_lease(self):
  result=self.update();self.assertTrue(result['verified']);self.assertEqual(result['elf'],self.elf)
  self.assertEqual(b''.join(self.written[1:]),self.data)
  self.assertFalse((device_root(self.root,self.id)/'firmware-maintenance.json').exists())
 def test_forced_same_image_waits_for_slot_change(self):
  self.delay_reboot=True;self.same_image=True
  self.assertTrue(self.update(force=True)['verified']);self.assertEqual(self.connections,3)
 def test_wrong_identity_or_layout_never_sends_image(self):
  for error in ('identity','layout'):
   self.connections=0;self.bad=error
   with self.assertRaises(ValueError):self.update()
   self.assertEqual(self.written,[])
 def test_uncommitted_image_never_records_success(self):
  self.bad='commit'
  with self.assertRaises(OSError):self.update()
  self.assertFalse((device_root(self.root,self.id)/'device-profile.json').exists())
 def test_stale_maintenance_cannot_pause_worker(self):
  target=self.root/'target';target.mkdir();(target/'firmware-maintenance.json').write_text(json.dumps({'pid':1,'until':0}));self.assertFalse(maintenance_active(target))
if __name__=='__main__':unittest.main()
