#!/usr/bin/env python3
from pathlib import Path
import binascii,json,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
import bambu_status as b
NOW=1794538800
class Printer(unittest.TestCase):
 def test_active_delta(self):
  m=b.Model();self.assertFalse(m.merge({'print':{'nozzle_temper':210}},NOW))
  self.assertTrue(m.merge({'print':{'gcode_state':'RUNNING','mc_percent':12,'mc_remaining_time':108,'subtask_name':'Long_print.gcode.3mf'}},NOW))
  s=m.snapshot(NOW);self.assertEqual((s['state'],s['progress'],s['finish_at'],s['name']),(b.PRINTING,12,NOW+6480,'Long print'))
  m.merge({'print':{'mc_percent':13}},NOW+10);s=m.snapshot(NOW+10);self.assertEqual(s['remaining_secs'],6470);self.assertEqual(s['name'],'Long print');self.assertEqual(s['observed_at'],NOW+10)
  m.merge({'print':{'mc_remaining_time':108}},NOW+20);self.assertEqual(m.snapshot(NOW+20)['finish_at'],NOW+6480)
 def test_pause_resume(self):
  m=b.Model();m.merge({'print':{'gcode_state':'RUNNING','mc_remaining_time':10}},NOW)
  m.merge({'print':{'gcode_state':'PAUSE'}},NOW+10);self.assertEqual(m.snapshot(NOW+1000)['finish_at'],0)
  m.merge({'print':{'gcode_state':'RUNNING'}},NOW+1000);self.assertEqual(m.snapshot(NOW+1000)['finish_at'],NOW+1600)
 def test_new_job(self):
  m=b.Model();m.merge({'print':{'gcode_state':'FINISH','task_id':'1','mc_percent':100,'subtask_name':'Old'}},NOW)
  m.merge({'print':{'gcode_state':'PREPARE','task_id':'2','subtask_name':'New'}},NOW+1);s=m.snapshot(NOW+1);self.assertEqual(s['progress'],-1);self.assertEqual(s['remaining_secs'],-1);self.assertEqual(s['name'],'New')
 def test_idle_unknown_and_invalid(self):
  m=b.Model();self.assertIsNone(m.snapshot(NOW));self.assertFalse(m.merge({'print':{'gcode_state':'UNRECOGNISED'}},NOW))
  m.merge({'print':{'gcode_state':'IDLE','mc_percent':99,'mc_remaining_time':9,'subtask_name':'old'}},NOW);self.assertEqual(m.snapshot(NOW)['name'],'');self.assertEqual(m.snapshot(NOW)['progress'],-1)
  m.merge({'print':{'gcode_state':'RUNNING','mc_percent':1000,'mc_remaining_time':-1}},NOW+1);self.assertEqual(m.snapshot(NOW+1)['progress'],-1);self.assertEqual(m.snapshot(NOW+1)['finish_at'],0)
 def test_no_false_finish(self):
  m=b.Model();m.merge({'print':{'gcode_state':'RUNNING','mc_remaining_time':0,'mc_percent':99}},NOW);self.assertEqual(m.snapshot(NOW+100)['state'],b.PRINTING)
 def test_title_and_wire(self):
  self.assertEqual(b.title('a_very\nlong.gcode.3mf'),'a very long');self.assertEqual(len(b.title('x'*1000)),95)
  s={'observed_at':NOW,'state':b.PRINTING,'progress':50,'remaining_secs':600,'finish_at':NOW+600,'name':'|@\nTest'}
  p=b.packet(s);body,crc=p[1:-1].rsplit(b'|',1);self.assertEqual(int(crc,16),binascii.crc_hqx(body,0xffff));self.assertEqual(len(body.split(b'|')),7)
result=unittest.main(exit=False)
if not result.result.wasSuccessful():sys.exit(1)
value={'observed_at':NOW,'state':b.PRINTING,'progress':12,'remaining_secs':6480,'finish_at':NOW+6480,'name':'EXAMPLE-CALIBRATION-PART-WITH-A-DELIBERATELY-LONG-SYNTHETIC-NAME-FOR-LAYOUT-TESTING'}
wire=b.packet(value)[1:-1].decode()
invalid=[]
for key,v in [('progress',101),('progress',-2),('remaining_secs',-2),('remaining_secs',3000000),('state',8),('finish_at',1)]:
 copy=dict(value);copy[key]=v;invalid.append(b.packet(copy)[1:-1].decode())
code=r'''
#include "PrinterRenderer.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
int main(){
 uint32_t now=1794538800;PrinterStatus s;std::vector<uint16_t> guarded(57602,0x7654);auto out=guarded.data()+1;
 assert(!s.ready()&&s.stale());PrinterView::render(out,s,now);
 assert(s.accept(PACKET,0xffffff00u,now));assert(s.current().state==PrinterStatus::Printing&&s.current().progress==12&&s.current().finish==now+6480);
 s.tick(uint32_t(0xffffff00u+89000u));assert(s.age()==89&&!s.stale());s.tick(uint32_t(0xffffff00u+90000u));assert(s.age()==90&&s.stale());
 auto revision=s.revision();for(const char* bad : {INVALID})assert(!s.accept(bad,0,now));assert(s.revision()==revision);
 std::string corrupt=PACKET;corrupt[12]^=1;assert(!s.accept(corrupt.c_str(),0,now));assert(!s.accept(PACKET,0,now-300));
 char a[100],b[100];PrinterView::nameLines(s.current().name,a,b);assert(WatchFace::Detail::textWidth(ClockFonts::Small,a)<=180&&WatchFace::Detail::textWidth(ClockFonts::Small,b)<=180);assert(std::string(b).find("...")!=std::string::npos);
 for(auto t : {now,now+6480,now+100000})PrinterView::render(out,s,t);
 assert(guarded.front()==0x7654&&guarded.back()==0x7654);
 puts("PASS actual Python packet -> firmware state/CRC/range validation, monotonic freshness, millis rollover, pixel-fitted title and render guards");
}
'''.replace('PACKET',json.dumps(wire)).replace('INVALID',','.join(json.dumps(x) for x in invalid))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'check.cpp';p.write_text(code);exe=Path(tmp)/'check'
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT/'XiaoPlayground'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
