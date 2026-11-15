#pragma once
#include "../WatchFace.h"
#include "WatchTitle.h"

class WatchCollection {
 uint32_t selectedAt=0;
 public:
 WatchFace::Style style=WatchFace::Style::BigBen;
 WatchTitle title;
 bool demo=false;
 static constexpr uint32_t DemoInterval=15000;
 void enter(uint32_t now){selectedAt=now;title.begin();}
 void next(uint32_t now,bool automatic=false){
  if(!automatic)demo=false;
  style=WatchFace::Style((int(style)+1)%WatchFace::Count);enter(now);
 }
 void toggleDemo(uint32_t now){demo=!demo;enter(now);}
 bool advanceDemo(uint32_t now){
  if(demo&&uint32_t(now-selectedAt)>=DemoInterval&&!title.active()){next(now,true);return true;}
  return false;
 }
};
