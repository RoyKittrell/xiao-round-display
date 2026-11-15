#include <cassert>
#include <cstdio>
#include "../XiaoPlayground/Celestial.h"
int main(){
 for(int m=1;m<=12;m++){
  ClockDate d={2026,m,15,12,0,0};auto t=Celestial::time(d);
  for(int i=0;i<8;i++){auto p=Celestial::planet(i,t);assert(isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z));}
 }
 auto hunter=Celestial::moon({2026,10,26,12,0,0});assert(hunter.fullToday&&!strcmp(hunter.name,"HUNTER'S MOON"));
 auto harvest=Celestial::moon({2026,9,26,12,0,0});assert(harvest.fullToday&&!strcmp(harvest.name,"HARVEST MOON"));
 auto blue=Celestial::moon({2026,5,31,12,0,0});assert(blue.fullToday&&blue.blue);
 auto super=Celestial::moon({2026,12,24,12,0,0});assert(super.fullToday&&super.supermoon);
 auto ordinary=Celestial::moon({2026,10,15,12,0,0});assert(!ordinary.fullToday&&ordinary.phase>0&&ordinary.phase<.25);
 fprintf(stderr,"Traditional full-Moon, Blue Moon, Supermoon and non-event-day tests passed\n");
}
