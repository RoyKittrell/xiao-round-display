#pragma once
#include <stdint.h>

class SceneRotation {
 uint32_t selectedAt=0;
 bool ready=false;
 uint32_t duration=15*60*1000UL;
 public:
 static constexpr uint32_t Interval=15*60*1000UL;
 void reset(uint32_t now){selectedAt=now;ready=true;}
 void configure(uint32_t milliseconds,uint32_t now){if(duration!=milliseconds){duration=milliseconds;reset(now);}}
 uint32_t interval()const{return duration;}
 bool observe(uint32_t now,bool eligible){
  if(!eligible){ready=false;return false;}
  if(!ready){reset(now);return false;}
  if(uint32_t(now-selectedAt)<duration)return false;
  reset(now);return true;
 }
 uint32_t remaining(uint32_t now) const{
  if(!ready)return duration;
  const uint32_t elapsed=now-selectedAt;
  return elapsed>=duration?0:duration-elapsed;
 }
};

// All wall times use the existing configured civil-time clock.
inline bool scheduledDisplayOff(uint32_t localSeconds,bool usbPowered){
 const uint32_t day=localSeconds%86400;
 return usbPowered&&day>=3600&&day<7*3600;
}

class UsbPowerState {
 bool seen=false,present=false;
 uint32_t lastGood=0;
 public:
 void observe(bool valid,bool pluggedIn,uint32_t now){
  if(valid){seen=true;present=pluggedIn;lastGood=now;}
 }
 bool connected(uint32_t now) const{
  // Brief read failures do not flash the screen. Unknown/stale power state
  // fails awake rather than imposing a USB-only schedule on battery power.
  return seen&&uint32_t(now-lastGood)<5000&&present;
 }
};

class HalfPastFaceRotation {
 bool ready=false;
 uint32_t lastTime=0,latestBoundary=0;
 public:
 void reset(uint32_t localSeconds){ready=true;lastTime=localSeconds;latestBoundary=(localSeconds+1800)/3600;}
 bool observe(uint32_t localSeconds,bool eligible){
  const uint32_t boundary=(localSeconds+1800)/3600;
  if(!ready){ready=true;lastTime=localSeconds;latestBoundary=boundary;return false;}
  const bool continuous=localSeconds>=lastTime&&localSeconds-lastTime<=120;
  lastTime=localSeconds;
  if(boundary<=latestBoundary)return false;
  latestBoundary=boundary;
  // Consume boundaries in other scenes/asleep without a later catch-up.
  // Large clock corrections/reboots never fast-forward the collection, and
  // a backwards correction cannot replay a boundary already consumed.
  return continuous&&eligible;
 }
};
