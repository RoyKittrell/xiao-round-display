#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace Conciergerie {
constexpr uint16_t Royal=0x114e,Gold=0xe62d,Ivory=0xff9b,Ink=0x3186;
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);p.disk(120,120,119,Gold);p.disk(120,120,116,Royal);
 for(int i=0;i<16;i++)Detail::fleur(p,radial((i+.5f)*Pi/8,108),3.2f,Gold);
 Detail::ring(p,100,1.6f,Gold);p.disk(120,120,97,Ivory);Detail::ring(p,94,.9f,Gold);
 Detail::track(p,88.8f,92,Ink,.5f,.9f);Detail::romans(p,75.5f,16,Ink,true,.72f);
 Detail::ring(p,61,.65f,Gold);Detail::ring(p,58,.6f,Gold);
 for(int i=0;i<24;i++){
  Hand h(p,i/24.0f);Point ray[]={{0,54},{-1.4f,17},{1.4f,17}};h.poly(ray,3,rgb(219,180,85));
 }
 p.disk(120,120,17,Gold);p.disk(120,120,14,rgb(238,205,127));
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);Hand h(p,Detail::hour(seconds)),m(p,Detail::minute(seconds));
 // Pierced gilt lozenges, with a dark edge so gold stays readable on ivory.
 Detail::lozenge(h,59,8,Ink);Detail::lozenge(h,57,6.2f,Gold,bg);
 h.disk(0,-17,4.5f,Gold);h.line(0,-23,0,15,2.6f,Gold);
 Detail::lozenge(m,89,5.7f,Ink);Detail::lozenge(m,87,4.1f,Gold,bg);
 m.line(-.7f,-11,-.7f,59,.65f,rgb(255,239,178));
 Detail::hub(p,Ink,Gold,5);
}
} }
