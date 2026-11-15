#include <assert.h>
#include <cmath>
#include <iostream>
#include "../XiaoPlayground/SunInformation.h"
using namespace SunInformation;
int main(int argc,char** argv){
 if(argc==14){
  SunMeta m={uint32_t(strtoul(argv[1],nullptr,10)),uint32_t(strtoul(argv[2],nullptr,10)),uint32_t(strtoul(argv[3],nullptr,10)),uint32_t(strtoul(argv[4],nullptr,10)),uint32_t(strtoul(argv[5],nullptr,10)),uint32_t(strtoul(argv[6],nullptr,10)),float(atof(argv[7])),atoi(argv[8]),atoi(argv[9]),atoi(argv[10]),{}, {}};
  snprintf(m.xray,sizeof(m.xray),"%s",argv[11]);snprintf(m.flare,sizeof(m.flare),"%s",argv[12]);
  Panel panel(m,strtoul(argv[13],nullptr,10));
  for(int i=0;i<panel.count;i++){const auto& r=panel.rows[i];std::cout<<r.top<<'|'<<r.tone<<'|'<<r.bold<<'|'<<r.text<<'\n';}
  return 0;
 }
 char line[64];
 kpText(line,sizeof(line),1.67f,false);assert(!strcmp(line,"Kp 1.67 / 9 | QUIET"));
 kpText(line,sizeof(line),0,false);assert(!strcmp(line,"Kp 0.00 / 9 | QUIET"));
 kpText(line,sizeof(line),9,false);assert(!strcmp(line,"Kp 9.00 / 9 | STORM"));
 kpText(line,sizeof(line),4.999f,false);assert(strstr(line,"5.00"));
 for(float v:{-1.f,9.01f,NAN,INFINITY}){kpText(line,sizeof(line),v,false);assert(!strcmp(line,"Kp UNAVAILABLE"));}
 assert(!strcmp(kpLevel(3),"UNSETTLED")&&!strcmp(kpLevel(4),"ACTIVE")&&!strcmp(kpLevel(5),"STORM"));
 kpText(line,sizeof(line),1.67f,true);assert(strstr(line,"OLD"));
 for(const char* v:{"","--","B","Bnan","Binf","Z2.0","B-1","B8.3bad"})assert(!strcmp(xrayLevel(v),"UNAVAILABLE"));
 assert(!strcmp(xrayLevel("B8.3"),"LOW"));assert(!strcmp(xrayLevel("M1.0"),"HIGH"));
 assert(!strcmp(xrayLevel("X12.5"),"VERY HIGH"));
 scaleText(line,sizeof(line),'S',0);assert(!strcmp(line,"S0 RADIATION: NO STORM"));
 scaleText(line,sizeof(line),'R',0);assert(strstr(line,"NO BLACKOUT"));
 scaleText(line,sizeof(line),'G',5);assert(strstr(line,"EXTREME"));
 scaleText(line,sizeof(line),'G',-1);assert(strstr(line,"UNAVAILABLE"));
 assert(stale(0,100));assert(!stale(100,1900));assert(stale(100,1901));assert(!stale(110,100));
 SunMeta m={845418245,845419825,845419620,845419680,845417940,845419620,1.67f,0,0,0,"B8.3","C1.0"};
 Panel panel(m,845419825);assert(panel.count==13);assert(!strcmp(panel.rows[6].text,"Kp 1.67 / 9 | QUIET"));
 assert(strstr(panel.rows[3].text,"15 OCT 22:47 UTC"));
 Panel old(m,845419825+86400);assert(old.count==14);assert(strstr(old.rows[2].text,"OLD DATA"));assert(strstr(old.rows[6].text,"OLD"));
 assert(strstr(old.rows[11].text,"OLD"));assert(!strcmp(old.rows[13].text,"IMAGE OVER 4H OLD"));
 m.kp=NAN;m.g=-1;m.r=-1;m.s=-1;m.xray[0]=0;m.xrayEpoch=0;m.kpEpoch=0;
 Panel missing(m,845419825);assert(!strcmp(missing.rows[2].text,"X-RAY UNAVAILABLE"));assert(!strcmp(missing.rows[6].text,"Kp UNAVAILABLE"));
 std::cout<<"Sun panel: decimal formatting, levels, missing/invalid data, timestamps and staleness verified\n";
}
