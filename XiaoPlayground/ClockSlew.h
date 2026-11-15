#pragma once
#include <stdint.h>
// Small host-time corrections change the rate by at most 1/32, never reverse
// the sweep. Large explicit date/time changes are handled by the caller.
struct ClockSlew {
 int32_t remaining=0;uint32_t budget=0;
 void reset(){remaining=0;budget=0;}
 void target(int32_t milliseconds){remaining=milliseconds;budget=0;}
 uint32_t advance(uint32_t elapsed){
  if(!remaining){budget=0;return elapsed;}
  budget+=elapsed;int32_t step=budget/32;budget%=32;
  if(remaining<0){if(step>-remaining)step=-remaining;step=-step;}
  else if(step>remaining)step=remaining;
  remaining-=step;return elapsed+step;
 }
};
