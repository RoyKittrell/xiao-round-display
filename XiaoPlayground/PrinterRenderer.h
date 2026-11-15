#pragma once
#include "UserSettings.h"
#include "PrinterStatus.h"
#include "watchfaces/face_helpers.h"
#include "assets/fonts/PrinterDigits.h"
#include "ClockDate.h"
namespace PrinterView {
using namespace WatchFace;
inline void text(uint16_t* out,const ClockFonts::Font& f,const char* value,int baseline,uint16_t colour){Detail::centred(out,f,value,120,baseline,colour);}
inline void shorten(char* line,int maxWidth){
 if(Detail::textWidth(ClockFonts::Small,line)<=maxWidth)return;
 size_t n=strlen(line);while(n&&Detail::textWidth(ClockFonts::Small,line)+Detail::textWidth(ClockFonts::Small,"...")>maxWidth)line[--n]=0;
 strcat(line,"...");
}
inline void nameLines(const char* value,char* a,char* b){
 char remaining[100];snprintf(remaining,sizeof(remaining),"%s",*value?value:"Current print");
 a[0]=b[0]=0;int fit=0,lastSpace=-1;
 while(remaining[fit]){char test[100];memcpy(test,remaining,fit+1);test[fit+1]=0;if(Detail::textWidth(ClockFonts::Small,test)>180)break;if(remaining[fit]==' ')lastSpace=fit;else if(remaining[fit]=='-')lastSpace=fit+1;fit++;}
 if(!remaining[fit]){strcpy(a,remaining);return;}
 if(lastSpace>fit/2)fit=lastSpace;
 memcpy(a,remaining,fit);a[fit]=0;const char* rest=remaining+fit;while(*rest==' ')rest++;
 snprintf(b,100,"%s",rest);shorten(b,180);
}
inline void render(uint16_t* out,const PrinterStatus& printer,uint32_t now){
 auto& s=printer.current();bool lost=printer.ready()&&printer.stale();
 const uint16_t bg=rgb(5,12,15),white=rgb(235,245,241),muted=rgb(133,154,160),green=rgb(91,219,164),amber=rgb(239,183,89),red=rgb(239,111,105);
 for(int i=0;i<240*240;i++)out[i]=bg;
 Painter p(out);p.ring(120,120,115,1,rgb(25,43,46));
 text(out,ClockFonts::Title,"BAMBU",44,white);text(out,ClockFonts::Small,"PRINT MONITOR",61,muted);
 if(!printer.ready()||s.state==PrinterStatus::Connecting||s.state==PrinterStatus::Offline||s.state==PrinterStatus::Idle){
  bool idle=printer.ready()&&!lost&&s.state==PrinterStatus::Idle;
  const char* status=!printer.ready()?"NO PRINT DATA":lost?"UPDATES LOST":idle?"INACTIVE":s.state==PrinterStatus::Offline?"OFFLINE":"CONNECTING";
  // Quiet printer outline, no animation or distracting UI when inactive.
  uint16_t c=idle?green:muted;
  p.line({98,85},{142,85},1.6f,c);p.line({98,85},{98,116},1.6f,c);p.line({142,85},{142,116},1.6f,c);
  p.line({98,116},{142,116},1.6f,c);p.line({105,108},{135,108},1.2f,c);p.line({120,87},{120,98},1.6f,c);p.disk(120,100,1.4f,c);
  text(out,ClockFonts::Title,status,151,idle?white:amber);
  text(out,ClockFonts::Small,!printer.ready()?"Enable Bambu in the helper":idle?"Ready for the next print":lost?"Last status is out of date":s.state==PrinterStatus::Offline?"Printer not reachable":"Waiting for printer status",176,muted);
  text(out,ClockFonts::Small,UserSettings::Branding,207,muted);return;
 }
 char a[100],b[100];nameLines(s.name,a,b);text(out,ClockFonts::Small,a,b[0]?81:87,white);if(b[0])text(out,ClockFonts::Small,b,94,white);
 const char* status=lost?"UPDATES LOST":s.state==PrinterStatus::Printing?"PRINTING":s.state==PrinterStatus::Preparing?"PREPARING":s.state==PrinterStatus::Paused?"PAUSED":s.state==PrinterStatus::Finished?"COMPLETE":"NEEDS ATTENTION";
 uint16_t accent=lost?amber:s.state==PrinterStatus::Error?red:s.state==PrinterStatus::Paused||s.state==PrinterStatus::Preparing?amber:green;
 text(out,ClockFonts::Small,status,112,accent);
 char time[24];int seconds=s.remaining;
 if(!lost&&printer.active()&&s.finish)seconds=s.finish>now?int(s.finish-now):0;
 if(s.state==PrinterStatus::Finished&&!lost)text(out,ClockFonts::Title,"Print finished",150,white);
 else if(lost||seconds<0)text(out,PrinterFonts::Digits,"--:--",155,muted);
 else {
  unsigned minutes=(unsigned(seconds)+59)/60;
  if(minutes/60>99){snprintf(time,sizeof(time),"%uh %um",minutes/60,minutes%60);text(out,ClockFonts::Title,time,150,white);}
  else {snprintf(time,sizeof(time),"%u:%02u",minutes/60,minutes%60);text(out,PrinterFonts::Digits,time,155,lost?muted:white);}
 }
 text(out,ClockFonts::Small,s.state==PrinterStatus::Finished&&!lost?"READY TO COLLECT":s.state==PrinterStatus::Paused?"TIME LEFT WHEN RESUMED":lost?"WAITING FOR FRESH DATA":"HOURS / MINUTES LEFT",171,muted);
 // Seamless perimeter progress arc, matching the Codex scene's geometry.
 // The open bottom quarter leaves room for the estimated finish line.
 int progress=s.progress<0?0:s.progress;
 float endAngle=(-135+270.0f*progress/100)*Pi/180;
 int ex=lroundf(sinf(endAngle)*1024),ey=lroundf(-cosf(endAngle)*1024);
 constexpr int outer=112*256,inner=107*256;
 for(int y=6;y<234;y++)for(int x=6;x<234;x++){
  int dx=x*256+128-30720,dy=y*256+128-30720;if(dy>abs(dx))continue;
  int square=dx*dx+dy*dy;
  int cover=128+(outer*outer-square)/(2*outer),inside=128+(square-inner*inner)/(2*inner);
  if(inside<cover)cover=inside;if(cover<=0)continue;if(cover>256)cover=256;
  bool afterStart=-dy-dx>=0,beforeEnd=dx*ey-dy*ex>=0;
  bool filled=progress>0&&(progress<=66?(afterStart&&beforeEnd):(afterStart||beforeEnd));
  out[y*240+x]=blend(bg,filled?accent:rgb(28,48,52),cover*255/256);
 }
 if(s.progress>=0)snprintf(time,sizeof(time),"%d%%",s.progress);else strcpy(time,"--%");
 text(out,ClockFonts::Title,time,199,muted);
 char finish[48];
 if(lost)snprintf(finish,sizeof(finish),"Status %lum old",(unsigned long)(printer.age()/60));
 else if(s.state==PrinterStatus::Paused)strcpy(finish,"Finish time waits for resume");
 else if(s.state==PrinterStatus::Error)strcpy(finish,"Check printer for details");
 else if(s.state==PrinterStatus::Finished)strcpy(finish,UserSettings::Branding);
 else if(s.finish&&s.finish>now){
  auto d=clockFromEpoch(s.finish-UserSettings::UnixAtCivilEpoch);unsigned days=(int64_t(s.finish)+UserSettings::UtcOffsetSeconds)/86400-(int64_t(now)+UserSettings::UtcOffsetSeconds)/86400;
  if(days)snprintf(finish,sizeof(finish),"Est. %02d:%02d %s +%ud",d.hour,d.minute,UserSettings::TimeZoneLabel,days);
  else snprintf(finish,sizeof(finish),"Est. finish %02d:%02d %s",d.hour,d.minute,UserSettings::TimeZoneLabel);
 }else strcpy(finish,"Awaiting time estimate");
 shorten(finish,136);text(out,ClockFonts::Small,finish,219,white);
}
}
