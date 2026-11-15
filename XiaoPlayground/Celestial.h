#pragma once
#include "ClockDate.h"
#include "UserSettings.h"
#include "src/astronomy/astronomy.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
namespace Celestial {
inline astro_time_t time(const ClockDate& d){return Astronomy_TimeFromDays(clockEpoch(d)/86400.0-0.5-UserSettings::UtcOffsetMinutes/1440.0);}
inline uint32_t epoch(astro_time_t t){return uint32_t(llround((t.ut+0.5+UserSettings::UtcOffsetMinutes/1440.0)*86400));}
struct XYZ {double x,y,z;};
inline XYZ planet(int index,astro_time_t t){auto v=Astronomy_HelioVector((astro_body_t)index,t);v=Astronomy_RotateVector(Astronomy_Rotation_EQJ_ECL(),v);return {v.x,v.y,v.z};}
// JPL 1800–2050 fitted orbital elements used only to draw the orbital paths.
// Current planet locations above use Astronomy Engine's VSOP/numerical ephemeris.
static const double elements[8][12]={
 {.38709927,.20563593,7.00497902,252.25032350,77.45779628,48.33076593,.00000037,.00001906,-.00594749,149472.67411175,.16047689,-.12534081},
 {.72333566,.00677672,3.39467605,181.97909950,131.60246718,76.67984255,.00000390,-.00004107,-.00078890,58517.81538729,.00268329,-.27769418},
 {1.00000261,.01671123,-.00001531,100.46457166,102.93768193,0,.00000562,-.00004392,-.01294668,35999.37244981,.32327364,0},
 {1.52371034,.09339410,1.84969142,-4.55343205,-23.94362959,49.55953891,.00001847,.00007882,-.00813131,19140.30268499,.44441088,-.29257343},
 {5.202887,.04838624,1.30439695,34.39644051,14.72847983,100.47390909,-.00011607,-.00013253,-.00183714,3034.74612775,.21252668,.20469106},
 {9.53667594,.05386179,2.48599187,49.95424423,92.59887831,113.66242448,-.00125060,-.00050991,.00193609,1222.49362201,-.41897216,-.28867794},
 {19.18916464,.04725744,.77263783,313.23810451,170.95427630,74.01692503,-.00196176,-.00004397,-.00242939,428.48202785,.40805281,.04240589},
 {30.06992276,.00859048,1.77004347,-55.12002969,44.96476227,131.78422574,.00026291,.00005105,.00035372,218.45945325,-.32241464,-.00508664}};
inline XYZ orbit(int index,double centuries,double eccentricAnomaly){
 const double* v=elements[index];double a=v[0]+v[6]*centuries,e=v[1]+v[7]*centuries;
 double i=(v[2]+v[8]*centuries)*M_PI/180,node=(v[5]+v[11]*centuries)*M_PI/180;
 double w=(v[4]+v[10]*centuries)*M_PI/180-node;
 double x=a*(cos(eccentricAnomaly)-e),y=a*sqrt(1-e*e)*sin(eccentricAnomaly);
 return {(cos(w)*cos(node)-sin(w)*sin(node)*cos(i))*x+(-sin(w)*cos(node)-cos(w)*sin(node)*cos(i))*y,
 (cos(w)*sin(node)+sin(w)*cos(node)*cos(i))*x+(-sin(w)*sin(node)+cos(w)*cos(node)*cos(i))*y,
 sin(w)*sin(i)*x+cos(w)*sin(i)*y};
}
struct MoonDetails {double phase,lit,distance;uint32_t fullEpoch;bool fullToday,supermoon,blue;char name[24];};
inline MoonDetails moon(const ClockDate& date){
 auto t=time(date);auto phase=Astronomy_MoonPhase(t);auto light=Astronomy_Illumination(BODY_MOON,t);
 MoonDetails d={};d.phase=phase.angle/360;d.lit=light.phase_fraction;
 d.distance=Astronomy_VectorLength(Astronomy_GeoMoon(t))*149597870.7;
 ClockDate midnight=date;midnight.hour=midnight.minute=midnight.second=0;
 auto full=Astronomy_SearchMoonPhase(180,time(midnight),40);
 if(full.status!=ASTRO_SUCCESS){strcpy(d.name,"FULL MOON");return d;}
 d.fullEpoch=epoch(full.time);ClockDate event=clockFromEpoch(d.fullEpoch);
 d.fullToday=event.year==date.year&&event.month==date.month&&event.day==date.day;
 d.supermoon=Astronomy_VectorLength(Astronomy_GeoMoon(full.time))*149597870.7<360000;
 const char* names[]={"WOLF MOON","SNOW MOON","WORM MOON","PINK MOON","FLOWER MOON","STRAWBERRY MOON","BUCK MOON","STURGEON MOON","CORN MOON","HUNTER'S MOON","BEAVER MOON","COLD MOON"};
 strcpy(d.name,names[event.month-1]);
 auto autumn=Astronomy_Seasons(event.year).sep_equinox;
 auto first=Astronomy_SearchMoonPhase(180,Astronomy_MakeTime(event.year,9,1,0,0,0),40);
 auto second=Astronomy_SearchMoonPhase(180,Astronomy_AddDays(first.time,1),40);
 auto harvest=fabs(first.time.ut-autumn.ut)<fabs(second.time.ut-autumn.ut)?first.time:second.time;
 auto hunter=Astronomy_SearchMoonPhase(180,Astronomy_AddDays(harvest,1),40).time;
 if(fabs(full.time.ut-harvest.ut)<.02)strcpy(d.name,"HARVEST MOON");
 else if(fabs(full.time.ut-hunter.ut)<.02)strcpy(d.name,"HUNTER'S MOON");
 ClockDate monthStart=event;monthStart.day=1;monthStart.hour=monthStart.minute=monthStart.second=0;
 auto monthFirst=Astronomy_SearchMoonPhase(180,time(monthStart),40);
 d.blue=full.time.ut-monthFirst.time.ut>20;
 return d;
}
inline const char* phaseName(double p){static const char* n[]={"New Moon","Waxing crescent","First quarter","Waxing gibbous","Full Moon","Waning gibbous","Last quarter","Waning crescent"};return n[int(floor(p*8+.5))%8];}
}
