#pragma once
#include "UserSettings.h"
#include "CodexUsage.h"
#include "watchfaces/face_helpers.h"
#include "assets/fonts/UsageDigits.h"
namespace CodexUsageView {
using namespace WatchFace;
inline void text(uint16_t* out,const ClockFonts::Font& font,const char* value,int baseline,uint16_t colour){Detail::centred(out,font,value,120,baseline,colour);}
inline void render(uint16_t* out,const CodexUsage& usage,uint32_t unixNow){
 const uint16_t background=rgb(4,10,15),ink=rgb(235,244,241),muted=rgb(125,150,160),track=rgb(23,41,49);
 for(int i=0;i<240*240;i++)out[i]=background;
 bool stale=usage.stale(unixNow);
 int remaining=usage.count()?usage.current().remainingTenths:0;
 uint16_t accent=stale?rgb(222,168,83):remaining<=100?rgb(239,113,105):remaining<=250?rgb(245,189,96):rgb(100,222,195);
 // Seamless antialiased 270-degree arc. Angular membership uses integer
 // cross products; there is no per-pixel trig, heap work or segment seam.
 float endAngle=(-135+270.0f*remaining/1000)*Pi/180;
 int ex=lroundf(sinf(endAngle)*1024),ey=lroundf(-cosf(endAngle)*1024);
 constexpr int outer=107*256+128,inner=102*256+128;
 for(int y=9;y<231;y++)for(int x=9;x<231;x++){
  int dx=x*256+128-30720,dy=y*256+128-30720;
  if(dy>abs(dx))continue; // Open lower quarter.
  int square=dx*dx+dy*dy;
  int cover=128+(outer*outer-square)/(2*outer),inside=128+(square-inner*inner)/(2*inner);
  if(inside<cover)cover=inside;if(cover<=0)continue;if(cover>256)cover=256;
  bool afterStart=-dy-dx>=0,beforeEnd=dx*ey-dy*ex>=0;
  bool filled=remaining>0&&(remaining<=666?(afterStart&&beforeEnd):(afterStart||beforeEnd));
  out[y*240+x]=blend(background,filled?accent:track,cover*255/256);
 }
 text(out,ClockFonts::Title,"CODEX",56,ink);
 if(!usage.count()){
  text(out,ClockFonts::Small,"Usage remaining",79,muted);
  text(out,ClockFonts::Title,"No usage data",122,ink);
  text(out,ClockFonts::Small,"Enable Codex in the helper",147,muted);
  text(out,ClockFonts::Small,UserSettings::Branding,192,muted);return;
 }
 text(out,ClockFonts::Small,usage.current().label,77,muted);
 char value[12];snprintf(value,sizeof(value),"%u%%",(remaining+5)/10);
 text(out,UsageFonts::Digits,value,132,stale?muted:ink);
 text(out,ClockFonts::Small,stale?"REMAINING / LAST READING":"REMAINING",151,stale?accent:muted);
 char line[48];CodexUsage::countdown(line,sizeof(line),usage.current().reset,unixNow);
 text(out,ClockFonts::Small,line,176,ink);
 if(usage.count()>1){snprintf(line,sizeof(line),"%u / %u  -  tap to switch",usage.selected()+1,usage.count());text(out,ClockFonts::Small,line,193,muted);}
 else text(out,ClockFonts::Small,UserSettings::Branding,193,muted);
 auto age=usage.age();
 if(age>=86400)snprintf(line,sizeof(line),"STALE / updated %lud ago",(unsigned long)(age/86400));
 else if(age>=3600)snprintf(line,sizeof(line),"STALE / updated %luh ago",(unsigned long)(age/3600));
 else if(age>=60)snprintf(line,sizeof(line),"%s / updated %lum ago",stale?"STALE":"LIVE",(unsigned long)(age/60));
 else snprintf(line,sizeof(line),"%s / updated %lus ago",stale?"STALE":"LIVE",(unsigned long)age);
 text(out,ClockFonts::Small,line,214,stale?accent:muted);
}
}
