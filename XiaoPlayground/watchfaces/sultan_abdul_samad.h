#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace SultanAbdulSamad {
constexpr uint16_t Copper=0xac49,Glass=0xef7b,Ink=0x2945,Lead=0xb5b2;
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);p.disk(120,120,119,rgb(101,61,38));p.disk(120,120,117,Copper);p.disk(120,120,114,rgb(206,167,119));p.disk(120,120,111.7f,Glass);
 Detail::ring(p,108,1.3f,Ink);Detail::ring(p,105,.65f,Lead);Detail::track(p,100.5f,104,Ink,.6f,1.1f);
 Detail::romans(p,84.8f,18,Ink,true,.74f);Detail::ring(p,69,.85f,Ink);
 // The dial's leaded six-point geometry is a signature of the KL tower.
 for(int phase=0;phase<2;phase++)for(int i=0;i<3;i++){
  float a=(i*2*Pi/3)+phase*Pi/3,b=((i+1)*2*Pi/3)+phase*Pi/3;
  p.line(radial(a,66),radial(b,66),.65f,Lead);
 }
 for(int i=0;i<12;i++)p.line(radial(i*Pi/6,69),radial(i*Pi/6,106),.4f,rgb(195,194,180));
 Detail::ring(p,22,.5f,Lead);
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);Hand h(p,Detail::hour(seconds)),m(p,Detail::minute(seconds));
 h.line(0,-18,0,51,3.6f,Ink);h.disk(0,44,6,Ink);
 Point spade[]={{0,67},{-6.5f,53},{0,44},{6.5f,53}};h.poly(spade,4,Ink);
 h.disk(0,-22,5.5f,Ink);h.disk(0,-22,3,Copper);
 Detail::lozenge(m,101,4.6f,Ink,bg);m.line(-.8f,0,-.8f,76,.5f,rgb(100,104,98));
 Detail::hub(p,Ink,Copper,4.8f);
}
} }
