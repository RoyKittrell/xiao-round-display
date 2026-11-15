#!/usr/bin/env python3
"""Test production scheduling and sleep/wake code with simulated time/power/LCD."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'XiaoPlayground/XiaoPlayground.ino').read_text()
def function(name):
 start=source.index(name);opening=source.index('{',start);depth=0
 for i in range(opening,len(source)):
  depth+=(source[i]=='{')-(source[i]=='}')
  if not depth:return source[start:i+1]
 raise AssertionError(name)
functions='\n'.join(function(x) for x in ('void selectScene(', 'void switchMode()', 'void serviceLighting()','void serviceDisplayRecovery()','void serviceDisplaySchedule()','void applyMenuControl('))
preamble=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
#include "DisplaySchedule.h"
#include "SceneControl.h"
#include "GameOfLife.h"
#include "Backlight.h"
#include "WatchAppearance.h"
#include "ClockDate.h"
#include "ui/WatchCollection.h"
uint32_t nowMs=0;uint32_t millis(){return nowMs;}uint32_t micros(){return nowMs*1000u;}void delay(uint32_t ms){nowMs+=ms;}
constexpr uint8_t GC9A01A_DISPOFF=0x28,GC9A01A_DISPON=0x29,GC9A01A_SLPIN=0x10,PIN_D6=6;
int hardwarePWM=-1;void analogWrite(int,int pwm){hardwarePWM=pwm;}
struct FakeSerial {template<class T>void print(T){}template<class T>void println(T){};} Serial;
struct FakeLCD {std::vector<int> commands;int restores=0;void sendCommand(int c){commands.push_back(c);}void restoreConfiguration(){restores++;}void setRotation(int){};} lcd;
struct TouchContact{};TouchContact touchContact;
struct Navigation {void sceneChanged(){}} touchNavigation;
struct Gesture {void reset(){}bool active(){return false;}} brightnessGesture;
bool calibrating=false;struct Calibration {bool active(){return calibrating;}} touchCalibration;
struct Playback {int entered=0,left=0;void enter(uint32_t){entered++;}void leave(uint32_t){left++;}} starPlayback;
struct Renderer {int resets=0;void resetTrails(){resets++;}} starRenderer;
UsbPowerState usbPower;HalfPastFaceRotation halfPastRotation;SceneRotation sceneRotation;SceneControl sceneControl;GameOfLife life;
WatchCollection watches;DisplayLighting lighting;Backlight backlight;
bool firstFrame=false,displaySleeping=false,scheduledWakeRepaint=false,waitingForWake=false;
uint32_t clockSeconds=0,lastRecovery=0,recoveryStarted=0,brightnessReportAt=0;
const char* names[]={"ROUND CLOCK","ANALEMMA","MOON","SOLAR SYSTEM","SUN","STAR SYSTEMS","GAME OF LIFE","CODEX USAGE","BAMBU PRINTER"};
constexpr int firstScene=2,sceneCount=9;
uint32_t switched=0,brightnessHintUntil=0,fadeTick=0;int previewMonth=0,previewDay=0;bool infoTarget=false;uint8_t infoOpacity=0;
int mode=2,mediaOwner=2,recoveryRow=240,appliedBrightness=-1,appliedPWM=-1,selectionCount=0;
void printLighting(){}void watchSelectionChanged(){selectionCount++;mediaOwner=-1;firstFrame=true;}
'''
tests=r'''
void reset(uint32_t seconds=0){
 nowMs=0;clockSeconds=seconds;usbPower=UsbPowerState();halfPastRotation=HalfPastFaceRotation();sceneRotation=SceneRotation();sceneControl=SceneControl();life=GameOfLife();calibrating=false;
 watches=WatchCollection();lighting=DisplayLighting();lighting.dayBrightness=80;lighting.nightBrightness=25;backlight=Backlight();
 lcd=FakeLCD();starPlayback=Playback();starRenderer=Renderer();firstFrame=false;displaySleeping=scheduledWakeRepaint=waitingForWake=false;
 mode=2;mediaOwner=2;recoveryRow=240;lastRecovery=recoveryStarted=brightnessReportAt=0;
 appliedBrightness=appliedPWM=hardwarePWM=-1;selectionCount=0;
}
void power(bool present){usbPower.observe(true,present,nowMs);}
int main(){
 assert(DisplayLighting().dayBrightness==0&&DisplayLighting().nightBrightness==0);
 assert(!scheduledDisplayOff(3599,true)&&scheduledDisplayOff(3600,true));
 assert(scheduledDisplayOff(7*3600-1,true)&&!scheduledDisplayOff(7*3600,true));
 for(uint32_t t=0;t<7*86400;t++){
  assert(scheduledDisplayOff(t,true)==(t%86400>=3600&&t%86400<25200));
  assert(!scheduledDisplayOff(t,false));
 }
 // VBUS read failures: briefly retain known power, then fail awake. Unplug is immediate.
 UsbPowerState p;assert(!p.connected(0));p.observe(true,true,100);
 assert(p.connected(5099));p.observe(false,false,200);assert(p.connected(5099)&&!p.connected(5100));
 p.observe(true,false,5200);assert(!p.connected(5200));p.observe(true,true,0xfffffff0u);assert(p.connected(1000));assert(!p.connected(6000));
 // Exact :30 crossing, repeated polls, midnight, and no replay on small backwards correction.
 HalfPastFaceRotation r;assert(!r.observe(1799,true));assert(r.observe(1800,true));
 assert(!r.observe(1800,true)&&!r.observe(1810,true));assert(!r.observe(1795,true));assert(!r.observe(1800,true));
 r.reset(86399);assert(!r.observe(86400,true));r.reset(86400+1799);assert(r.observe(86400+1800,true));
 r.reset(1799);assert(!r.observe(1800,false));assert(!r.observe(1801,true));
 r.reset(0);assert(!r.observe(24*3600,true));assert(!r.observe(24*3600+1,true));
 // Normal continuous playback advances exactly once per hour over two days.
 r.reset(0);int count=0;for(uint32_t t=1;t<=2*86400;t++)count+=r.observe(t,true);assert(count==48);
 // The actual firmware's LCD transition and lighting functions.
 reset(3599);power(true);serviceDisplaySchedule();serviceLighting();assert(!displaySleeping&&hardwarePWM>0);
 clockSeconds=3600;serviceDisplaySchedule();assert(displaySleeping&&hardwarePWM==0);
 assert(lcd.commands.size()==2&&lcd.commands[0]==GC9A01A_DISPOFF&&lcd.commands[1]==GC9A01A_SLPIN);
 for(int i=0;i<10;i++){nowMs+=1000;power(true);serviceDisplaySchedule();serviceDisplayRecovery();serviceLighting();assert(hardwarePWM==0);}
 assert(lcd.restores==0&&lcd.commands.size()==2); // Periodic recovery cannot wake a sleeping LCD.
 clockSeconds=5400;serviceDisplaySchedule();assert(selectionCount==0); // No invisible overnight rotation.
 clockSeconds=25200;power(true);serviceDisplaySchedule();assert(!displaySleeping&&lcd.restores==1&&scheduledWakeRepaint);
 serviceLighting();assert(hardwarePWM==0); // Backlight stays blank until the first frame is ready.
 firstFrame=false;scheduledWakeRepaint=false;
 for(int i=0;i<100;i++){nowMs+=10;serviceLighting();}assert(hardwarePWM>0);
 assert(lighting.dayBrightness==80&&lighting.nightBrightness==25);
 // Battery/USB hot-plug at night. Keep scene and pause/resume stars, no repeated commands.
 reset(3*3600);mode=7;power(false);serviceDisplaySchedule();assert(!displaySleeping);
 power(true);serviceDisplaySchedule();assert(displaySleeping&&starPlayback.left==1);
 serviceDisplaySchedule();assert(lcd.commands.size()==2);
 power(false);serviceDisplaySchedule();assert(!displaySleeping&&starPlayback.entered==1);
 // Actual collection: :30 wraps all ten faces; other scenes/tour suppress scheduled advances.
 reset(1799);power(false);serviceDisplaySchedule();
 clockSeconds=1800;serviceDisplaySchedule();assert(selectionCount==1&&watches.style==WatchFace::Style::StMarks);
 for(int i=0;i<20;i++)serviceDisplaySchedule();assert(selectionCount==1);
 reset(1799);mode=3;serviceDisplaySchedule();clockSeconds=1800;serviceDisplaySchedule();mode=2;serviceDisplaySchedule();assert(selectionCount==0);
 reset(1799);watches.demo=true;serviceDisplaySchedule();clockSeconds=1800;serviceDisplaySchedule();assert(selectionCount==0&&watches.demo);
 reset(0);for(uint32_t t=0;t<=10*3600;t++){clockSeconds=t;serviceDisplaySchedule();}assert(selectionCount==10&&watches.style==WatchFace::Style::BigBen);

 // Monotonic full-quarter-hour scene dwell, independent of RTC corrections.
 SceneRotation timer;timer.reset(100);
 assert(!timer.observe(900099,true));assert(timer.observe(900100,true));
 assert(!timer.observe(900100,true)&&timer.remaining(900100)==900000);
 timer.reset(0xfffffff0u);assert(!timer.observe(uint32_t(0xfffffff0u+899999u),true));
 assert(timer.observe(uint32_t(0xfffffff0u+900000u),true));
 timer.observe(1000,false);assert(!timer.observe(90000000,true));assert(timer.remaining(90000000)==900000);
 // Exact production scene selection/scheduler: all nine scenes, wrap and no rapid catch-up.
 reset(12*3600);serviceDisplaySchedule();
 for(int next : {3,4,5,6,7,8,9,10,2}){
  nowMs+=899999;serviceDisplaySchedule();int prior=mode;
  assert(sceneRotation.remaining(nowMs)==1);
  nowMs++;serviceDisplaySchedule();assert(mode==next&&mode!=prior);if(mode==8)assert(life.running());else assert(!life.running());
  for(int j=0;j<10;j++)serviceDisplaySchedule();assert(mode==next);
 }
 assert(starPlayback.entered==1&&starPlayback.left==1);
 // Manual selection buys a fresh 15 minutes, without changing watch style.
 reset(12*3600);serviceDisplaySchedule();nowMs=899000;selectScene(6);
 nowMs=900000;serviceDisplaySchedule();assert(mode==6);
 nowMs=1799000;serviceDisplaySchedule();assert(mode==7);
 reset(12*3600);serviceDisplaySchedule();clockSeconds+=3*86400;nowMs=100;serviceDisplaySchedule();assert(mode==2);
 // A stalled loop advances once; it does not replay every missed quarter-hour.
 nowMs=10*900000;serviceDisplaySchedule();assert(mode==3);serviceDisplaySchedule();assert(mode==3);
 // Sleeping and calibration inhibit scene changes and resume with a full interval.
 reset(3600);power(true);serviceDisplaySchedule();assert(displaySleeping);
 nowMs=8*3600*1000;power(true);serviceDisplaySchedule();assert(mode==2);
 clockSeconds=25200;power(true);serviceDisplaySchedule();assert(!displaySleeping&&mode==2);
 assert(sceneRotation.remaining(nowMs)==900000);
 nowMs+=899999;power(true);serviceDisplaySchedule();assert(mode==2);
 nowMs++;power(true);serviceDisplaySchedule();assert(mode==3);
 reset(12*3600);calibrating=true;serviceDisplaySchedule();nowMs=2000000;serviceDisplaySchedule();assert(mode==2);
 calibrating=false;serviceDisplaySchedule();assert(sceneRotation.remaining(nowMs)==900000&&mode==2);
 reset(12*3600);selectScene(8);serviceDisplaySchedule();assert(life.running());
 clockSeconds=3600;power(true);serviceDisplaySchedule();assert(displaySleeping&&!life.running());
 clockSeconds=25200;power(true);serviceDisplaySchedule();assert(!displaySleeping&&life.running()&&mode==8);
 // Actual menu-control handler: cached brightness never brightens a boot.
 reset(12*3600);lighting=DisplayLighting();sceneControl.setSession(123);serviceLighting();
 SceneControl::Settings setting;setting.revision=1;setting.day=80;setting.night=25;
 applyMenuControl(setting);assert(lighting.dayBrightness==0&&lighting.nightBrightness==0&&hardwarePWM==0);
 setting.revision=2;setting.brightness=true;setting.targetBoot=999;applyMenuControl(setting);assert(lighting.dayBrightness==0);
 setting.revision=3;setting.targetBoot=123;setting.day=10;setting.night=2;applyMenuControl(setting);assert(lighting.dayBrightness==10&&lighting.nightBrightness==2);
 nowMs+=100;auto before=sceneRotation.remaining(nowMs);
 setting.revision=4;setting.day=12;applyMenuControl(setting);assert(sceneRotation.remaining(nowMs)==before); // Dimming does not restart carousel.
 setting.revision=5;setting.carousel=false;applyMenuControl(setting);nowMs+=9000000;serviceDisplaySchedule();assert(mode==2);
 setting.revision=6;setting.carousel=true;setting.mask=(1u<<0)|(1u<<8);setting.seconds=60;applyMenuControl(setting);
 nowMs+=60000;serviceDisplaySchedule();assert(mode==10);nowMs+=60000;serviceDisplaySchedule();assert(mode==2);
 setting.revision=7;setting.scene=4;setting.selection=1;applyMenuControl(setting);assert(mode==4); // Excluded scenes remain manually selectable.
 selectScene(3);applyMenuControl(setting);assert(mode==3); // Replayed request does not force old scene.
 setting.revision=8;setting.scene=-1;setting.power=2;applyMenuControl(setting);assert(displaySleeping&&hardwarePWM==0);
 setting.revision=9;setting.power=1;applyMenuControl(setting);assert(!displaySleeping);
 clockSeconds=3*3600;power(true);serviceDisplaySchedule();assert(!displaySleeping); // Explicit always-on override.
 setting.revision=10;setting.power=0;applyMenuControl(setting);assert(displaySleeping);
 // Empty selection pauses carousel on the current scene; re-inclusion resumes
 // with a full dwell instead of catching up elapsed intervals.
 reset(12*3600);setting=SceneControl::Settings();setting.revision=1;setting.mask=0;
 applyMenuControl(setting);selectScene(4);serviceDisplaySchedule();nowMs=9000000;serviceDisplaySchedule();assert(mode==4);
 switchMode();assert(mode==4);
 setting.revision=2;setting.mask=(1u<<7)|(1u<<8);setting.seconds=60;applyMenuControl(setting);
 serviceDisplaySchedule();assert(mode==4);nowMs+=59999;serviceDisplaySchedule();assert(mode==4);
 nowMs++;serviceDisplaySchedule();assert(mode==9);nowMs+=60000;serviceDisplaySchedule();assert(mode==10);
 puts("PASS actual menu handler: zero-brightness boot/replay, explicit boot-bound brightness, filtered carousel, hold, manual selection, idempotence and LCD off/on.");
 puts("PASS 15-minute scene dwell, all-nine wrap, manual reset, sleep/calibration inhibition, no catch-up, RTC independence and millis rollover.");
 puts("PASS actual display sleep/wake/lighting functions, USB-only boundaries, VBUS outages/hotplug, half-past rotation/wrap and recovery isolation.");
}
'''
with tempfile.TemporaryDirectory(prefix='xiao-display-schedule-') as tmp:
 cpp=Path(tmp)/'check.cpp';exe=Path(tmp)/'check';cpp.write_text(preamble+functions+tests)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(root/'XiaoPlayground'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
