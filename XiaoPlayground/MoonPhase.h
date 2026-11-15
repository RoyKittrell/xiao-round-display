#pragma once
#include <math.h>
#include "UserSettings.h"
// SunCalc 1.9 moon illumination approximation (Vladimir Agafonkin, BSD-2-Clause).
// RTC civil time is configured fixed-offset time. Renderer orients the bright limb for the configured observer.
namespace MoonPhase {
struct State{double phase,lit,inc,angle,moonRA,moonDec;};
inline double julian(int y,int m,int d,int hh,int mm,int ss){
 if(m<=2){y--;m+=12;}int a=y/100;
 return floor(365.25*(y+4716))+floor(30.6001*(m+1))+d+2-a+a/4-1524.5+(hh-UserSettings::UtcOffsetMinutes/60.0+mm/60.0+ss/3600.0)/24;
}
inline double ra(double l,double b){double e=23.4397*M_PI/180;return atan2(sin(l)*cos(e)-tan(b)*sin(e),cos(l));}
inline double dec(double l,double b){double e=23.4397*M_PI/180;return asin(sin(b)*cos(e)+cos(b)*sin(e)*sin(l));}
inline State calculate(double jd){
 double d=jd-2451545,rad=M_PI/180;
 double sm=rad*(357.5291+0.98560028*d);
 double sl=sm+rad*(1.9148*sin(sm)+0.02*sin(2*sm)+0.0003*sin(3*sm)+102.9372)+M_PI;
 double ml=rad*(218.316+13.176396*d),ma=rad*(134.963+13.064993*d),mf=rad*(93.272+13.229350*d);
 double l=ml+rad*6.289*sin(ma),b=rad*5.128*sin(mf),distance=385001-20905*cos(ma);
 double sr=ra(sl,0),sd=dec(sl,0),mr=ra(l,b),md=dec(l,b);
 double cosine=sin(sd)*sin(md)+cos(sd)*cos(md)*cos(sr-mr);
 double separation=acos(fmax(-1.0,fmin(1.0,cosine)));
 double inc=atan2(149598000*sin(separation),distance-149598000*cos(separation));
 double angle=atan2(cos(sd)*sin(sr-mr),sin(sd)*cos(md)-cos(sd)*sin(md)*cos(sr-mr));
 return {0.5+0.5*inc*(angle<0?-1:1)/M_PI,(1+cos(inc))/2,inc,angle,mr,md};
}
inline double parallactic(double jd,const State& moon,double latitude,double longitude){
 double rad=M_PI/180;
 double hourAngle=rad*(280.16+360.9856235*(jd-2451545)+longitude)-moon.moonRA;
 return atan2(sin(hourAngle),tan(latitude*rad)*cos(moon.moonDec)-sin(moon.moonDec)*cos(hourAngle));
}
}
