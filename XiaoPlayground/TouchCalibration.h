#pragma once
#include <stdint.h>

struct TouchPoint {int x,y;};
struct TouchCoordinates {
 // bit 2 swaps axes; bits 0/1 mirror the resulting display X/Y.
 // Four-dot calibration on the reference panel confirmed direct display coordinates.
 uint8_t orientation=0;
 TouchPoint map(int x,int y) const{
  if(orientation&4){int t=x;x=y;y=t;}
  return {orientation&1?239-x:x,orientation&2?239-y:y};
 }
};
class TouchCalibration {
 bool running=false,held=false,captured=false,releasing=false;
 uint8_t step=0;
 uint32_t heldAt=0,releaseAt=0;
 int sumX=0,sumY=0,samples=0;
 TouchPoint observed[4]={};
 public:
 enum Event {None,Captured,Next,Complete,Failed};
 uint8_t result=0;
 int error=0;
 bool active() const{return running;}
 bool liftFinger() const{return captured;}
 int index() const{return step;}
 static TouchPoint target(int index){
  const TouchPoint points[]={{120,40},{200,120},{120,200},{40,120}};
  return points[index%4];
 }
 TouchPoint rawPoint(int index) const{return observed[index%4];}
 void begin(){running=true;held=captured=releasing=false;step=0;samples=sumX=sumY=0;error=0;}
 void cancel(){running=false;}
 Event update(bool pressed,int x,int y,uint32_t now){
  if(!running)return None;
  if(pressed){
   releasing=false;
   if(captured)return None;
   if(!held){held=true;heldAt=now;samples=sumX=sumY=0;}
   sumX+=x;sumY+=y;samples++;
   if(uint32_t(now-heldAt)>=120&&samples>=4){
    observed[step]={sumX/samples,sumY/samples};captured=true;return Captured;
   }
  }else{
   held=false;
   if(!captured)return None;
   if(!releasing){releasing=true;releaseAt=now;}
   if(uint32_t(now-releaseAt)<70)return None;
   captured=releasing=false;step++;
   if(step<4)return Next;
   int best=0x7fffffff;uint8_t choice=0;
   for(uint8_t candidate=0;candidate<8;candidate++){
    TouchCoordinates mapping;mapping.orientation=candidate;int score=0;
    for(int i=0;i<4;i++){
     auto p=mapping.map(observed[i].x,observed[i].y),t=target(i);
     int dx=p.x-t.x,dy=p.y-t.y;score+=dx*dx+dy*dy;
    }
    if(score<best){best=score;choice=candidate;}
   }
   result=choice;error=best;running=false;
   return best<=4*35*35?Complete:Failed;
  }
  return None;
 }
};
