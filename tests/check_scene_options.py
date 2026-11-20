"""Clock filtering and persisted location behavior, offline and permission-free."""
from pathlib import Path
import binascii,json,subprocess,sys,tempfile,threading,time,unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'companion'))
import display_control as c,weather_status as w
LOCATION={'city':'Example City','latitude':1.0,'longitude':2.0,'utc_offset_seconds':0}
from helper_runtime import Source,Registry

class Options(unittest.TestCase):
 def test_old_preferences_get_all_faces(self):
  self.assertEqual(c.validate({'revision':5})['clock_mask'],1023)
  for n in [-1,1024,True]:
   with self.assertRaises(ValueError):c.validate({'clock_mask':n})
 def test_cache_belongs_to_saved_location(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);(root/'assets/weather').mkdir(parents=True)
   (root/'weather-location.json').write_text(json.dumps(LOCATION))
   snapshot=w.pending(root);(root/'assets/weather/latest.json').write_text(json.dumps(snapshot))
   self.assertEqual(w.load_cache(root)['city'],'Example City')
   (root/'weather-location.json').write_text(json.dumps({**LOCATION,'city':'Other City','latitude':3.0,'longitude':4.0,'utc_offset_seconds':20700}))
   self.assertIsNone(w.load_cache(root))
   pending=w.pending(root);self.assertEqual(pending['probabilities'],[255]*30)
   self.assertEqual((pending['base']+pending['offset'])%3600,0)
 def test_location_change_wakes_long_interval_source(self):
  version=[0];seen=[];completed=threading.Event()
  def fetch():
   seen.append(version[0])
   if version[0]==1:completed.set()
   return {'value':version[0]}
  reg=Registry([Source('weather','Weather',fetch,900,trigger=lambda:version[0],invalidated=lambda:{'pending':True})])
  try:
   reg.start();reg.connected.set();deadline=time.monotonic()+1
   while not seen and time.monotonic()<deadline:time.sleep(.01)
   version[0]=1;self.assertTrue(completed.wait(1))
   self.assertEqual(seen,[0,1])
  finally:reg.close()

if __name__=='__main__':
 r=unittest.main(exit=False)
 if not r.result.wasSuccessful():sys.exit(1)
 packet=c.packet({**c.DEFAULTS,'clock_mask':(1<<0)|(1<<9)})[1:-1].decode()
 fields=packet.rsplit('|',1)[0].split('|');legacy='|'.join([fields[0],'1',*fields[2:-1]])
 legacy+='|'+f'{binascii.crc_hqx(legacy.encode(),0xffff):04x}'
 code=r'''
#include "SceneControl.h"
#include "ui/WatchCollection.h"
#include <cassert>
#include <cstdio>
int main(){
 SceneControl controls;SceneControl::Settings settings;
 assert(controls.parse(PACKET,settings)&&settings.clockMask==513);
 assert(controls.parse(LEGACY,settings)&&settings.clockMask==1023);
 controls.parse(PACKET,settings);settings.clockMask=0;controls.apply(settings);assert(controls.next(11)==3);
 WatchCollection clocks;clocks.setMask(513,0);assert(int(clocks.style)==0);
 for(int i=0;i<50;i++){clocks.next(i*100);assert(int(clocks.style)==(i%2?0:9));}
 clocks.setMask(1<<7,6000);assert(int(clocks.style)==7);clocks.next(6100);assert(int(clocks.style)==7);
 clocks.title.dismiss();clocks.demo=true;assert(!clocks.advanceDemo(60000));
 clocks.setMask(0,70000);assert(!clocks.enabledMask&&!clocks.demo);clocks.next(71000);assert(int(clocks.style)==7);
 clocks.setMask(1023,72000);assert(int(clocks.style)==7);clocks.next(73000);assert(int(clocks.style)==8);
 puts("PASS clock filtering: v1/v2 controls, subset wrap, current-face exclusion, zero/one/all, tour and re-enable.");
}
'''.replace('PACKET',json.dumps(packet)).replace('LEGACY',json.dumps(legacy))
 with tempfile.TemporaryDirectory(prefix='xiao-options-check-',dir='/tmp') as tmp:
  cpp=Path(tmp)/'test.cpp';cpp.write_text(code);exe=Path(tmp)/'check'
  subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT/'XiaoPlayground'),str(cpp),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
