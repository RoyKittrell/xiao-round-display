#pragma once
#include "../WatchGraphics.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif
namespace WatchFace { namespace BigBen {
// White glass, Gothic Roman numerals and gilded tracery drawn from the original
// reference. Only the circular dial is reproduced, without the tower surround.
constexpr uint16_t Glass=0xf7be,Ink=0x2105,Iron=0x4a8a,Gold=0xb4aa;
inline Point bezier(Point a,Point b,Point c,Point d,float t){
 float u=1-t;return {u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,
                     u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y};
}
inline void curve(Painter& p,Point a,Point b,Point c,Point d,float width,uint16_t colour){
 Point last=a;for(int i=1;i<=12;i++){Point next=bezier(a,b,c,d,i/12.0f);p.line(last,next,width,colour);last=next;}
}
inline void leaf(Painter& p,Hand& sector,float inside,float outside,float spread){
 Point a=sector.at(0,inside),b=sector.at(0,outside);
 curve(p,a,sector.at(-spread,inside+2),sector.at(-spread,outside-3),b,.65f,Gold);
 curve(p,a,sector.at(spread,inside+2),sector.at(spread,outside-3),b,.65f,Gold);
}
inline void gothicStroke(Hand& h,float x0,float y0,float x1,float y1,float width){
 h.line(x0,y0,x1,y1,width,Ink);
 // A hairline glint gives the cast-iron numerals a little relief.
 h.line(x0-.35f,y0,x1-.35f,y1,.35f,rgb(99,105,107));
}
inline float glyphWidth(char ch){return ch=='I'?4.5f:8.5f;}
inline void serif(Hand& h,float x,float y,float halfWidth,bool top){
 float sign=top?1:-1;
 Point finial[4]={{x-halfWidth,y+sign*1.4f},{x+halfWidth,y+sign*1.4f},{x+1,y-sign*1.1f},{x-1,y-sign*1.1f}};
 h.poly(finial,4,Ink);
 h.line(x-halfWidth,y+sign*1.4f,x+halfWidth,y+sign*1.4f,.45f,Iron);
}
inline void numeral(Hand& h,float x,char ch){
 if(ch=='I'){
  gothicStroke(h,x,-9.2f,x,9.2f,2.2f);serif(h,x,9.2f,2.8f,true);serif(h,x,-9.2f,2.8f,false);
 }else if(ch=='V'){
  gothicStroke(h,x-3.8f,9.3f,x,-10.4f,2.1f);gothicStroke(h,x,-10.4f,x+3.8f,9.3f,1.5f);
  serif(h,x-3.8f,9.3f,2.2f,true);serif(h,x+3.8f,9.3f,2.2f,true);
 }else{
  gothicStroke(h,x-3.7f,-9.3f,x+3.7f,9.3f,1.9f);gothicStroke(h,x-3.7f,9.3f,x+3.7f,-9.3f,1.5f);
  serif(h,x-3.7f,9.3f,2,true);serif(h,x+3.7f,9.3f,2,true);
  serif(h,x-3.7f,-9.3f,2,false);serif(h,x+3.7f,-9.3f,2,false);
 }
}
inline void dial(uint16_t* output){
 memset(output,0,Size*Size*sizeof(uint16_t));Painter p(output);
 p.disk(120,120,119,Ink);p.disk(120,120,117.6f,Iron);p.disk(120,120,116.5f,Glass);
 // The leaded-glass minute track and the separate Roman-numeral chapter ring.
 p.disk(120,120,113.4f,Iron);p.disk(120,120,112.3f,Ink);p.disk(120,120,111.3f,Glass);
 p.disk(120,120,99.4f,Iron);p.disk(120,120,98.3f,Ink);p.disk(120,120,97.2f,Glass);
 for(int i=0;i<60;i++){
  float a=i*Pi/30;
  p.line(radial(a,99),radial(a,113),i%5?1.0f:1.45f,Iron);
  p.line(radial(a,65),radial(a,97),.3f,rgb(188,188,179));
 }
 // Twelve cross-shaped ornaments in the outer track.
 for(int i=0;i<12;i++){
  Hand h(p,i/12.0f);
  h.line(-4.6f,105.9f,4.6f,105.9f,1.65f,Iron);h.line(0,101,0,111,2.0f,Iron);
  const Point tip[4]={{0,113.5f},{-2.2f,110.6f},{0,108.9f},{2.2f,110.6f}};
  const Point left[4]={{-6,105.9f},{-3.9f,108},{-2.7f,105.9f},{-3.9f,103.8f}};
  const Point right[4]={{6,105.9f},{3.9f,108},{2.7f,105.9f},{3.9f,103.8f}};
  const Point bottom[4]={{0,98.8f},{-2.1f,101.3f},{0,103},{2.1f,101.3f}};
  h.poly(tip,4,Iron);h.poly(left,4,Iron);h.poly(right,4,Iron);h.poly(bottom,4,Iron);
 }
 const char* numerals[]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};
 // Hand's local outward coordinate is offset to the numeral's baseline.
 for(int i=0;i<12;i++){
  const char* text=numerals[i];float width=-1.7f;
  for(const char* c=text;*c;c++)width+=glyphWidth(*c)+1.7f;
  float x=-width*.5f;Hand sector(p,i/12.0f);
  // Shift the numeral origin to radius 81 without introducing another canvas.
  for(const char* c=text;*c;c++){
   // Numeral geometry uses +/-10 radially. Translate through a small adapter
   // by drawing into a world-space origin in Hand (same rotation convention).
   Hand letter(p,i/12.0f);letter.origin=sector.at(0,81);
   numeral(letter,x+glyphWidth(*c)*.5f,*c);x+=glyphWidth(*c)+1.7f;
  }
 }
 p.disk(120,120,66.9f,Ink);p.disk(120,120,65.4f,Iron);p.disk(120,120,64.4f,Glass);
 p.disk(120,120,63.2f,Gold);p.disk(120,120,62.6f,Glass);
 for(int i=0;i<12;i++){
  float a=i*Pi/6;Hand sector(p,i/12.0f);
  // Two concentric wreaths of pointed glass petals.
  leaf(p,sector,5,21,6.5f);leaf(p,sector,23,48,10.2f);
  Point inner=radial(a,48),outer=radial(a,62.4f);
  Point left=radial(a-Pi/12,51),right=radial(a+Pi/12,51);
  p.line(inner,left,.65f,Gold);p.line(left,outer,.65f,Gold);
  p.line(outer,right,.65f,Gold);p.line(right,inner,.65f,Gold);
 }
}
inline void hands(uint16_t* output,const uint16_t* background,double seconds){
 memcpy(output,background,Size*Size*sizeof(uint16_t));Painter p(output);
 float t=float(fmod(seconds,43200.0));Hand hour(p,t/43200),minute(p,t/3600);
 // Ornate black hour hand, with a spade head and a broad counterweight.
 const Point shaft[4]={{-3.3f,-20},{-3.3f,49},{3.3f,49},{3.3f,-20}};
 const Point head[4]={{0,65},{-8.2f,52},{0,39},{8.2f,52}};
 const Point leftWing[4]={{-2,45},{-9.5f,48},{-7.1f,56},{0,60}};
 const Point rightWing[4]={{2,45},{9.5f,48},{7.1f,56},{0,60}};
 const Point tail[7]={{-3,-12},{-8,-21},{-7,-29},{0,-33},{7,-29},{8,-21},{3,-12}};
 hour.poly(shaft,4,Ink);hour.poly(head,4,Ink);hour.poly(leftWing,4,Ink);hour.poly(rightWing,4,Ink);hour.poly(tail,7,Ink);
 hour.line(-1.6f,-17,-1.6f,39,.6f,rgb(72,78,81));
 // Long straight minute hand, finished with an open diamond spear point.
 const Point minuteShaft[4]={{-2.6f,-16},{-2.6f,98},{2.6f,98},{2.6f,-16}};
 const Point spear[4]={{0,113},{-6.2f,100},{0,88},{6.2f,100}};
 const Point inset[4]={{0,107.8f},{-2.6f,100},{0,94.4f},{2.6f,100}};
 minute.poly(minuteShaft,4,Ink);minute.poly(spear,4,Ink);minute.poly(inset,4,0,false,background);
 minute.line(-1.3f,-12,-1.3f,91,.55f,rgb(76,83,86));
 p.disk(120,120,5.4f,Ink);p.disk(120,120,3.6f,rgb(43,48,50));p.disk(119.5f,119.5f,1.8f,Ink);
 // The reference has hour/minute hands only: no seconds hand on this face.
}
} } // namespace WatchFace::BigBen
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
