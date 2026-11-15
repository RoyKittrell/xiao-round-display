#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace RoyalHolloway {
const uint16_t Glass=rgb(242,238,218),Ink=rgb(43,54,59),Gold=rgb(185,155,94);
const uint16_t Plate=rgb(25,34,36),HandColour=rgb(62,71,68),Facet=rgb(113,120,106);
inline Point cubic(Point a,Point b,Point c,Point d,float t){float u=1-t;return {u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y};}
inline void curvedLead(Painter& p,Hand& sector){
 Point last=sector.at(0,0);
 for(int j=1;j<=24;j++){
  Point next=cubic(sector.at(0,0),sector.at(-14,20),sector.at(12,44),sector.at(0,68),j/24.0f);
  p.line(last,next,1.0f,Gold);last=next;
 }
}
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);p.disk(120,120,119,Plate);
 Detail::ring(p,118,1.5f,Gold);p.disk(120,120,112.5f,Gold);p.disk(120,120,110.7f,Glass);
 // The four corner flowers on the original square surround become small
 // rosettes outside the circular dial, so the numerals keep their full size.
 for(int i=0;i<4;i++){
  Point at=radial((i+.5f)*Pi/2,115);Detail::star(p,at,3.6f,Gold,8);p.disk(at.x,at.y,1.1f,Gold);
 }
 Detail::ring(p,109.2f,.7f,Gold);Detail::ring(p,101.0f,1.0f,Gold);
 for(int i=0;i<60;i++){
  float a=i*Pi/30;p.line(radial(a,101.3f),radial(a,108.7f),i%5?.75f:1.15f,Ink);
 }
 const char* roman[]={"XII","I","II","III","IIII","V","VI","VII","VIII","IX","X","XI"};
 for(int i=0;i<12;i++){
  Hand letter(p,i/12.0f);letter.origin=radial(i*Pi/6,84.6f);Detail::roman(letter,roman[i],22,Ink,.75f);
  float a=(i+.5f)*Pi/6;p.line(radial(a,68.8f),radial(a,100.4f),.55f,Gold);
 }
 Detail::ring(p,68.5f,1.3f,Gold);
 for(int i=0;i<4;i++){Hand quadrant(p,i/4.0f);curvedLead(p,quadrant);}
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);Hand hour(p,Detail::hour(seconds)),minute(p,Detail::minute(seconds));
 hour.line(0,-8,0,64,3.7f,HandColour);
 const Point head[]={{0,70},{-5.8f,56},{0,44},{5.8f,56}};
 const Point eye[]={{0,61},{-1.8f,56},{0,51},{1.8f,56}};
 hour.poly(head,4,HandColour);hour.poly(eye,4,0,false,bg);
 const Point blade[]={{-1.9f,-10},{-2.5f,85},{0,108},{2.5f,85},{1.9f,-10}};
 minute.poly(blade,5,HandColour);minute.line(-.7f,13,-.7f,94,.6f,Facet);
 p.disk(120,120,4.8f,HandColour);p.disk(119.6f,119.5f,1.4f,Facet);
}
} }
