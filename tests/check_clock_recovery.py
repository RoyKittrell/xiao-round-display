#!/usr/bin/env python3
"""Exercise the actual sketch's clock functions against an intermittent RTC."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root/'XiaoPlayground/XiaoPlayground.ino').read_text()
def function(name):
    start = source.index(name)
    opening = source.index('{', start)
    depth = 0
    for i in range(opening, len(source)):
        depth += (source[i] == '{') - (source[i] == '}')
        if not depth:
            return source[start:i+1]
    raise AssertionError(name)
functions = '\n'.join(function(n) for n in (
    'void clockTick()', 'void serviceClock()', 'ClockDate currentClock()',
    'void printClock()', 'void setClockFromSerial()'))
harness = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include "ClockDate.h"
#include "UserSettings.h"
#include "SceneControl.h"
#include "ClockSlew.h"
#include "WatchAppearance.h"
#include "DisplaySchedule.h"
uint32_t nowMs=0;
uint32_t millis(){return nowMs;}
struct SerialFake {template<class T>void println(T){};} Serial;
struct WireFake {} Wire;
ClockSlew clockSlew;SceneControl sceneControl;
HalfPastFaceRotation halfPastRotation;
uint32_t clockSeconds=0,clockTickMs=0,clockFractionMs=0,clockPollMs=0;
bool clockReady=false,rtcObserved=false,clockWritePending=false,rtcOK=false;
char timeInput[32];int timeInputLength=0;
bool readsWork=true,writesWork=true;
ClockDate stored={2026,10,16,9,48,0};
int writeAttempts=0;
int bcd(int n){return (n/10)*16+n%10;}
int dec(uint8_t n){return (n>>4)*10+(n&15);}
bool writeClock(const ClockDate& d){++writeAttempts;if(!writesWork)return false;stored=d;return true;}
bool readRegs(WireFake&,int,int,uint8_t* r,int){
 if(!readsWork)return false;
 r[0]=bcd(stored.second);r[1]=bcd(stored.minute);r[2]=bcd(stored.hour);
 r[3]=bcd(stored.day);r[4]=0;r[5]=bcd(stored.month);r[6]=bcd(stored.year-2000);return true;
}
'''
tests = r'''
void host(const char* time){strcpy(timeInput,time);timeInputLength=strlen(time);setClockFromSerial();}
void poll(uint32_t delta=1000){nowMs+=delta;serviceClock();}
void reset(){nowMs=clockSeconds=clockTickMs=clockFractionMs=clockPollMs=0;clockReady=rtcObserved=clockWritePending=rtcOK=false;clockSlew.reset();readsWork=writesWork=true;writeAttempts=0;stored={2026,10,16,9,48,0};}
int main(){
 reset();writesWork=readsWork=false;
 host("2026-10-17 23:59:50.250");
 uint32_t expected=clockEpoch({2026,10,17,23,59,50});
 assert(clockWritePending&&!rtcOK);
 for(int i=1;i<=20;i++){poll();assert(clockSeconds==expected+i);assert(scheduledAppearance(clockSeconds)==WatchAppearance::Dark);}
 // Readable stale RTC, but writes still fail: host time must win.
 readsWork=true;
 for(int i=21;i<=30;i++){poll();assert(clockSeconds==expected+i);assert(clockWritePending);}
 // A newer host update replaces the pending one, then drains to RTC on recovery.
 host("2026-10-18 00:02:00.500");expected=clockEpoch({2026,10,18,0,2,0});
 writesWork=true;poll();assert(!clockWritePending&&rtcOK&&rtcObserved);
 assert(clockEpoch(stored)==expected+1);assert(clockSeconds==expected+1);
 poll();assert(clockSeconds==expected+2);assert(clockFractionMs==500);
 // Successful ordinary host sync and valid RTC startup still work.
 reset();host("2026-10-17 12:00:00.000");assert(!clockWritePending&&rtcOK);poll();assert(currentClock().hour==12);
 reset();clockReady=true;poll();assert(currentClock().day==16&&currentClock().hour==9);
 // Scheduled boundaries use authoritative host time while RTC is inaccessible.
 for(const auto* stamp:{"2026-10-17 16:59:59.000","2026-10-18 07:29:59.000"}){
  reset();writesWork=false;host(stamp);auto before=scheduledAppearance(clockSeconds);poll();
  assert(before!=scheduledAppearance(clockSeconds));assert(clockWritePending);
 }
 // Retry cadence stays bounded and works over millis wrap.
 reset();writesWork=false;nowMs=clockTickMs=clockPollMs=0xfffffe00;
 host("2026-10-17 23:00:00.000");int attempts=writeAttempts;
 for(int i=0;i<9;i++){nowMs+=100;serviceClock();}assert(writeAttempts==attempts);
 nowMs+=100;serviceClock();assert(writeAttempts==attempts+1);assert(currentClock().second==1);
 puts("PASS actual firmware clock functions: stale RTC recovery, pending/new host sync, retry cadence, midnight, schedule boundaries and millis wrap.");
}
'''
with tempfile.TemporaryDirectory(prefix='xiao-clock-recovery-') as tmp:
    cpp=Path(tmp)/'check.cpp';exe=Path(tmp)/'check'
    cpp.write_text('#include <initializer_list>\n'+harness+functions+tests)
    subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(root/'XiaoPlayground'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
