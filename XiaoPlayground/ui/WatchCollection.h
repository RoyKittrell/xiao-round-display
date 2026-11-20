#pragma once
#include "../WatchFace.h"
#include "WatchTitle.h"

class WatchCollection {
 uint32_t selectedAt=0;
 public:
 WatchFace::Style style=WatchFace::Style::BigBen;
 WatchTitle title;
 bool demo=false;
 uint16_t enabledMask=(1u<<WatchFace::Count)-1;
 static constexpr uint32_t DemoInterval=15000;
 void enter(uint32_t now){selectedAt=now;title.begin();}
 void setMask(uint16_t mask,uint32_t now){
  enabledMask=mask&((1u<<WatchFace::Count)-1);
  if(!enabledMask){demo=false;title.dismiss();return;}
  if(!(enabledMask&(1u<<int(style)))){
   for(int i=0;i<WatchFace::Count;i++)if(enabledMask&(1u<<i)){style=WatchFace::Style(i);break;}
  }
  enter(now);
 }
 void next(uint32_t now,bool automatic=false){
  if(!automatic)demo=false;
  for(int i=1;i<=WatchFace::Count;i++){
   int candidate=(int(style)+i)%WatchFace::Count;
   if(enabledMask&(1u<<candidate)){if(candidate!=int(style)){style=WatchFace::Style(candidate);enter(now);}else selectedAt=now;return;}
  }
 }
 void toggleDemo(uint32_t now){demo=enabledMask?!demo:false;enter(now);}
 bool advanceDemo(uint32_t now){
  if(demo&&uint32_t(now-selectedAt)>=DemoInterval&&!title.active()){auto before=style;next(now,true);return style!=before;}
  return false;
 }
};
