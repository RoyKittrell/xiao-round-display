#pragma once
#include "Model.h"
namespace StarSystems {
// Rendering and input share this bounded, allocation-free playback state.
class Playback {
 uint32_t last=0,shownAt=0,noticeAt=0;uint64_t elapsed=0,selectedElapsed=0;
 bool active=false,presented=false,notice=false;
 public:
 System system=System::TYC;bool historical=false,demo=false;
 static constexpr uint32_t TourMs=60000;
 void tick(uint32_t now){if(active){uint32_t dt=now-last;elapsed+=dt;selectedElapsed+=dt;}last=now;}
 void enter(uint32_t now){active=true;last=now;selectedElapsed=0;presented=false;notice=false;}
 void leave(uint32_t now){tick(now);active=false;}
 void next(uint32_t now,bool automatic=false){tick(now);system=System((int(system)+1)%Count);historical=false;if(!automatic)demo=false;elapsed=0;enter(now);}
 bool advance(uint32_t now){tick(now);if(demo&&selectedElapsed>=TourMs){next(now,true);return true;}return false;}
 void toggleDemo(uint32_t now){demo=!demo;enter(now);}
 void toggleHistorical(uint32_t now){if(system==System::HR){historical=!historical;notice=true;noticeAt=now;}}
 void didPresent(uint32_t now){if(!presented){presented=true;shownAt=now;}}
 uint32_t age(uint32_t now) const{return presented?uint32_t(now-shownAt):0;}
 static uint8_t fade(uint32_t age,uint32_t hold,uint32_t duration){return age<=hold?255:age>=hold+duration?0:uint8_t((hold+duration-age)*255/duration);}
 uint8_t titleOpacity(uint32_t now) const{return fade(age(now),2800,900);}
 uint8_t labelOpacity(uint32_t now) const{return fade(age(now),6500,1400);}
 uint8_t noticeOpacity(uint32_t now) const{return notice?fade(uint32_t(now-noticeAt),3200,1000):0;}
 double seconds() const{return elapsed*.001;}
};
enum class Tap {None,Next,Historical};
inline Tap tap(System system,int x,int y){
 int dx=x-120,dy=y-120;if(dx*dx+dy*dy<=38*38)return Tap::Next;
 if(system==System::HR&&x>=70&&x<=170&&y>=174&&y<=210)return Tap::Historical;
 return Tap::None;
}
} // namespace StarSystems
