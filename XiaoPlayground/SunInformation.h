#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SunTypes.h"
#include "ClockDate.h"
#include "UserSettings.h"

// Text shared by the actual display, serial diagnostics and desktop preview.
// This MCU's lean printf omits floating-point output. Format Kp as integers.
namespace SunInformation {
enum Tone { White, Gold, Cyan, Warning };
struct Row { int top; Tone tone; bool bold; char text[48]; };
inline bool validKp(float kp){return kp>=0 && kp<=9;}
inline const char* kpLevel(float kp){
 if(!validKp(kp))return "UNAVAILABLE";
 return kp<3?"QUIET":kp<4?"UNSETTLED":kp<5?"ACTIVE":"STORM";
}
inline bool stale(uint32_t stamp,uint32_t now,uint32_t limit=1800){
 return !stamp || (now>stamp && now-stamp>limit);
}
inline void stamp(char* out,size_t n,uint32_t epoch,bool date=true){
 if(!epoch){snprintf(out,n,"TIME UNAVAILABLE");return;}
 const char* months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
 ClockDate d=clockFromEpoch(epoch);
 if(date)snprintf(out,n,"%02d %s %02d:%02d %s",d.day,months[d.month-1],d.hour,d.minute,UserSettings::TimeZoneLabel);
 else snprintf(out,n,"%02d:%02d %s",d.hour,d.minute,UserSettings::TimeZoneLabel);
}
inline const char* xrayLevel(const char* value){
 if(!*value)return "UNAVAILABLE";
 char* end=nullptr;double multiplier=strtod(value+1,&end);
 if(end==value+1 || *end || !(multiplier>0) || multiplier>10000)return "UNAVAILABLE";
 // Friendly intensity labels, not NOAA's separate R radio-blackout scale.
 switch(*value){case 'A':case 'B':return "LOW";case 'C':return "ELEVATED";case 'M':return "HIGH";case 'X':return "VERY HIGH";default:return "UNAVAILABLE";}
}
inline void kpText(char* out,size_t n,float kp,bool old){
 if(!validKp(kp)){snprintf(out,n,"Kp UNAVAILABLE");return;}
 int hundredths=int(kp*100+0.5f);
 snprintf(out,n,"Kp %d.%02d / 9 | %s",hundredths/100,hundredths%100,old?"OLD":kpLevel(kp));
}
inline void scaleText(char* out,size_t n,char key,int value){
 const char* name=key=='G'?"GEOMAG":key=='R'?"RADIO":"RADIATION";
 const char* levels[]={"NONE","MINOR","MODERATE","STRONG","SEVERE","EXTREME"};
 if(value<0 || value>5){snprintf(out,n,"%s: UNAVAILABLE",name);return;}
 if(!value)snprintf(out,n,"%c0 %s: NO %s",key,name,key=='R'?"BLACKOUT":"STORM");
 else snprintf(out,n,"%c%d %s: %s",key,value,name,levels[value]);
}
struct Panel {
 Row rows[16];int count=0;
 Row& add(int y,Tone tone=White,bool bold=false){Row& r=rows[count++];r.top=y;r.tone=tone;r.bold=bold;r.text[0]=0;return r;}
 Panel(const SunMeta& m,uint32_t now){
  char time[32];Row* r;
  strcpy(add(24,Gold,true).text,"THE SUN");
  strcpy(add(46).text,"He II 304 A / NASA SDAC");
  const char* level=xrayLevel(m.xray);bool valid=strcmp(level,"UNAVAILABLE")!=0;
  bool old=stale(m.xrayEpoch,now);
  r=&add(63,old||!valid?Warning:White);
  if(valid)snprintf(r->text,sizeof(r->text),"X-RAY %s | %s",m.xray,old?"OLD DATA":level);
  else strcpy(r->text,"X-RAY UNAVAILABLE");
  stamp(time,sizeof(time),m.xrayEpoch);snprintf(add(75).text,48,"AS OF %s",time);
  r=&add(90);if(m.flareEpoch)snprintf(r->text,48,"LAST FLARE %s",m.flare);else strcpy(r->text,"LAST FLARE UNAVAILABLE");
  stamp(add(102).text,48,m.flareEpoch);
  kpText(add(120,validKp(m.kp)&&!stale(m.kpEpoch,now)?Cyan:Warning,true).text,48,m.kp,stale(m.kpEpoch,now));
  stamp(time,sizeof(time),m.kpEpoch);snprintf(add(143).text,48,"EST. %s",time);
  scaleText(add(157,m.g>0?Warning:White).text,48,'G',m.g);
  scaleText(add(169,m.r>0?Warning:White).text,48,'R',m.r);
  scaleText(add(181,m.s>0?Warning:White).text,48,'S',m.s);
  stamp(time,sizeof(time),m.scalesEpoch,false);
  snprintf(add(193,stale(m.scalesEpoch,now)?Warning:Cyan).text,48,"SCALES %s%s",time,stale(m.scalesEpoch,now)?" OLD":"");
  stamp(time,sizeof(time),m.imageEpoch);snprintf(add(207,Gold).text,48,"IMG %s",time);
  if(stale(m.imageEpoch,now,4*3600))strcpy(add(220,Warning).text,"IMAGE OVER 4H OLD");
 }
};
}
