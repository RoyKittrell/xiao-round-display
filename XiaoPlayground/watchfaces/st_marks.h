#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace StMarks {
constexpr uint16_t Blue=0x10ce,Gold=0xd5c9,Stone=0xe6f9,Ink=0x2947;
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);
 p.disk(120,120,119,Gold);p.disk(120,120,116,Stone);
 Detail::ring(p,113,.8f,Gold);Detail::ring(p,90,1.2f,Gold);
 const char* hours[]={"XXIV","I","II","III","IV","V","VI","VII","VIII","IX","X","XI","XII","XIII","XIV","XV","XVI","XVII","XVIII","XIX","XX","XXI","XXII","XXIII"};
 for(int i=0;i<24;i++){
  Hand h(p,i/24.0f);h.origin=radial(i*Pi/12,102);Detail::roman(h,hours[i],9.5f,Ink,.62f);
  h.origin={120,120};h.line(0,111,0,113.5f,.7f,Gold);
 }
 p.disk(120,120,88.5f,Blue);Detail::ring(p,86.8f,1.2f,Gold);
 // The sky, the zodiac and the Earth are separate, readable concentric layers.
 for(int i=0;i<12;i++){
  float a=(i+.5f)*Pi/6;Detail::zodiac(p,i,radial(a,74),4.9f,Gold);
  p.line(radial(i*Pi/6,62),radial(i*Pi/6,86),.65f,rgb(151,133,67));
 }
 Detail::ring(p,61,.8f,Gold);Detail::ring(p,55,.55f,rgb(107,117,113));
 for(int i=0;i<30;i++){
  Point s=radial((i*137.508f)*Pi/180,25+(i*23)%29);Detail::star(p,s,i%3?1.6f:2.3f,rgb(192,167,94),4);
 }
 p.disk(120,120,19.5f,Gold);p.disk(120,120,18.1f,rgb(38,91,119));
 Point land1[]={{110,107},{116,105},{120,110},{116,115},{109,114}};p.polygon(land1,5,rgb(132,157,108));
 Point land2[]={{120,116},{127,114},{131,119},{126,127},{122,130}};p.polygon(land2,5,rgb(132,157,108));
 Detail::ring(p,18.1f,.65f,Gold);
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);
 // A 24-hour sun pointer, read against the I–XXIV chapter ring.
 Hand sun(p,float(fmod(seconds,86400.0)/86400.0));
 sun.line(0,-12,0,99,2.0f,Gold);Detail::sun(p,sun.at(0,48),9.5f,Gold);
 Point tip[]={{0,110},{-3.5f,98},{3.5f,98}};sun.poly(tip,3,rgb(247,225,150));
 // A discreet minute needle is an intentional small-display addition.
 Hand minute(p,Detail::minute(seconds));minute.line(0,0,0,58,1.05f,rgb(235,229,198));minute.disk(0,58,2,rgb(235,229,198));
 Detail::hub(p,Gold,Stone,4);
}
} }
