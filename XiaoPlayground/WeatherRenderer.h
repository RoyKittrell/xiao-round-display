#pragma once
#include "WeatherStatus.h"
#include "watchfaces/face_helpers.h"
#include "assets/fonts/WeatherFonts.h"
#include <ctype.h>

namespace WeatherView {
using namespace WatchFace;
inline void text(uint16_t* out,const ClockFonts::Font& f,const char* s,int cx,int baseline,uint16_t c){Detail::centred(out,f,s,cx,baseline,c);}
inline uint32_t local(uint32_t now,int offset){return uint32_t(int64_t(now)+offset);}
inline Point at(float hour,int horizon,float radius){return radial(hour*2*Pi/horizon,radius);}
inline void hourLabel(char* out,int hour,bool suffix){int h=hour%24;snprintf(out,12,suffix?"%d%s":"%d",h%12?h%12:12,h>=12?"pm":"am");}
inline void range(char* out,size_t size,uint32_t begin,uint32_t end,uint32_t now,int offset){
 int a=local(begin,offset)/3600%24,b=local(end,offset)/3600%24;char aa[12],bb[12];
 if(begin<=now)strcpy(aa,"Now");else hourLabel(aa,a,(a>=12)!=(b>=12));hourLabel(bb,b,true);
 snprintf(out,size,"%s-%s",aa,bb);
}
inline void cloud(Painter& p,float x,float y,uint16_t c,float scale){p.disk(x-7*scale,y+2*scale,5*scale,c);p.disk(x-2*scale,y-3*scale,7*scale,c);p.disk(x+7*scale,y,6*scale,c);p.disk(x,y+2*scale,6*scale,c);p.line({x-7*scale,y+6*scale},{x+7*scale,y+6*scale},3*scale,c);}
inline void icon(uint16_t* out,WeatherStatus::Icon which,bool day){
 Painter p(out);auto sun=rgb(255,209,136),light=rgb(211,229,238),blue=rgb(105,206,230);
 constexpr float scale=2.5f,centreY=108;
 auto point=[&](float x,float y)->Point{return {120+(x-120)*scale,centreY+(y-84)*scale};};
 auto disk=[&](float x,float y,float radius,uint16_t c){Point a=point(x,y);p.disk(a.x,a.y,radius*scale,c);};
 auto line=[&](float x,float y,float xx,float yy,float width,uint16_t c){p.line(point(x,y),point(xx,yy),width*scale,c);};
 if(which==WeatherStatus::Unknown){text(out,WeatherFonts::Hero,"?",120,120,rgb(144,163,176));return;}
 bool clear=which==WeatherStatus::Clear,part=which==WeatherStatus::PartlyCloudy;
 if(clear||part){float x=part?114:120,y=part?79:84;
  if(day){disk(x,y,10,rgb(90,76,50));disk(x,y,8,sun);}
  else {disk(x,y,8,rgb(205,221,237));disk(x+4,y-3,7,rgb(24,43,57));}
 }
 if(!clear){Point a=point(122,85);cloud(p,a.x,a.y,light,scale);}
 if(which==WeatherStatus::Rain||which==WeatherStatus::Thunder||which==WeatherStatus::Snow){
  if(which==WeatherStatus::Thunder){Point bolt[]={point(120,91),point(116,98),point(120,98),point(118,102),point(126,95),point(122,95),point(125,91)};p.polygon(bolt,7,sun);}
  else for(int i=-1;i<=1;i++){if(which==WeatherStatus::Snow)disk(122+i*8,98,1.4f,light);else line(124.f+i*8,95,121.f+i*8,99,1.3f,blue);}
 }
 if(which==WeatherStatus::Fog)for(int y=96;y<=100;y+=2)line(107.f,float(y),135.f,float(y),1,light);
}
inline void render(uint16_t* out,const WeatherStatus& weather,uint32_t now,int horizon){
 const auto& s=weather.current();bool ready=weather.ready(),old=weather.stale(now);
 const uint16_t white=rgb(244,241,231),muted=rgb(168,190,204),cyan=rgb(101,204,226),gold=rgb(237,200,132),track=rgb(20,43,57);
 // Integer background gradient. No extra framebuffers or per-frame allocation.
 for(int y=0;y<240;y++)for(int x=0;x<240;x++){
  int dx=x-120,dy=y-78,t=255-(dx*dx+dy*dy)/78;if(t<0)t=0;
  int d=((x&1)^((y&1)<<1))-1;
  out[y*240+x]=rgb(8+20*t/255+d,15+44*t/255+d,25+50*t/255+d);
 }
 float localHours=(local(now,s.offset)%86400)/3600.f;
 int current=int(localHours),startIndex=weather.index(now);float fraction=localHours-current;
 // A pixel is assigned to its actual clock hour, then to the matching future
 // UTC interval. The elapsed part of the first sector is the last partial
 // interval of the horizon. This preserves a full rolling 12/24 hours.
 for(int y=5;y<235;y++)for(int x=5;x<235;x++){
  float dx=x+.5f-120,dy=y+.5f-120,square=dx*dx+dy*dy;
  if(square<85*85||square>115*115)continue;float radius=sqrtf(square);
  float angle=atan2f(dx,-dy);if(angle<0)angle+=2*Pi;
  float clockHour=angle*horizon/(2*Pi),phase=clockHour-floorf(clockHour);
  float gap=horizon==12?.06f:.08f;
  float edge=fminf(phase-gap,1-gap-phase)*2*Pi*radius/horizon;
  int cover=int(clamp(fminf(fminf(114-radius,radius-86),edge)+.5f,0,1)*255);if(!cover)continue;
  int k=(int(clockHour)-(current%horizon)+horizon)%horizon;
  if(k==0&&phase<fraction)k=horizon;
  int probability=ready?weather.probability(startIndex+k):-1;
  uint16_t c=track;
  if(probability<0){if(((x+y)/4)%2)c=rgb(59,72,82);}
  else {
   float inner=114-28*probability/100.f;
   int fill=int(clamp(radius-inner+.5f,0,1)*255);
   c=blend(track,old?rgb(74,110,125):cyan,fill);
  }
  out[y*240+x]=blend(out[y*240+x],c,cover);
 }
 Painter p(out);char value[64];
 for(int h=0;h<horizon;h++){
  p.line(at(h,horizon,116.5f),at(h,horizon,118),.6f,rgb(98,121,133));
  if(horizon==24&&h%3)continue;
  if(horizon==12)snprintf(value,sizeof(value),"%d",h?h:12);else snprintf(value,sizeof(value),"%02d",h);
  Point pos=at(h,horizon,78);text(out,WeatherFonts::Hour,value,lroundf(pos.x),lroundf(pos.y)+3,h==current%horizon?gold:muted);
 }
 p.line(at(localHours,horizon,92),at(localHours,horizon,118),1.2f,gold);
 Point marker[]={at(localHours,horizon,111),at(localHours-.05f,horizon,117),at(localHours+.05f,horizon,117)};p.polygon(marker,3,gold);
 char city[25];snprintf(city,sizeof(city),"%s",ready?s.city:"LOCAL WEATHER");for(char* c=city;*c;c++)*c=toupper(uint8_t(*c));
 text(out,WeatherFonts::Label,city,120,66,muted);
 icon(out,ready&&!old?weather.icon(now):WeatherStatus::Unknown,weather.isDay(now));
 if(!ready||old||s.temperature10==-9999)strcpy(value,"--");
 else {int t=s.temperature10;snprintf(value,sizeof(value),"%s%d",t<0?"-":"",(abs(t)+5)/10);}
 strcat(value," C");int origin=96-Detail::textWidth(WeatherFonts::Metric,value)/2;
 text(out,WeatherFonts::Metric,value,96,169,white);
 if(ready&&!old&&s.temperature10!=-9999){char number[20];snprintf(number,sizeof(number),"%s%d",s.temperature10<0?"-":"",(abs(s.temperature10)+5)/10);p.ring(origin+Detail::textWidth(WeatherFonts::Metric,number)+2,157,1.5f,.8f,white);}
 int uv=ready&&!old?s.uv10:-1;
 if(uv<0)strcpy(value,"--");else if(uv<10&&uv>0)snprintf(value,sizeof(value),"0.%d",uv);else snprintf(value,sizeof(value),"%d",(uv+5)/10);
 text(out,WeatherFonts::Metric,value,150,169,uv>=60?gold:white);
 text(out,WeatherFonts::Tiny,"TEMP NOW",96,181,muted);
 int categoryUV=uv<0?-1:(uv+5)/10*10;
 const char* category=uv<0?"UV --":categoryUV<30?"UV LOW":categoryUV<60?"UV MOD":categoryUV<80?"UV HIGH":categoryUV<110?"UV V.HIGH":"UV EXTREME";
 text(out,WeatherFonts::Tiny,category,151,181,uv>=60?gold:muted);p.line({125,153},{125,180},.5f,rgb(64,83,96));
 // Keep failure states explicit even though the normal central headline is gone.
 if(!ready)strcpy(value,"WAITING FOR MAC");else if(old)strcpy(value,"FORECAST OLD");else snprintf(value,sizeof(value),"%dH",horizon);
 text(out,WeatherFonts::Tiny,value,120,191,old?gold:muted);
}
}
