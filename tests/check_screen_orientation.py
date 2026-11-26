"""Real firmware transforms/parser and device-scoped helper actions; no hardware."""
from pathlib import Path
from unittest.mock import patch
import json,sys,tempfile,unittest,subprocess
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
from screen_orientation import Orientation,packet
from device_registry import atomic,SelectionChanged

class Binding:
    id='usb:001122334455';epoch='test'
    def check(self):pass
class Port:
    def __init__(self,turns=0):self.turns=turns;self.sent=[];self.lines=[]
    def write(self,data):
        self.sent.append(data);wanted=int(data[3:4])
        if wanted<4:self.turns=wanted
        self.lines.append(f'ORIENTATION turns={self.turns}\n'.encode())
    def flush(self):pass
    def readline(self):return self.lines.pop(0) if self.lines else b''

class Tests(unittest.TestCase):
    def test_absolute_preference_is_idempotent_and_survives_reconnect(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);m=Orientation(root);p=Port(2);b=Binding()
            self.assertEqual(m.service(p,b)['turns'],2)
            atomic(m.settings,{'device_id':b.id,'turns':3,'token':'right'})
            self.assertEqual(m.service(p,b)['turns'],3)
            m.service(p,b);self.assertEqual(p.sent.count(packet(3)),1)
            p.turns=0;m.reconnect();self.assertEqual(m.service(p,b)['turns'],3)
            self.assertEqual(json.loads(m.status.read_text())['token'],'right')
    def test_another_device_cannot_receive_saved_rotation(self):
        with tempfile.TemporaryDirectory() as tmp:
            m=Orientation(tmp);p=Port();b=Binding()
            atomic(m.settings,{'device_id':'usb:aabbccddeeff','turns':1})
            with self.assertRaises(ValueError):m.service(p,b)
            self.assertEqual(p.sent,[])
    def test_binding_change_blocks_write(self):
        with tempfile.TemporaryDirectory() as tmp:
            m=Orientation(tmp);p=Port();b=Binding()
            with patch.object(b,'check',side_effect=SelectionChanged()):
                with self.assertRaises(SelectionChanged):m.service(p,b)
            self.assertEqual(p.sent,[])
    def test_real_cpp_mapping_calibration_and_crc_parser(self):
        requests=','.join(json.dumps(packet(n)[1:-1].decode()) for n in range(5))
        code=r'''
#include <cassert>
#include "ScreenOrientation.h"
#include "TouchCalibration.h"
int main(){
 const char* packets[]={PACKETS};
 for(int n=0;n<5;n++){uint8_t value=9;assert(ScreenOrientation::parse(packets[n],value)&&value==n);}
 for(const char* bad:{"O|5|0000","O|1|0000","O|0|ffffextra","O|0","O|-1|0000"}){uint8_t value;assert(!ScreenOrientation::parse(bad,value));}
 for(int r=0;r<4;r++){
  ScreenOrientation rotation;rotation.set(r);
  for(int y=0;y<240;y++)for(int x=0;x<240;x++){
   auto physical=ScreenOrientation::toPanel(x,y,r),back=rotation.touch(physical.x,physical.y);
   assert(back.x==x&&back.y==y);
  }
  assert(rotation.touch(-1,-1).x==-1);
  for(int calibration=0;calibration<8;calibration++){
   TouchCalibration c;c.begin(r);uint32_t now=1000;
   for(int i=0;i<4;i++){
    auto t=TouchCalibration::target(i);auto raw=ScreenOrientation::toPanel(t.x,t.y,r);
    if(calibration&1)raw.x=239-raw.x;if(calibration&2)raw.y=239-raw.y;
    if(calibration&4){int v=raw.x;raw.x=raw.y;raw.y=v;}
    c.update(true,raw.x,raw.y,now);c.update(true,raw.x,raw.y,now+40);c.update(true,raw.x,raw.y,now+80);c.update(true,raw.x,raw.y,now+150);
    c.update(false,raw.x,raw.y,now+160);c.update(false,raw.x,raw.y,now+240);now+=300;
   }
   assert(!c.active()&&c.result==calibration&&c.error==0);
  }
 }
 ScreenOrientation r;r.set(1);auto top=r.touch(239,120);assert(top.x==120&&top.y==0);
}
'''.replace('PACKETS',requests).replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'test.cpp').write_text(code)
            subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(ROOT/'XiaoPlayground'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)

if __name__=='__main__':unittest.main()
