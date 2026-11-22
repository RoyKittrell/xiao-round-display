#pragma once
#include <stdint.h>
#include <math.h>
// Estimated travel represented as a virtual wheel of fixed 2100 mm circumference.
// Set the paired client's wheel size to 2100 mm. No physical wheel is claimed.
class VirtualWheel {
 double metres=0;
 uint32_t revolutions=0;
 uint64_t eventUs=0;
public:
 static constexpr double Circumference=2.1;
 void pulse(uint64_t now,float rpm,double distance){
  metres+=distance;uint32_t next=uint32_t(uint64_t(floor(metres/Circumference)));
  if(next!=revolutions){
   double residual=fmod(metres,Circumference);
   uint64_t lag=rpm>0?uint64_t((60000000.0/rpm)*(residual/distance)):0;
   eventUs=now>=lag?now-lag:now;revolutions=next;
  }
 }
 uint32_t count()const{return revolutions;}
 uint16_t time1024()const{return uint16_t(eventUs*1024ULL/1000000ULL);}
 uint16_t time2048()const{return uint16_t(eventUs*2048ULL/1000000ULL);}
 static void put16(uint8_t* p,uint16_t n){p[0]=uint8_t(n);p[1]=uint8_t(n>>8);}
 static void put32(uint8_t* p,uint32_t n){for(int i=0;i<4;i++)p[i]=uint8_t(n>>(i*8));}
 void csc(uint8_t out[11],uint16_t crank,uint16_t crankTime)const{
  out[0]=3;put32(out+1,revolutions);put16(out+5,time1024());put16(out+7,crank);put16(out+9,crankTime);
 }
 void power(uint8_t out[16],int16_t watts,uint16_t crank,uint16_t crankTime,uint16_t energyKj=0)const{
  put16(out,0x0830);put16(out+2,uint16_t(watts));put32(out+4,revolutions);put16(out+8,time2048());put16(out+10,crank);put16(out+12,crankTime);put16(out+14,energyKj);
 }
};
