#pragma once
#include <math.h>
#include "Celestial.h"
// Port of Timelapser/solar_diagrams.py's schematic, not sky coordinates.
namespace SeasonalAnalemma {
struct Point { float x,y; };
inline Point point(float phase){
 float seasonalY=-sinf(phase);
 return {sinf(2*phase)*(0.72f+0.28f*seasonalY),seasonalY*(1+0.22f*seasonalY)};
}
inline bool leap(int year){return year%4==0&&(year%100!=0||year%400==0);}
inline int dayOfYear(int year,int month,int day){
 const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
 int result=day;for(int i=1;i<month;i++)result+=lengths[i-1]+(i==2&&leap(year)?1:0);return result;
}
struct Seasons {int march,june,september,december;};
inline Seasons seasons(int year){
 auto events=Astronomy_Seasons(year);
 if(events.status==ASTRO_SUCCESS){
  auto day=[&](astro_time_t t){auto d=clockFromEpoch(Celestial::epoch(t));return dayOfYear(d.year,d.month,d.day);};
  return {day(events.mar_equinox),day(events.jun_solstice),day(events.sep_equinox),day(events.dec_solstice)};
 }
 // Match its offline NOAA-declination approximation for other years.
 float nearestMarch=100,nearestSeptember=100,maxJune=-100,minDecember=100;
 Seasons s={0,0,0,0};int days=leap(year)?366:365;
 for(int month=1;month<=12;month++){
  int count=month==2?(leap(year)?29:28):((month==4||month==6||month==9||month==11)?30:31);
  for(int day=1;day<=count;day++){
   int n=dayOfYear(year,month,day);float g=2*M_PI/days*(n-1);
   float d=0.006918f-0.399912f*cosf(g)+0.070257f*sinf(g)-0.006758f*cosf(2*g)+0.000907f*sinf(2*g)-0.002697f*cosf(3*g)+0.00148f*sinf(3*g);
   if((month==3||month==4)&&fabsf(d)<nearestMarch){nearestMarch=fabsf(d);s.march=n-1;}
   if((month==6||month==7)&&d>maxJune){maxJune=d;s.june=n-1;}
   if((month==9||month==10)&&fabsf(d)<nearestSeptember){nearestSeptember=fabsf(d);s.september=n-1;}
   if(month==12&&d<minDecember){minDecember=d;s.december=n-1;}
  }
 }
 return s;
}
inline float phase(int year,int month,int day,const Seasons& s){
 int n=dayOfYear(year,month,day),start,end;float base;
 if(n<s.march){start=s.december+(leap(year-1)?1:0)-(leap(year)?1:0)-(leap(year-1)?366:365);end=s.march;base=-M_PI/2;}
 else if(n<s.june){start=s.march;end=s.june;base=0;}
 else if(n<s.september){start=s.june;end=s.september;base=M_PI/2;}
 else if(n<s.december){start=s.september;end=s.december;base=M_PI;}
 else {start=s.december;end=s.march+(leap(year)?366:365)+(leap(year+1)?1:0)-(leap(year)?1:0);base=3*M_PI/2;}
 return base+float(n-start)/(end-start)*M_PI/2;
}
}
