#pragma once
#include <stdint.h>
struct ClockDate {int year,month,day,hour,minute,second;};
inline ClockDate clockCompiled(const char* date,const char* time){
 const char* months="JanFebMarAprMayJunJulAugSepOctNovDec";int month=1;
 for(int m=0;m<12;m++)if(date[0]==months[m*3]&&date[1]==months[m*3+1]&&date[2]==months[m*3+2])month=m+1;
 return {(date[7]-'0')*1000+(date[8]-'0')*100+(date[9]-'0')*10+date[10]-'0',month,
  (date[4]==' '?0:date[4]-'0')*10+date[5]-'0',
  (time[0]-'0')*10+time[1]-'0',(time[3]-'0')*10+time[4]-'0',(time[6]-'0')*10+time[7]-'0'};
}
inline bool clockLeap(int y){return y%4==0&&(y%100!=0||y%400==0);}
inline int clockMonthDays(int y,int m){const int n[]={31,28,31,30,31,30,31,31,30,31,30,31};return m>=1&&m<=12?n[m-1]+(m==2&&clockLeap(y)):0;}
inline bool clockValid(const ClockDate& d){return d.year>=2000&&d.year<=2099&&d.month>=1&&d.month<=12&&d.day>=1&&d.day<=clockMonthDays(d.year,d.month)&&d.hour>=0&&d.hour<24&&d.minute>=0&&d.minute<60&&d.second>=0&&d.second<60;}
inline uint32_t clockEpoch(const ClockDate& d){uint32_t days=0;for(int y=2000;y<d.year;y++)days+=clockLeap(y)?366:365;for(int m=1;m<d.month;m++)days+=clockMonthDays(d.year,m);days+=d.day-1;return days*86400UL+d.hour*3600UL+d.minute*60UL+d.second;}
inline ClockDate clockFromEpoch(uint32_t seconds){ClockDate d={2000,1,1,0,0,0};uint32_t days=seconds/86400;d.hour=seconds%86400/3600;d.minute=seconds%3600/60;d.second=seconds%60;while(days>=uint32_t(clockLeap(d.year)?366:365)){days-=clockLeap(d.year)?366:365;d.year++;}while(days>=uint32_t(clockMonthDays(d.year,d.month))){days-=clockMonthDays(d.year,d.month);d.month++;}d.day=days+1;return d;}
