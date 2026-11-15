#pragma once
#include <stdint.h>

// Twelve-bit, time-based fading. Squared duty gives finer perceived control
// near darkness, where direct linear PWM made small swipes look abrupt.
class Backlight {
 bool ready=false;
 uint16_t position=0;
 uint32_t lastAt=0;
 public:
 static constexpr int Maximum=4095;
 void off(uint32_t now){ready=true;position=0;lastAt=now;}
 static uint16_t duty(uint16_t linear){
  uint32_t value=(uint32_t(linear)*linear+Maximum/2)/Maximum;
  return linear&&value==0?1:uint16_t(value);
 }
 uint16_t update(int percent,uint32_t now){
  if(percent<0)percent=0;if(percent>100)percent=100;
  int target=(percent*Maximum+50)/100;
  if(!ready){ready=true;position=target;lastAt=now;return duty(position);}
  uint32_t dt=now-lastAt;lastAt=now;if(dt>100)dt=100;
  int difference=target-position;
  if(dt&&difference){
   int step=difference*int(dt)/int(45+dt);
   if(!step)step=difference>0?1:-1;
   position+=step;
  }
  return duty(position);
 }
};
