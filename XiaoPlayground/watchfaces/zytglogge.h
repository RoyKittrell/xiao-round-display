#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace Zytglogge {
constexpr uint16_t Red=0x58c5,Gold=0xd58b,Sky=0x1a4b,Black=0x0863;
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);p.disk(120,120,119,Gold);p.disk(120,120,116,Red);
 Detail::ring(p,112,.65f,Gold);Detail::romans(p,101,13,Gold);
 Detail::ring(p,89,1.5f,Gold);p.disk(120,120,86.8f,Black);
 Detail::ring(p,83,.85f,Gold);Detail::romans(p,74,10.5f,Gold);
 p.disk(120,120,63,Gold);p.disk(120,120,61.5f,Sky);
 for(int i=0;i<12;i++){
  Detail::zodiac(p,i,radial((i+.5f)*Pi/6,52),4.2f,Gold);
  p.line(radial(i*Pi/6,42),radial(i*Pi/6,61),.6f,Gold);
 }
 Detail::ring(p,41,.7f,Gold);
 // The eccentric rete and intersecting latitude arcs retain the astrolabe's
 // identity without the automata or an unreadably small calendar inscription.
 Detail::ring(p,120,131,29,.8f,Gold);Detail::ring(p,120,131,20,.6f,Gold);
 p.line({80,120},{160,120},.7f,Gold);p.line({120,80},{120,160},.7f,Gold);
 for(int i=0;i<8;i++)Detail::star(p,radial((i+.3f)*Pi/4,32),1.7f,Gold,4);
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);Hand h(p,Detail::hour(seconds)),m(p,Detail::minute(seconds));
 h.line(0,-21,0,108,2.1f,Gold);Detail::sun(p,h.at(0,91),9,Gold);
 Point tip[]={{0,113},{-2.8f,104},{2.8f,104}};h.poly(tip,3,Gold);
 Detail::lance(m,79,3.2f,rgb(240,218,153),20);
 m.disk(0,-25,6.3f,Gold);m.disk(2.0f,-25,5.5f,Black);
 Detail::hub(p,Gold,Red,4.5f);
}
} }
