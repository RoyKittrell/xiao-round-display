#pragma once
#include "face_helpers.h"
namespace WatchFace { namespace Aberdeen {
// Aberdeen Town House's main tower: Roman chapter, six-point leaded glazing,
// fine brass borders and pierced cast-metal hands. Static dial uses the cache.
const uint16_t Glass=rgb(243,240,223);
const uint16_t Ink=rgb(40,45,43),Lead=rgb(94,98,85),Gold=rgb(174,153,85);
const uint16_t Rim=rgb(39,44,43),RimLight=rgb(112,115,104),Facet=rgb(119,127,111);
inline void dial(uint16_t* out){
 memset(out,0,Size*Size*2);Painter p(out);
 p.disk(120,120,119,Rim);Detail::ring(p,117.7f,.9f,RimLight);
 p.disk(120,120,114.8f,Glass);
 Detail::ring(p,112.8f,1.2f,Gold);Detail::ring(p,109.9f,.7f,Ink);
 Detail::ring(p,78.0f,1.25f,Gold);
 // Six long diamond panes meet at the pivot. Short radial joints join the
 // inner valleys; unlike the Royal Holloway face these are straight leads.
 for(int i=0;i<6;i++){
  float a=i*Pi/3;
  Point tip=radial(a,77.2f),left=radial(a-Pi/6,30),right=radial(a+Pi/6,30);
  p.line(left,tip,1.0f,Lead);p.line(tip,right,1.0f,Lead);
  p.line({120,120},right,.85f,Lead);
 }
 const char* roman[]={"XII","I","II","III","IIII","V","VI","VII","VIII","IX","X","XI"};
 for(int i=0;i<12;i++){
  Hand letter(p,i/12.0f);letter.origin=radial(i*Pi/6,94.4f);
  Detail::roman(letter,roman[i],25.3f,Ink,.72f);
  for(float radius : {78.0f,112.8f}){
   Point rivet=radial(i*Pi/6,radius);p.disk(rivet.x,rivet.y,1.45f,Gold);
  }
 }
}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds){
 Detail::prepare(out,bg);Painter p(out);
 Hand hour(p,Detail::hour(seconds)),minute(p,Detail::minute(seconds));
 // Build the sculpted hour hand from convex pieces, leaving the glass visible
 // inside its leaf. Restore holes before drawing the overlapping minute hand.
 hour.line(0,-9,0,61,3.8f,Ink);
 const Point leaf[]={{0,76},{-7.3f,55},{-6.5f,32},{0,15},{6.5f,32},{7.3f,55}};
 const Point shoulderL[]={{-5,37},{-10.2f,42},{-7.5f,52},{-4,57}};
 const Point shoulderR[]={{5,37},{10.2f,42},{7.5f,52},{4,57}};
 const Point eye[]={{0,65},{-3.7f,51},{0,27},{3.7f,51}};
 hour.poly(leaf,6,Ink);hour.poly(shoulderL,4,Ink);hour.poly(shoulderR,4,Ink);
 hour.poly(eye,4,0,false,bg);hour.line(-5.4f,34,-5.9f,52,.6f,Facet);
 const Point spear[]={{0,109},{-5.8f,34},{0,-11},{5.8f,34}};
 const Point opening[]={{0,96},{-3.0f,34},{0,12},{3.0f,34}};
 // Four convex strips leave a real aperture: at overlap it reveals the hour
 // hand already beneath it, with no scratch image or extra framebuffer.
 for(int i=0;i<4;i++){
  int j=(i+1)%4;Point border[]={spear[i],spear[j],opening[j],opening[i]};
  minute.poly(border,4,Ink);
 }
 minute.line(-4.6f,36,-.8f,94,.55f,Facet);
 Detail::hub(p,Ink,RimLight,5.0f);
}
} }
