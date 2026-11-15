#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace Spasskaya {
constexpr uint16_t Gold=0xe62c,Dark=0x0841,Bronze=0x72e5;
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);p.disk(120,120,119,Bronze);p.disk(120,120,117.4f,Gold);p.disk(120,120,114.8f,Dark);
 Detail::ring(p,111.5f,1.2f,Gold);Detail::track(p,103,109,Gold,.85f,2.0f);
 Detail::ring(p,101.1f,.8f,Gold);Detail::romans(p,83.7f,20,Gold,true,.68f);
 Detail::ring(p,65.4f,1.4f,Gold);Detail::ring(p,62.5f,.65f,Bronze);
 // Quiet black centre and monumental gilt numerals distinguish the Kremlin dial.
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);Hand h(p,Detail::hour(seconds)),m(p,Detail::minute(seconds));
 Detail::lance(h,64,10.5f,Gold,16);h.line(-1.4f,3,-2,42,.8f,rgb(255,234,156));
 Detail::lance(m,103,7.3f,Gold,20);m.line(-1.2f,6,-1.8f,71,.7f,rgb(255,234,156));
 Detail::hub(p,Bronze,Gold,5.3f);
}
} }
