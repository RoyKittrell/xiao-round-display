#!/usr/bin/env python3
"""Exercise the actual registry and dispatcher without network/device access."""
import json,sys,tempfile,threading,time,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
from helper_runtime import Registry,Source
import display_bridge as bridge
import codex_usage,bambu_status
NOW=1794541000
quota={'observed_at':NOW,'windows':[{'label':'Weekly','remaining_tenths':130,'duration_mins':10080,'resets_at':NOW+36000}]}
printer={'observed_at':NOW,'state':2,'progress':30,'remaining_secs':3600,'finish_at':NOW+3600,'name':'Printer test'}
class Port:
 def __init__(self):self.packets=[];self.responses=[]
 def write(self,data):self.packets.append(data);self.responses.append(b'ACK '+data[1:2]+b'\n')
 def flush(self):pass
 def readline(self):return self.responses.pop(0) if self.responses else b''
class Runtime(unittest.TestCase):
 def test_independent_failure_and_blocking(self):
  blocked=threading.Event();fast=threading.Event();calls=[]
  def slow():blocked.wait(2);return {'slow':True}
  def failed():raise RuntimeError('test source unavailable')
  def quick():calls.append(1);fast.set();return {'count':len(calls)}
  reg=Registry([Source('slow','Slow',slow,1),Source('failed','Failed',failed,1),Source('fast','Fast',quick,.01)])
  try:
   reg.start();reg.connected.set();self.assertTrue(fast.wait(1))
   deadline=time.monotonic()+1
   while len(calls)<3 and time.monotonic()<deadline:time.sleep(.01)
   self.assertGreaterEqual(len(calls),3);self.assertIsNone(reg.states[0].read()[0]);self.assertIsNotNone(reg.states[1].error)
   self.assertGreaterEqual(reg.states[2].read()[1],2)
  finally:blocked.set();reg.close()
 def test_cached_replay_does_not_fake_freshness(self):
  with tempfile.TemporaryDirectory() as tmp:
   cache=Path(tmp)/'latest.json';cache.write_text(json.dumps(quota))
   source=Source('codex','Codex',lambda:None,60,encode=codex_usage.packet,cache=cache)
   reg=Registry([source]);self.assertEqual(reg.states[0].read()[0]['observed_at'],NOW)
   port=Port();d=bridge.Dispatcher(reg,'fake');d.hardware.value={'role':'desktop'}
   d.small(port=port);self.assertEqual(len(port.packets),1)
   d.small(port=port);self.assertEqual(len(port.packets),1)
   reg.reconnect();d.small(port=port);self.assertEqual(len(port.packets),2)
   self.assertEqual(reg.states[0].read()[0]['observed_at'],NOW);self.assertGreater(reg.states[0].delivered_at,0)
 def test_all_small_sources_interleave_with_image(self):
  port=Port();clock=[1.0]
  def transfer(*args,on_progress):
   for i in range(3):on_progress(port);clock[0]+=6
  reg=Registry([Source('codex','Codex',lambda:quota,60,encode=codex_usage.packet),Source('bambu','Bambu',lambda:printer,1,encode=bambu_status.packet),Source('sun','Sun',lambda:None,900,transfer=transfer)])
  reg.states[0].publish(quota);reg.states[1].publish(printer);reg.states[2].publish(([1],{}))
  d=bridge.Dispatcher(reg,'fake');d.hardware.value={'role':'desktop'}
  with tempfile.TemporaryDirectory() as tmp,patch.object(bridge,'ROOT',Path(tmp)),patch.object(bridge,'usb_lock',lambda:__import__('contextlib').nullcontext()),patch.object(bridge.time,'monotonic',lambda:clock[0]):
   d.images()
  self.assertEqual([p[1:2] for p in port.packets],[b'U',b'M']*3)
  self.assertEqual(reg.states[2].last_sent_version,1)
 def test_duplicate_ids_and_corrupt_cache(self):
  with self.assertRaises(ValueError):Registry([Source('x','X',lambda:None,1),Source('x','X',lambda:None,1)])
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)/'bad.json';p.write_text('invalid JSON')
   reg=Registry([Source('x','X',lambda:None,1,cache=p)]);self.assertIsNone(reg.states[0].read()[0]);self.assertIsNotNone(reg.states[0].error)
if __name__=='__main__':unittest.main()
