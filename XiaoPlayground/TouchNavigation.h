#pragma once
#include <stdint.h>

// Shared touchscreen routing for all scenes. The upper 60 pixels are reserved
// for navigation; other presses remain immediate scene-specific taps.
class TouchNavigation {
 bool down=false,releasing=false,valid=false,top=false,second=false,pending=false;
 int startX=0,startY=0,firstX=0,firstY=0;
 uint32_t started=0,releaseAt=0,firstRelease=0;
 static bool close(int x,int y,int a,int b,int radius){int dx=x-a,dy=y-b;return dx*dx+dy*dy<=radius*radius;}
 public:
 enum Kind {None,Tap,NextScene};
 struct Event {Kind kind;int x,y;};
 // A real short touch can be observed by only one poll, especially while the
 // LCD is being sent in strips. Do not require multiple sampled down frames.
 static constexpr uint32_t ReleaseMs=20,MinPressMs=0,MaxPressMs=450,MinGapMs=25,MaxGapMs=650;
 static bool topZone(int x,int y){return x>=0&&x<240&&y>=0&&y<=60;}
 void sceneChanged(){pending=false;second=false;if(down)valid=false;}
 Event update(bool pressed,int x,int y,uint32_t now){
  Event event={None,0,0};
  // Brief IRQ gaps while a finger is still down must not become extra taps.
  if(down&&releasing&&uint32_t(now-releaseAt)>=ReleaseMs){
   down=false;releasing=false;
   uint32_t duration=releaseAt-started;
   if(valid&&top&&duration>=MinPressMs&&duration<=MaxPressMs){
    if(second){event={NextScene,startX,startY};pending=false;}
    else{pending=true;firstRelease=releaseAt;firstX=startX;firstY=startY;}
   }
   second=false;
  }
  if(pending&&uint32_t(now-firstRelease)>MaxGapMs)pending=false;
  if(pressed){
   if(!down){
    down=true;releasing=false;started=now;startX=x;startY=y;
    valid=x>=0&&x<240&&y>=0&&y<240;top=topZone(x,y);second=false;
    if(top&&pending){
     uint32_t gap=now-firstRelease;
     second=gap>=MinGapMs&&gap<=MaxGapMs&&close(x,y,firstX,firstY,50);
     pending=false;
    }else if(!top)pending=false;
    if(valid&&!top&&event.kind==None)event={Tap,x,y};
   }else{
    releasing=false;
    if(!close(x,y,startX,startY,28)||topZone(x,y)!=top)valid=false;
   }
   if(uint32_t(now-started)>MaxPressMs){valid=false;pending=false;}
  }else if(down&&!releasing){releaseAt=now;releasing=true;}
  return event;
 }
};
