#pragma once
#include "face_helpers.h"
#include "hand_mask.h"
#include "../assets/fonts/WakoNumerals.h"
#include <stdio.h>
namespace WatchFace { namespace Wako {
// Numeral contours and optical placement measured from the original museum photo.
const uint16_t Glass=rgb(228,230,224),Ink=rgb(27,33,35),Metal=rgb(62,72,73);
const uint16_t Bezel=rgb(35,42,44),RimLight=rgb(114,124,122),GlassEdge=rgb(200,208,202);
const uint16_t HandFacet=rgb(46,54,56),Rivet=rgb(14,21,23);
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);
 p.disk(120,120,119.3f,Bezel);p.disk(120,120,118.4f,Glass);
 for(int i=0;i<60;i++){
  Hand index(p,i/60.0f);
  if(i%5==0){const Point diamond[]={{0,105.5f},{-3.3f,109.3f},{0,113.1f},{3.3f,109.3f}};index.poly(diamond,4,Ink);}
  else index.line(0,107.0f,0,111.6f,2.1f,Ink);
 }
 static const Point positions[]={{123,37},{162,46},{194,79},{206,121},{190,164},{158,197},{121,206},{87,196},{50,162},{34,122},{52,79},{86,44}};
 for(int i=0;i<12;i++){
  char number[3];snprintf(number,sizeof(number),"%d",i?i:12);
  int width=Detail::textWidth(ClockFonts::WakoNumerals,number)-2;
  Detail::text(out,ClockFonts::WakoNumerals,number,int(positions[i].x)-width/2,int(positions[i].y)+19,Ink);
 }
 // No wordmark or substitute text: The optional dial is clean and unsigned.
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);
 Detail::handMask(out,WakoAssets::Hour,Detail::hour(seconds),Ink);
 Detail::handMask(out,WakoAssets::Minute,Detail::minute(seconds),Ink);
 Painter p(out);Hand m(p,Detail::minute(seconds));
 m.line(-.55f,14,-.55f,97,.35f,HandFacet);
 for(int i=0;i<6;i++)m.disk(0,24+i*13,.4f,Rivet);
 p.disk(120,120,5.1f,Ink);p.disk(119.7f,119.5f,1.5f,Metal);
}
} }
