#include <cassert>
#include <cstdio>
#include "../XiaoPlayground/ClockDate.h"
int main(){int count=0;for(int y=2000;y<2100;y++)for(int m=1;m<=12;m++)for(int day=1;day<=clockMonthDays(y,m);day++){ClockDate a={y,m,day,23,59,59};assert(clockValid(a));auto b=clockFromEpoch(clockEpoch(a));assert(a.year==b.year&&a.month==b.month&&a.day==b.day&&b.hour==23&&b.minute==59&&b.second==59);count++;}auto leap=clockFromEpoch(clockEpoch({2028,2,28,23,59,59})+1);assert(leap.month==2&&leap.day==29);auto year=clockFromEpoch(clockEpoch({2026,12,31,23,59,59})+1);assert(year.year==2027&&year.month==1&&year.day==1);assert(!clockValid({2026,2,29,0,0,0}));printf("%d dates round-trip; leap day, year rollover and invalid-date checks passed\\n",count);}
