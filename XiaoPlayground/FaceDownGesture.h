#pragma once
#include <stdint.h>
#include <math.h>
// Screen-up is negative sensor Z on this assembled display.
// Require a stable face-down hold, then a face-up hold before another change.
class FaceDownGesture {
 bool armed=false,downTiming=false,upTiming=false;
 uint32_t downStart=0,upStart=0;
public:
 void disarm(){armed=false;downTiming=false;upTiming=false;}
 void cancelHold(){downTiming=false;upTiming=false;}
 bool update(float z,float magnitude,uint32_t now){
  if(magnitude<0.75f||magnitude>1.25f){cancelHold();return false;}
  float orientation=z/magnitude;
  if(!armed){
   if(orientation<-.55f){
    if(!upTiming){upTiming=true;upStart=now;}
    if(now-upStart>=300){armed=true;upTiming=false;}
   }else upTiming=false;
   return false;
  }
  if(orientation>.82f){
   if(!downTiming){downTiming=true;downStart=now;}
   if(now-downStart>=700){disarm();return true;}
  }else downTiming=false;
  return false;
 }
};
