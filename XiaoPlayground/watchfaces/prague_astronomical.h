#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace Prague {
constexpr uint16_t Gold=0xd5cb,Blue=0x238f,Night=0x0884,Rust=0x8b25,Ink=0x18a4;
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);p.disk(120,120,119,rgb(105,74,35));p.disk(120,120,116,Gold);p.disk(120,120,113,Ink);
 Detail::track(p,109,112,Gold,.55f,1.0f);Detail::romans(p,99,13,Gold);
 p.disk(120,120,86.5f,Gold);p.disk(120,120,84.9f,Blue);
 // The blue sky, terracotta twilight and dark night identify the astrolabe.
 // This is a composed dial, not a calculation of Prague's horizon or ephemeris.
 for(int y=130;y<204;y++){
  float dy=y+.5f-120;float half=sqrtf(84.5f*84.5f-dy*dy);
  p.line({120-half,float(y)+.5f},{120+half,float(y)+.5f},1.02f,Rust);
 }
 p.disk(120,164,40.5f,Night);
 // Restore the circular boundary after the lower night disc.
 Detail::ring(p,85.3f,1.4f,Gold);
 for(float r : {31.0f,55.0f,72.0f})Detail::ring(p,r,.65f,rgb(178,151,78));
 for(int i=-3;i<=3;i++){
  float a=i*Pi/10;p.line(radial(a+Pi,83),{120,120},.55f,rgb(166,145,80));
 }
 Detail::ring(p,120,100,59,1.4f,Gold);Detail::ring(p,120,100,44,.85f,Gold);
 for(int i=0;i<12;i++){
  float a=(i+.3f)*Pi/6;Point c={120+sinf(a)*51.5f,100-cosf(a)*51.5f};Detail::zodiac(p,i,c,4.1f,Gold);
  Point a1={120+sinf(i*Pi/6)*44,100-cosf(i*Pi/6)*44};Point a2={120+sinf(i*Pi/6)*59,100-cosf(i*Pi/6)*59};p.line(a1,a2,.55f,Gold);
 }
 p.disk(120,120,8,rgb(43,107,125));Detail::ring(p,8,.75f,Gold);
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);Hand h(p,Detail::hour(seconds)),m(p,Detail::minute(seconds));
 h.line(0,-25,0,94,1.8f,Gold);Detail::sun(p,h.at(0,71),8,Gold);
 Point tip[]={{0,108},{-3.5f,96},{3.5f,96}};h.poly(tip,3,Gold);
 // A contrasting steel-blue minute pointer keeps this intricate face usable.
 Detail::lance(m,82,4.5f,rgb(233,230,205),13);
 m.disk(0,-25,5,rgb(231,221,172));m.disk(1.8f,-25,4,Night);
 Detail::hub(p,Gold,Ink);
}
} }
