#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace GrandCentral {
constexpr uint16_t Brass=0xc509,Ink=0x2944;
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);
 p.disk(120,120,119,rgb(92,62,27));p.disk(120,120,117,Brass);p.disk(120,120,114,rgb(243,214,151));
 p.disk(120,120,112,rgb(142,102,42));p.disk(120,120,109,rgb(241,221,179));
 // Very gentle radial warmth suggests illuminated opal glass, rather than
 // adding texture or a photograph that would consume a frame of RAM.
 for(int r=108;r>=1;r--){int v=(108-r)*10/108;p.disk(120,120,r,rgb(238+v,218+v,176+v));}
 Detail::ring(p,104,.65f,Ink);Detail::track(p,97.8f,102.5f,Ink,.65f,1.6f);
 Detail::arabics(out,ClockFonts::Rail,82,Ink);Detail::ring(p,63,.6f,rgb(163,138,92));
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);Hand h(p,Detail::hour(seconds)),m(p,Detail::minute(seconds));
 // Rounded railway spades, a plain long minute hand, and no seconds needle.
 h.line(0,-12,0,46,4.7f,Ink);h.disk(0,46,7.5f,Ink);
 Point point[]={{-6,48},{0,62},{6,48}};h.poly(point,3,Ink);
 const Point minute[]={{-2.0f,-14},{-2.8f,76},{0,96},{2.8f,76},{2.0f,-14}};m.poly(minute,5,Ink);
 m.disk(0,-17,3.7f,Ink);Detail::hub(p,Ink,Brass,4.9f);
}
} }
