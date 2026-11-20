"""Offline weather normalization -> packet -> firmware parser/render checks."""
from pathlib import Path
from unittest.mock import patch
import copy,json,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'companion'))
import weather_status as w
NOW=1794877200 # Hour aligned, deterministic sample.
NOW=NOW//3600*3600
LOCATION={'city':'Example City','latitude':1.0,'longitude':2.0,'utc_offset_seconds':0}

def response(now=NOW):
    times=[now+i*3600 for i in range(-2,38)]
    return {'current':{'time':now,'temperature_2m':30.2},'hourly':{
        'time':times,'precipitation_probability':[10]*len(times),
        'weather_code':[2]*len(times),'is_day':[1]*len(times),'uv_index':[7.1]*len(times)}}

class Weather(unittest.TestCase):
 def test_preceding_hour_mapping(self):
    raw=response();raw['hourly']['precipitation_probability'][3]=85
    s=w.normalize(raw,LOCATION,NOW+1200)
    self.assertEqual(s['probabilities'][0],85)
    self.assertEqual((s['base'],s['temperature10'],s['uv10']),(NOW,302,71))
    self.assertEqual(len(s['probabilities']),30)
 def test_missing_does_not_become_zero(self):
    raw=response();raw['hourly']['precipitation_probability'][3]=None;raw['hourly']['uv_index'][3]=None
    raw['current']['temperature_2m']=None
    s=w.normalize(raw,LOCATION,NOW)
    self.assertEqual((s['probabilities'][0],s['temperature10'],s['uv10']),(255,-9999,-1))
 def test_quarter_hour_timezone(self):
    raw=response();offset=20700
    raw['hourly']['time']=[t-offset%3600 for t in raw['hourly']['time']]
    raw['utc_offset_seconds']=offset
    s=w.normalize(raw,{**LOCATION,'utc_offset_seconds':offset},NOW)
    self.assertEqual((s['base']+offset)%3600,0)
    self.assertTrue(s['base']<=NOW<s['base']+3600)
 def test_invalid_and_stale_inputs(self):
    raw=response();raw['hourly']['time'][4]+=1
    with self.assertRaises(ValueError):w.normalize(raw,LOCATION,NOW)
    raw=response();raw['current']['time']=NOW-7200
    self.assertEqual(w.normalize(raw,LOCATION,NOW)['temperature10'],-9999)
    raw=response();raw['hourly']['precipitation_probability']=[None]*40
    with self.assertRaises(ValueError):w.normalize(raw,LOCATION,NOW)
 def test_bounded_packet(self):
    s=w.normalize(response(),LOCATION,NOW);self.assertLess(len(w.packet(s)),400)
    for k,v in [('city','Bad\ncity'),('probabilities',[101]*30),('days',[2]*30)]:
      bad={**s,k:v}
      with self.assertRaises(ValueError):w.packet(bad)
 def test_http_request_is_bounded(self):
    class Reply:
      def __enter__(self):return self
      def __exit__(self,*args):pass
      def read(self,size):self.size=size;return json.dumps(response()).encode()
    reply=Reply()
    with tempfile.TemporaryDirectory() as temp,patch.object(w,'urlopen',return_value=reply) as call,patch.object(w.time,'time',return_value=NOW):
      (Path(temp)/'weather-location.json').write_text(json.dumps(LOCATION))
      w.fetch(temp);self.assertEqual(call.call_args.kwargs['timeout'],20)
      self.assertEqual(reply.size,2_000_001)

if __name__=='__main__':
 result=unittest.main(exit=False)
 if not result.result.wasSuccessful():sys.exit(1)
 s=w.normalize(response(),LOCATION,NOW)
 s['probabilities']=[10,15,20,30,45,55,70,85,40,25,15,10]+[10]*18
 # The model's starved/fresh icon must use next three hours, not the later rain.
 body=w.packet(s)[1:-1].decode()
 code=r'''
#include "WeatherRenderer.h"
#include "SceneControl.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
int main(int argc,char** argv){
 constexpr uint32_t now=NOW_VALUE;WeatherStatus weather;
 assert(weather.accept(PACKET,now));assert(weather.ready()&&!weather.stale(now));
 assert(weather.current().temperature10==302&&weather.current().uv10==71);
 assert(weather.probability(7)==85&&weather.probability(30)==-1);
 auto window=weather.window(now,12);assert(window.first==6&&window.last==8&&window.threshold==60);
 assert(weather.icon(now)==WeatherStatus::PartlyCloudy);
 assert(weather.stale(now+2700));assert(!weather.accept(PACKET,now-1000));
 std::string bad=PACKET;bad[15]^=1;assert(!weather.accept(bad.c_str(),now));
 assert(weather.index(now+1800)==0&&weather.index(now+3600)==1);
 std::vector<uint16_t> out(57600+2,0xa55a);
 for(int horizon:{12,24}){
  for(uint32_t t:{now,now+1800,now+86399}){
   WeatherView::render(out.data()+1,weather,t,horizon);
   assert(out.front()==0xa55a&&out.back()==0xa55a);
  }
  WeatherView::render(out.data()+1,weather,now,horizon);
  if(argc>1){char path[512];snprintf(path,sizeof(path),"%s/weather-%dh.ppm",argv[1],horizon);FILE* f=fopen(path,"wb");assert(f);fputs("P6\n240 240\n255\n",f);
   for(int i=1;i<=57600;i++){uint16_t c=out[i];unsigned char p[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};fwrite(p,1,3,f);}fclose(f);}
 }
 WeatherStatus empty;WeatherView::render(out.data()+1,empty,now,12);
 // Forecast windows spanning midnight retain both AM/PM suffixes.
 char label[64];WeatherView::range(label,sizeof(label),now+3600,now+10800,now,28800);assert(*label);
 SceneControl control;assert(control.next(10)==11&&control.next(11)==2);
 puts("PASS weather: Python packet, CRC, freshness, 3h icon, hourly windows, 12/24 rendering and carousel wrap");
}
'''.replace('NOW_VALUE',str(NOW)).replace('PACKET',json.dumps(body))
 with tempfile.TemporaryDirectory(prefix='xiao-weather-check-',dir='/tmp') as temp:
  cpp=Path(temp)/'check.cpp';cpp.write_text(code);exe=Path(temp)/'check'
  subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(ROOT/'XiaoPlayground'),str(cpp),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
