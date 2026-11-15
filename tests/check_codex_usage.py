#!/usr/bin/env python3
from pathlib import Path
import binascii,copy,importlib.util,json,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'companion'))
import codex_usage as usage
NOW=1793620000
class Quota(unittest.TestCase):
 def raw(self):return {'rateLimitsByLimitId':{'codex':{'primary':{'usedPercent':89,'windowDurationMins':10080,'resetsAt':NOW+100000},'secondary':None}},'rateLimits':{'primary':{'usedPercent':1}}}
 def test_normalise(self):
  s=usage.normalise(self.raw(),NOW);self.assertEqual(s['windows'],[{'label':'Weekly','remaining_tenths':110,'duration_mins':10080,'resets_at':NOW+100000}]);self.assertEqual(s['observed_at'],NOW)
 def test_legacy_and_missing(self):
  raw=self.raw();raw['rateLimits']=raw.pop('rateLimitsByLimitId')['codex'];self.assertEqual(usage.normalise(raw,NOW)['windows'][0]['remaining_tenths'],110)
  for raw in ({},{'rateLimits':{'primary':None}}, {'rateLimitsByLimitId':{'codex':{'primary':{'usedPercent':None}}}}):
   with self.assertRaises(RuntimeError):usage.normalise(raw,NOW)
 def test_multi(self):
  raw=self.raw();raw['rateLimitsByLimitId']['codex']['secondary']={'usedPercent':20.5,'windowDurationMins':300,'resetsAt':NOW+5000}
  raw['rateLimitsByLimitId']['codex_other|\n']={'limitName':'Fancy|model\n','primary':{'usedPercent':-3,'windowDurationMins':60,'resetsAt':NOW+6000}}
  s=usage.normalise(raw,NOW);self.assertEqual(len(s['windows']),3);self.assertEqual(s['windows'][1]['remaining_tenths'],795)
  self.assertEqual(s['windows'][2]['remaining_tenths'],1000);self.assertNotIn('|',s['windows'][2]['label'])
 def test_invalid(self):
  for key,value in [('usedPercent',float('nan')),('usedPercent',True),('resetsAt',None),('windowDurationMins',0),('windowDurationMins',True)]:
   raw=self.raw();raw['rateLimitsByLimitId']['codex']['primary'][key]=value
   with self.assertRaises(RuntimeError):usage.normalise(raw,NOW)
 def test_wire(self):
  p=usage.packet(usage.normalise(self.raw(),NOW));body,crc=p[1:-1].rsplit(b'|',1)
  self.assertEqual(int(crc,16),binascii.crc_hqx(body,0xffff));self.assertTrue(p.startswith(b'@U|'))
result=unittest.main(exit=False)
if not result.result.wasSuccessful():sys.exit(1)
# Native test gets packets from the actual Python encoder to catch protocol drift.
snapshot=usage.normalise(Quota().raw(),NOW)
payload=usage.packet(snapshot)[1:-1].decode()
multi=copy.deepcopy(snapshot);multi['windows'].append({'label':'5 hour','remaining_tenths':800,'duration_mins':300,'resets_at':NOW+5000})
def encoded(body):
 return body+'|'+f'{binascii.crc_hqx(body.encode(),0xffff):04x}'
invalid=[]
base=f'U|{NOW}|1|Weekly|110|10080|{NOW+100000}'
for bad in [base.replace('|110|','|1001|'),base.replace('|110|','|-1|'),base.replace('|10080|','|0|'),base.replace('|Weekly|','||'),base.replace('|Weekly|','|'+'x'*23+'|'),base.replace('|1|','|9|',1),base+'|EXTRA',base.replace(str(NOW+100000),'0')]:
 invalid.append(encoded(bad))
code=r'''
#include "CodexUsageRenderer.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
int main(){
 const uint32_t now=1793620000;CodexUsage s;
 std::vector<uint16_t> guarded(240*240+2,0x1234);auto out=guarded.data()+1;
 CodexUsageView::render(out,s,now);assert(s.count()==0&&s.stale(now));
 const char* one=PAYLOAD;assert(s.accept(one,0xffffff00u,now));assert(s.count()==1&&s.current().remainingTenths==110&&!s.stale(now));
 s.tick(uint32_t(0xffffff00u+299000u));assert(s.age()==299&&!s.stale(now));
 s.tick(uint32_t(0xffffff00u+300000u));assert(s.age()==300&&s.stale(now));
 auto revision=s.revision();std::string corrupt=one;corrupt[10]^=1;assert(!s.accept(corrupt.c_str(),0,now)&&s.revision()==revision);
 for(const char* bad : {INVALID}){assert(!s.accept(bad,0,now));assert(s.revision()==revision);}
 assert(!s.accept("U|invalid|1111",0,now));assert(!s.accept(one,0,now-1000));
 assert(s.accept(MULTI,100,now));assert(s.count()==2&&s.selected()==0);s.next();assert(s.selected()==1);
 assert(s.accept(MULTI,200,now));assert(s.selected()==1);s.next();assert(s.selected()==0);
 s.tick(1200);assert(s.age()==1);assert(s.stale(now+100001)); // Reset due never manufactures 100%.
 char text[64];CodexUsage::countdown(text,sizeof(text),now+90061,now);assert(std::string(text)=="Resets in 1d 1h");
 CodexUsage::countdown(text,sizeof(text),now,now);assert(std::string(text)=="Awaiting refreshed quota");
 // Render all percentage extremes and fresh/stale/expired/multiple-window states.
 for(auto t : {now,now+5000,now+100001}){CodexUsageView::render(out,s,t);assert(guarded.front()==0x1234&&guarded.back()==0x1234);}
 puts("PASS actual encoder -> firmware CRC/atomic accept, missing data, multi-window selection, freshness/reset semantics, millis rollover and render guards");
}
'''.replace('INVALID',','.join(json.dumps(p) for p in invalid)).replace('PAYLOAD',json.dumps(payload)).replace('MULTI',json.dumps(usage.packet(multi)[1:-1].decode()))
with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'check.cpp';cpp.write_text(code);exe=Path(tmp)/'check'
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT/'XiaoPlayground'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
