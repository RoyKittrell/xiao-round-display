#pragma once
#include <stdint.h>
#include <math.h>
// Two separate acceleration bursts, with a calm interval between them.
class DoubleShake {
 bool armed=true,calming=false,pending=false;
 uint32_t calmStart=0,first=0,lastBurst=0;
public:
 void reset(){armed=true;calming=false;pending=false;lastBurst=0;}
 int update(float magnitude,uint32_t now){
  if(pending&&now-first>1800)pending=false;
  if(fabsf(magnitude-1.0f)<0.22f){
   if(!calming){calming=true;calmStart=now;}
   if(now-calmStart>=100)armed=true;
  }else calming=false;
  if(!armed||fabsf(magnitude-1.0f)<0.62f)return 0;
  armed=false;
  if(lastBurst&&now-lastBurst<350)return 0;
  lastBurst=now;
  if(pending&&now-first<=1800){pending=false;return 2;}
  pending=true;first=now;return 1;
 }
};
