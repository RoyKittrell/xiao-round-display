#!/usr/bin/env python3
from pathlib import Path
import json,sys,tempfile,subprocess,binascii,unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
import display_control as c
class Port:
 def __init__(self,boot=123):self.sent=[];self.boot=boot;self.response=[];self.revision=0;self.day=0;self.night=0
 def write(self,p):
  self.sent.append(p);f=p[1:-1].decode().split('|')
  if int(f[2])!=self.revision:
   if f[12]=='1' and int(f[13])==self.boot:self.day=int(f[6]);self.night=int(f[7])
   self.revision=int(f[2])
  self.response=[f"ACK C|{f[2]}|2|{f[3]}|{f[4]}|{f[5]}|{self.day}|{self.night}|{f[8]}|0|{f[9]}|{self.boot}\n".encode()]
 def flush(self):pass
 def readline(self):return self.response.pop(0) if self.response else b''
class Controls(unittest.TestCase):
 def test_validation(self):
  for field,value in [('enabled_mask',-1),('enabled_mask',512),('day_brightness',101),('interval_seconds',0),('selected_scene',1)]:
   with self.assertRaises(ValueError):c.validate({field:value})
  self.assertEqual(len(json.loads((ROOT/'companion/scenes.json').read_text())),9)
 def test_brightness_is_explicit_and_not_replayed(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);p={**c.DEFAULTS,'day_brightness':80,'brightness_token':4};c.atomic(root/'display-preferences.json',p)
   driver=c.Control(root);port=Port();driver.send(port)
   self.assertEqual(port.sent[-1].decode().split('|')[-3],'0') # applyBrightness false at startup.
   p.update(revision=2,brightness_token=5,day_brightness=10);c.atomic(driver.path,p);driver.poll();driver.send(port)
   self.assertEqual(port.sent[-1].decode().split('|')[-3],'1');self.assertEqual(port.sent[-1].decode().split('|')[-2],'123')
   driver.next_send=0;driver.send(port);self.assertEqual(port.sent[-1].decode().split('|')[-3],'0')
   p.update(revision=3,brightness_token=6);c.atomic(driver.path,p);driver.poll();driver.reconnect();driver.send(port)
   self.assertEqual(port.sent[-1].decode().split('|')[-3],'0')
 def test_slider_action_before_first_ack_is_not_lost(self):
  with tempfile.TemporaryDirectory() as tmp:
   driver=c.Control(Path(tmp));port=Port()
   value={**c.DEFAULTS,'revision':2,'brightness_token':1,'day_brightness':25}
   c.atomic(driver.path,value);driver.poll();self.assertTrue(driver.pending_brightness)
   result=driver.send(port)
   self.assertEqual(len(port.sent),2);self.assertEqual(result['day_brightness'],25)
   self.assertNotEqual(port.sent[0].decode().split('|')[2],port.sent[1].decode().split('|')[2])
   driver.send(port);self.assertEqual(port.sent[-1].decode().split('|')[-3],'0')
 def test_atomic_bad_preferences_keep_last_good(self):
  with tempfile.TemporaryDirectory() as tmp:
   driver=c.Control(Path(tmp));driver.path.write_text('invalid');driver.poll();self.assertEqual(driver.value['enabled_mask'],511);self.assertIsNotNone(driver.error)
result=unittest.main(exit=False)
if not result.result.wasSuccessful():sys.exit(1)
payload=c.packet(c.DEFAULTS)[1:-1].decode()
empty=c.packet({**c.DEFAULTS,'enabled_mask':0})[1:-1].decode()
code=r'''
#include "SceneControl.h"
#include "DisplaySchedule.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(){
 SceneControl control;SceneControl::Settings value;
 assert(control.parse(PACKET,value));assert(value.mask==511&&value.day==0&&value.night==0&&!value.brightness);
 assert(control.next(10)==2);value.mask=257;value.revision=2;control.apply(value);assert(control.next(2)==10&&control.next(10)==2);
 assert(control.parse(EMPTY_CONTROL,value)&&value.mask==0);value.revision=3;control.apply(value);
 for(int scene=2;scene<=10;scene++)assert(control.next(scene)==scene);
 std::string bad=PACKET;bad[10]^=1;assert(!control.parse(bad.c_str(),value));
 assert(!control.displayOff(12*3600,true));assert(control.displayOff(3*3600,true));
 SceneRotation timer;timer.configure(60000,0xfffffff0u);assert(!timer.observe(uint32_t(0xfffffff0u+59999u),true));assert(timer.observe(uint32_t(0xfffffff0u+60000u),true));
 puts("PASS actual Python control encoder -> bounded firmware parser, CRC, filtered wrap and configurable interval rollover");
}
'''.replace('EMPTY_CONTROL',json.dumps(empty)).replace('PACKET',json.dumps(payload))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'check.cpp';p.write_text(code);exe=Path(tmp)/'check';subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT/'XiaoPlayground'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
