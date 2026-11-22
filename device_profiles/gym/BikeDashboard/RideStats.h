#pragma once
#include <stdint.h>
#include <math.h>
struct RideTotals {
 uint32_t magic=0x52494431;
 uint64_t revolutions=0,elapsedUs=0,movingUs=0;
 double metres=0,movingMetres=0,joules=0,rpmSeconds=0;
 float maxRpm=0,maxWatts=0,maxKph=0;
};
// Work/moving averages use completed crank intervals. An idle timeout never
// invents energy or distance. Elapsed time includes pauses while powered on.
class RideStats {
 RideTotals totals;
 uint64_t lastTick=0,lastPulse=0;
 bool haveTick=false,havePulse=false;
public:
 bool dirty=false;
 const RideTotals& data()const{return totals;}
 void tick(uint64_t now){if(!haveTick){lastTick=now;haveTick=true;return;}if(now<lastTick)return;if(totals.revolutions)totals.elapsedUs+=now-lastTick;lastTick=now;}
 void pulse(uint64_t now,float rpm,float watts,double metres){
  if(!totals.revolutions){lastTick=now;haveTick=true;}else tick(now);++totals.revolutions;totals.metres+=metres;
  float kph=rpm*metres*0.06f;
  if(rpm>totals.maxRpm)totals.maxRpm=rpm;
  if(watts>totals.maxWatts)totals.maxWatts=watts;
  if(kph>totals.maxKph)totals.maxKph=kph;
  if(havePulse&&now>=lastPulse&&rpm>0){
   uint64_t interval=now-lastPulse;totals.movingUs+=interval;
   totals.movingMetres+=metres;totals.joules+=watts*(interval/1000000.0);
   totals.rpmSeconds+=rpm*(interval/1000000.0);
  }
  lastPulse=now;havePulse=true;dirty=true;
 }
 void reset(){totals=RideTotals{};haveTick=havePulse=false;dirty=true;}
 bool restore(const RideTotals& saved){
  if(saved.magic!=0x52494431||saved.movingUs>saved.elapsedUs+1000000ULL||!isfinite(saved.metres)||!isfinite(saved.joules)||!isfinite(saved.rpmSeconds)||!isfinite(saved.movingMetres)||!isfinite(saved.maxRpm)||!isfinite(saved.maxWatts)||!isfinite(saved.maxKph)||saved.metres<0||saved.joules<0||saved.rpmSeconds<0||saved.movingMetres<0||saved.maxRpm<0||saved.maxWatts<0||saved.maxKph<0)return false;
  totals=saved;haveTick=havePulse=false;dirty=false;return true;
 }
 double averageWatts()const{return totals.movingUs?totals.joules*1000000.0/totals.movingUs:0;}
 double averageRpm()const{return totals.movingUs?totals.rpmSeconds*1000000.0/totals.movingUs:0;}
 double averageKph()const{return totals.movingUs?totals.movingMetres*3600000.0/totals.movingUs:0;}
 double overallWatts()const{return totals.elapsedUs?totals.joules*1000000.0/totals.elapsedUs:0;}
 double overallKph()const{return totals.elapsedUs?totals.movingMetres*3600000.0/totals.elapsedUs:0;}
};
