#pragma once
#include <stdint.h>
// Bluetooth CSC crank-only measurement. Only accepted physical switch closures
// call pulse(); UI Demo Mode has no path to this counter.
class CyclingCadence {
  uint16_t revolutions=0,eventTime=0;
  uint8_t pulsesPerRevolution=1,phase=0;
  uint64_t lastCrankUs=0,intervalUs=0;
  bool haveCrank=false;
public:
  void calibrate(uint8_t value){if(value>=1&&value<=16){pulsesPerRevolution=value;phase=0;intervalUs=0;haveCrank=false;}}
  uint8_t calibration()const{return pulsesPerRevolution;}
  bool pulse(uint64_t eventUs){
    if(++phase<pulsesPerRevolution)return false;
    if(haveCrank){
      uint64_t gap=eventUs-lastCrankUs;
      uint64_t timeout=intervalUs*3>4000000ULL?intervalUs*3:4000000ULL;
      intervalUs=intervalUs&&gap>timeout?0:gap;
    }
    lastCrankUs=eventUs;haveCrank=true;
    phase=0;++revolutions;eventTime=uint16_t(eventUs*1024ULL/1000000ULL);return true;
  }
  float rpm(uint64_t nowUs)const{
    if(!haveCrank||!intervalUs||nowUs<lastCrankUs)return 0;
    uint64_t timeout=intervalUs*3>4000000ULL?intervalUs*3:4000000ULL;
    if(nowUs-lastCrankUs>timeout)return 0;
    return 60000000.0f/intervalUs;
  }
  uint16_t count()const{return revolutions;}
  uint16_t time()const{return eventTime;}
  void measurement(uint8_t out[5])const{
    out[0]=0x02; // Crank data present, no invented wheel/speed data.
    out[1]=uint8_t(revolutions);out[2]=uint8_t(revolutions>>8);
    out[3]=uint8_t(eventTime);out[4]=uint8_t(eventTime>>8);
  }
};
