#pragma once
#include "../../XiaoPlayground/WatchGraphics.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif
namespace WatchFace {
struct Palette {
 uint16_t lume,edge,logo;
};
inline Palette palette(){
 return {rgb(237,227,197),rgb(189,179,150),rgb(147,148,139)};
}
inline void diverDial(uint16_t* output){
 memset(output,0,Size*Size*sizeof(uint16_t));Painter p(output);auto colours=palette();
 // Restrained polished rehaut; the dial itself remains deep black.
 p.disk(120,120,119,rgb(54,61,64));p.disk(120,120,118.2f,rgb(109,117,118));
 p.disk(120,120,117.4f,rgb(19,23,25));p.disk(120,120,115.9f,0);
 for(int i=0;i<60;i++){
  float angle=i*Pi/30;bool major=i%5==0;
  p.line(radial(angle,major?108.1f:110),radial(angle,113.1f),major?1.3f:.75f,major?rgb(178,181,176):rgb(108,115,116));
 }
 for(int i=0;i<12;i++){
  Point c=radial(i*Pi/6,94);
  if(i==0){
   Point rim[3]={{107.6f,23.5f},{132.4f,23.5f},{120,47.3f}};
   Point fill[3]={{110.2f,25.4f},{129.8f,25.4f},{120,44.1f}};
   p.polygon(rim,3,0,true);p.polygon(fill,3,colours.lume);
  }else if(i==6||i==9){
   float hx=i==6?6.2f:10.9f,hy=i==6?10.9f:6.2f;
   Point rim[4]={{c.x-hx,c.y-hy},{c.x+hx,c.y-hy},{c.x+hx,c.y+hy},{c.x-hx,c.y+hy}};
   Point fill[4]={{c.x-hx+1.6f,c.y-hy+1.6f},{c.x+hx-1.6f,c.y-hy+1.6f},
                  {c.x+hx-1.6f,c.y+hy-1.6f},{c.x-hx+1.6f,c.y+hy-1.6f}};
   p.polygon(rim,4,0,true);p.polygon(fill,4,colours.edge);
   fill[0].x+=.5f;fill[0].y+=.5f;fill[1].x-=.5f;fill[1].y+=.5f;
   fill[2].x-=.5f;fill[2].y-=.5f;fill[3].x+=.5f;fill[3].y-=.5f;
   p.polygon(fill,4,colours.lume);
  }else{
   p.disk(c.x,c.y,8.6f,0,true);p.disk(c.x,c.y,7.05f,colours.edge);p.disk(c.x,c.y,6.45f,colours.lume);
  }
 }
 // Our own small compass-star, centred below the 12 o'clock triangle.
 Point vertical[4]={{120,55.5f},{122.1f,62},{120,68.5f},{117.9f,62}};
 Point horizontal[4]={{114.8f,62},{120,60.3f},{125.2f,62},{120,63.7f}};
 p.polygon(vertical,4,colours.logo);p.polygon(horizontal,4,colours.logo);
}
inline void diverHands(uint16_t* output,const uint16_t* background,double seconds){
 memcpy(output,background,Size*Size*sizeof(uint16_t));Painter p(output);auto colours=palette();
 // Reduce before converting to float: epoch-sized floats cannot resolve a sweep.
 float t=float(fmod(seconds,43200.0));Hand hour(p,t/43200),minute(p,t/3600),second(p,float(fmod(seconds,60.0))/60);
 const Point hourOutline[4]={{-3.7f,-11},{-3.7f,39},{3.7f,39},{3.7f,-11}};
 const Point hourLume[4]={{-1.9f,-8},{-1.9f,36},{1.9f,36},{1.9f,-8}};
 const Point tip[3]={{-4.7f,42},{0,65},{4.7f,42}};
 const Point tipLume[3]={{-2.7f,44},{0,60.5f},{2.7f,44}};
 hour.poly(hourOutline,4,0,true);hour.poly(hourLume,4,colours.lume);
 hour.poly(tip,3,0,true);hour.poly(tipLume,3,colours.lume);
 hour.disk(0,37,11,0,true);hour.disk(0,37,9.15f,colours.edge);hour.disk(0,37,8.5f,colours.lume);
 for(int i=0;i<3;i++){float a=i*2*Pi/3;hour.line(0,37,sinf(a)*8.6f,37+cosf(a)*8.6f,1.7f,rgb(101,115,117));}
 hour.disk(0,37,1.25f,rgb(129,144,145));
 const Point sword[5]={{-3.2f,-12},{-4.4f,64},{0,91},{4.4f,64},{3.2f,-12}};
 const Point swordLume[5]={{-1.6f,-8},{-2.6f,63.5f},{0,85},{2.6f,63.5f},{1.6f,-8}};
 minute.poly(sword,5,0,true);minute.poly(swordLume,5,colours.lume);
 // A narrow specular facet runs along the minute hand's left edge.
 minute.line(-3.1f,5,-3.8f,61,.5f,rgb(219,229,229));
 const uint16_t red=rgb(232,48,55);
 second.line(0,-25,0,103,1.05f,red);
 second.disk(0,-19,3.2f,red);second.disk(0,-19,1.8f,0);
 second.disk(0,79,3.8f,red);second.disk(0,79,2.35f,colours.lume);
 p.disk(120,120,4.6f,0,true);p.disk(120,120,3.25f,rgb(19,23,25));p.disk(120,120,1.6f,red);
}
} // namespace WatchFace
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
