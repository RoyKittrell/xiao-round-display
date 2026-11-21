#pragma once
#include <stdint.h>

// Grab near 3 o'clock. Keep the initial level under the finger, then map the
// remaining upward/downward travel to the two endpoints. No jump on contact,
// and neither endpoint depends on the level or position at which you started.
class BrightnessGesture {
 bool held=false,captured=false,releasing=false;
 int anchorY=0,anchorLevel=0,filteredY=0;
 uint32_t filterAt=0,releaseAt=0;
 public:
 static constexpr int Top=64,Bottom=192,Deadzone=6;
 struct Result {bool consumed,began,changed,ended;int level;};
 bool active() const{return captured;}
 void reset(){held=captured=releasing=false;}
 Result update(bool pressed,int x,int y,int current,uint32_t now){
  Result result={false,false,false,false,current};
  // Ignore a brief no-contact report during a swipe. A real lift must last
  // 35 ms before a new finger contact can start another gesture.
  if(releasing&&uint32_t(now-releaseAt)>=35){
   result.ended=captured;held=captured=releasing=false;
  }
  if(!pressed){
   result.consumed=captured||result.ended;
   if(held&&!releasing){releasing=true;releaseAt=now;}
   return result;
  }
  releasing=false;
  if(!held){
   // Narrow the initial grab strip to avoid the resistance + button (x<=204).
   held=true;captured=x>=208&&x<240&&y>=80&&y<=174;
   anchorY=y;anchorLevel=current;filteredY=y*256;filterAt=now;
   result.began=captured;
  }
  result.consumed=captured;if(!captured)return result;
  uint32_t dt=now-filterAt;if(!dt)return result;
  filterAt=now;if(dt>50)dt=50;
  int difference=y*256-filteredY;
  int step=difference*int(dt)/int(28+dt);
  if(!step&&difference)step=difference>0?1:-1;
  filteredY+=step;
  int next=anchorLevel*256;
  int upAnchor=(anchorY-Deadzone)*256,downAnchor=(anchorY+Deadzone)*256;
  if(filteredY<upAnchor){
   if(filteredY<=Top*256)next=100*256;
   else next+=(100-anchorLevel)*256*(upAnchor-filteredY)/(upAnchor-Top*256);
  }else if(filteredY>downAnchor){
   if(filteredY>=Bottom*256)next=0;
   else next-=anchorLevel*256*(filteredY-downAnchor)/(Bottom*256-downAnchor);
  }
  // Raw endpoint contact finishes the drag immediately, even with a quick
  // lift. The backlight fade still smooths the output. Re-anchor the filter
  // too, so it cannot pull the level away from that endpoint on the next poll.
  if(y<=Top){next=100*256;anchorY=Top;anchorLevel=100;filteredY=Top*256;}
  if(y>=Bottom){next=0;anchorY=Bottom;anchorLevel=0;filteredY=Bottom*256;}
  if(next<0)next=0;if(next>100*256)next=100*256;
  int delta=next-current*256;
  if(next==0||next==100*256||delta>=192||delta<=-192){
   result.level=(next+128)/256;result.changed=result.level!=current;
  }
  return result;
 }
};
