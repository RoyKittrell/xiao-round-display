#include <Arduino.h>
#include "UserSettings.h"
#include <Wire.h>
#include <SPI.h>
#include "BoardPlatform.h"
#include "SeasonalAnalemma.h"
#include "TouchNavigation.h"
#include "TouchContact.h"
#include "TouchCalibration.h"
#include "DisplayBusHealth.h"
#include "MoonPhase.h"
#include "MoonTexture.h"
#include "ClockDate.h"
#include "ClockSlew.h"
#include "Celestial.h"
#include "SunTypes.h"
#include "SunInformation.h"
#include "SunSnapshot.h"
#include <Adafruit_GFX.h>
#include "DisplayCanvas.h"
#include <Fonts/FreeSansBold9pt7b.h>
#include <Adafruit_GC9A01A.h>
#include "WatchFace.h"
#include "WatchDirty.h"
#include "FrameTiles.h"
#include "ui/WatchCollection.h"
#include "ui/WatchInput.h"
#include "ui/WatchTitleRenderer.h"
#include "ui/BrightnessGesture.h"
#include "Backlight.h"
#include "DisplaySchedule.h"
#include "SceneControl.h"
#include "GameOfLife.h"
#include "CodexUsageRenderer.h"
#include "PrinterRenderer.h"
#include "WeatherRenderer.h"
#include "star_systems/Renderer.h"
#include "HostLink.h"
#include "SunImageStore.h"
HostLink hostLink;
#ifdef Serial
#undef Serial
#endif
#define Serial hostLink
#if defined(ARDUINO_ARCH_ESP32)
Adafruit_GC9A01A lcd(PIN_D1,PIN_D3,-1);
SunImageStore sunStore;
#else
Adafruit_GC9A01A lcd(PIN_D1, PIN_D3, PIN_D10, PIN_D8, -1);
#endif
DisplayCanvas16 cv(240,240);
FrameTiles frameTiles;
DisplayCanvas16 moonImage(240,240);
#if !defined(ARDUINO_ARCH_ESP32)
GFXcanvas16 sunImage(240,240);
#endif
GFXcanvas1 textMask(240,240);
TouchNavigation touchNavigation;
TouchContact touchContact;
TouchCoordinates touchCoordinates;
TouchCalibration touchCalibration;
ScreenOrientation screenOrientation;
void printOrientation(){Serial.print("ORIENTATION turns=");Serial.println(screenOrientation.turns());}
DisplayBusHealth displayBus;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
const char* firmwareTag="esp32s3-public-r36-wifi-ota-rotation";
#elif defined(ARDUINO_ARCH_ESP32)
const char* firmwareTag="esp32c6-public-r35-wifi-rotation";
#else
const char* firmwareTag="nrf54-public-r35-rotation";
#endif
volatile bool touchReportPending=true;
void touchReportReady(){touchReportPending=true;}
bool traceTouch=false;
bool accelerationFresh=false;
bool firstFrame=true;
const uint16_t BG=0x0843, WHITE=0xffff, CYAN=0x07ff, GOLD=0xfd20;
// IDs 0/1 are retired. USB3-9 retained; USB0 selects Codex usage; USBp selects printer status.
const char* names[]={"ROUND CLOCK","ANALEMMA","MOON","SOLAR SYSTEM","SUN","STAR SYSTEMS","GAME OF LIFE","CODEX USAGE","BAMBU PRINTER","WEATHER"};
constexpr int firstScene=2,sceneCount=sizeof(names)/sizeof(names[0]);
int mode=firstScene;
int previewMonth=0,previewDay=0;
ClockDate buildClock=clockCompiled(__DATE__,__TIME__);
ClockSlew clockSlew;
uint32_t clockSeconds=0,clockTickMs=0,clockFractionMs=0,clockPollMs=0;
bool clockReady=false;
bool rtcObserved=false;
bool clockWritePending=false;
WatchCollection watches;
StarSystems::Playback starPlayback;
StarSystems::Renderer starRenderer;
DisplayLighting lighting;
BrightnessGesture brightnessGesture;
Backlight backlight;
UsbPowerState usbPower;
HalfPastFaceRotation halfPastRotation;
SceneRotation sceneRotation;
SceneControl sceneControl;
static_assert(sceneCount==SceneControl::Count,"Update menu scene catalog when adding a scene");
GameOfLife life;
CodexUsage codexUsage;
PrinterStatus printerStatus;
WeatherStatus weatherStatus;
int weatherHorizon=12;
bool displaySleeping=false;
bool scheduledWakeRepaint=false;
uint8_t usbPowerStatus=0;
bool usbPowerReadOK=false;
uint32_t powerPollAt=0;
int appliedBrightness=-1,appliedPWM=-1;
uint32_t brightnessReportAt=0;
uint32_t brightnessHintUntil=0;
char timeInput[32];bool receivingTime=false;int timeInputLength=0;

float ax=0,ay=0,az=1;
bool imuOK=false,rtcOK=false,buttonWas=false;
uint32_t switched=0,lastFrame=0;
int mediaOwner=-1;
bool infoTarget=false;uint8_t infoOpacity=0;uint32_t fadeTick=0;
bool compressedOrbits=true;
SunMeta sunMeta=sunSnapshotMeta,pendingSunMeta={};
bool sunReady=false,sunReceiving=false;uint8_t sunRows[240]={};int sunRowCount=0;
uint32_t sunPacketAt=0;char packet[2400];bool receivingPacket=false;int packetLength=0;
Celestial::MoonDetails moonFacts={};uint32_t moonFactsEpoch=0;

uint8_t bcd(int n){return (n/10)*16+n%10;}
int dec(uint8_t n){return (n>>4)*10+(n&15);}
bool displayLinesReleased(){return Board::pinLevel(PIN_D4)==HIGH&&Board::pinLevel(PIN_D5)==HIGH;}
bool readRegs(TwoWire &bus,uint8_t addr,uint8_t reg,uint8_t* data,int n){
 // These calls start only between complete synchronous transactions. A line
 // already held low cannot accept START; avoid the core's long timeout so a
 // failed touch/RTC chip cannot freeze the independently rendered clock.
 if(&bus==&Wire&&!displayLinesReleased())return false;
#if defined(ARDUINO_ARCH_ESP32)
 // CHSC6X reports are read directly, as in Seeed's driver; writing a register
 // prefix before a repeated-start can wedge this target on ESP32 I2C.
 if(&bus==&Wire&&addr==0x2e){
  if(bus.requestFrom(addr,uint8_t(n))!=n)return false;
  for(int i=0;i<n;i++)data[i]=bus.read();return true;
 }
#endif
 bus.beginTransmission(addr);bus.write(reg);if(bus.endTransmission(false))return false;
 bus.requestFrom(addr,(uint8_t)n);if(bus.available()<n)return false;
 for(int i=0;i<n;i++)data[i]=bus.read();return true;
}
void printDisplayBus(){
 Serial.print("TOUCH BUS reads=");Serial.print(displayBus.reads);
 Serial.print(" failures=");Serial.print(displayBus.failures);
 Serial.print(" recoveries=");Serial.print(displayBus.recoveries);
 Serial.print(" good_age_ms=");Serial.print(displayBus.goodAge(millis()));
 Serial.print(" SDA=");Serial.print(Board::pinLevel(PIN_D4));
 Serial.print(" SCL=");Serial.println(Board::pinLevel(PIN_D5));
}
bool releaseDisplayLine(uint8_t pin){
 pinMode(pin,INPUT_PULLUP);uint32_t began=micros();
 while(digitalRead(pin)==LOW&&uint32_t(micros()-began)<150){}
 return digitalRead(pin)==HIGH;
}
void lowDisplayLine(uint8_t pin){digitalWrite(pin,LOW);pinMode(pin,OUTPUT);delayMicroseconds(5);}
void recoverDisplayBus(){
 // Release the controller, clock out a target's unfinished byte, then send
 // STOP. Never drive either bus line high; pull-ups release it safely.
 Wire.end();releaseDisplayLine(PIN_D4);bool clockFree=releaseDisplayLine(PIN_D5);
 if(clockFree){
  for(int i=0;i<9&&digitalRead(PIN_D4)==LOW;i++){
   lowDisplayLine(PIN_D5);if(!releaseDisplayLine(PIN_D5))break;delayMicroseconds(5);
  }
  lowDisplayLine(PIN_D5);lowDisplayLine(PIN_D4);
  releaseDisplayLine(PIN_D5);delayMicroseconds(5);releaseDisplayLine(PIN_D4);
 }
 Board::beginDisplayBus();
 touchContact=TouchContact();touchNavigation=TouchNavigation();brightnessGesture.reset();
 touchReportPending=true;
 displayBus.didRecover(millis());clockPollMs=millis()-1000;
 Serial.println("DISPLAY BUS: automatic recovery");printDisplayBus();
}
bool writeReg(TwoWire &bus,uint8_t addr,uint8_t reg,uint8_t value){if(&bus==&Wire&&!displayLinesReleased())return false;bus.beginTransmission(addr);bus.write(reg);bus.write(value);return bus.endTransmission()==0;}

void clockTick(){uint32_t now=millis(),elapsed=now-clockTickMs;clockTickMs=now;clockFractionMs+=clockSlew.advance(elapsed);clockSeconds+=clockFractionMs/1000;clockFractionMs%=1000;}
bool writeClock(const ClockDate& d){
 if(!clockValid(d))return false;
 if(!writeReg(Wire,0x51,0,0x20))return false;
 Wire.beginTransmission(0x51);Wire.write(2);
 Wire.write(bcd(d.second));Wire.write(bcd(d.minute));Wire.write(bcd(d.hour));
 Wire.write(bcd(d.day));Wire.write((clockEpoch(d)/86400+6)%7);Wire.write(bcd(d.month));Wire.write(bcd(d.year-2000));
 bool written=Wire.endTransmission()==0;
 bool started=writeReg(Wire,0x51,0,0);
 return written&&started;
}
void serviceClock(){
 if(!clockReady)return;
 clockTick();if(millis()-clockPollMs<1000)return;clockPollMs=millis();
 // A host time sync can arrive while the shared bus is unavailable. Keep that
 // running time authoritative until the RTC accepts it; never re-adopt the
 // RTC's stale but otherwise valid date when the bus starts responding again.
 if(clockWritePending){
  rtcOK=writeClock(clockFromEpoch(clockSeconds));
  if(rtcOK){clockWritePending=false;rtcObserved=true;Serial.println("RTC pending host sync applied");}
  return;
 }
 uint8_t r[7];if(!readRegs(Wire,0x51,2,r,7)){rtcOK=false;return;}
 ClockDate d={2000+dec(r[6]),dec(r[5]&31),dec(r[3]&63),dec(r[2]&63),dec(r[1]&127),dec(r[0]&127)};
 if((r[0]&0x80)||!clockValid(d)){
  rtcOK=writeClock(clockFromEpoch(clockSeconds));
  if(rtcOK){rtcObserved=true;Serial.println("RTC restored from running clock");}
 }else{
  rtcOK=true;uint32_t observed=clockEpoch(d);
  // RTC reads have only whole seconds. Replacing our fractional phase on every
  // poll makes a sweeping hand visibly jump backwards. Keep the monotonic
  // millisecond clock between samples; only correct a real wall-time change.
  int64_t error=int64_t(observed)-clockSeconds;
  if(!rtcObserved||error>2||error<-2){clockSeconds=observed;clockFractionMs=0;clockTickMs=millis();clockSlew.reset();}
  rtcObserved=true;
 }
}
ClockDate currentClock(){clockTick();return clockFromEpoch(clockSeconds);}
void printClock(){
 auto d=currentClock();char line[80];
 snprintf(line,sizeof(line),"CLOCK %04d-%02d-%02d %02d:%02d:%02d %s RTC=%d",d.year,d.month,d.day,d.hour,d.minute,d.second,UserSettings::TimeZoneLabel,rtcOK);Serial.println(line);
}
void setClockFromSerial(){
 timeInput[timeInputLength]=0;ClockDate d;int ms=0;
 int fields=sscanf(timeInput,"%d-%d-%d %d:%d:%d.%d",&d.year,&d.month,&d.day,&d.hour,&d.minute,&d.second,&ms);
 if(fields<6||!clockValid(d)||ms<0||ms>999){Serial.println("Invalid clock date");return;}
 if(!sceneControl.session())sceneControl.setSession(uint32_t(uint64_t(clockEpoch(d))*1000u+uint32_t(ms)));
 clockTick();int64_t difference=(int64_t(clockEpoch(d))-clockSeconds)*1000+ms-clockFractionMs;
 if(rtcObserved&&difference>=-2000&&difference<=2000)clockSlew.target(int32_t(difference));
 else{clockSeconds=clockEpoch(d);clockTickMs=millis();clockFractionMs=ms;clockSlew.reset();}
 if(difference>120000||difference< -120000)halfPastRotation.reset(clockSeconds);
 clockReady=true;rtcOK=writeClock(d);clockWritePending=!rtcOK;rtcObserved=rtcOK;printClock();
 if(clockWritePending)Serial.println("RTC host sync queued until bus recovers");
}
void watchSelectionChanged(){mediaOwner=-1;firstFrame=true;Serial.print("WATCH FACE ");Serial.println(WatchFace::name(watches.style));}
void nextWatchStyle(){watches.next(millis());watchSelectionChanged();}
void toggleWatchDemo(){watches.toggleDemo(millis());firstFrame=true;Serial.print("WATCH DEMO ");Serial.println(watches.demo?"ON":"OFF");}
void printLighting(){
 Serial.print("APPEARANCE ");Serial.print(lighting.appearance==WatchAppearance::Dark?"DARK":"LIGHT");
 Serial.print(" policy=");Serial.print(lighting.overrideMode==AppearanceOverride::Automatic?"AUTO":lighting.overrideMode==AppearanceOverride::Light?"LIGHT":"DARK");
 Serial.print(" brightness=");Serial.print(lighting.brightness());Serial.println("%");
}
void serviceLighting(){
 bool changed=lighting.update(clockSeconds);
 if(changed){if(mediaOwner==2)mediaOwner=-1;watches.title.repaint();if(mode==2)firstFrame=true;}
 if(appliedBrightness!=lighting.brightness()){
  appliedBrightness=lighting.brightness();
  if(!brightnessGesture.active()||millis()-brightnessReportAt>=100){printLighting();brightnessReportAt=millis();}
 }else if(changed)printLighting();
 int pwm=backlight.update(displaySleeping||scheduledWakeRepaint?0:lighting.brightness(),millis());
 if(pwm!=appliedPWM){appliedPWM=pwm;analogWrite(PIN_D6,pwm);}
}
void adjustBrightness(int delta){lighting.adjust(delta);brightnessHintUntil=millis()+1800;serviceLighting();}
void text(int cx,int cy,const char* s,int size=2,uint16_t col=WHITE){cv.setTextSize(size);cv.setTextColor(col);cv.setCursor(cx-strlen(s)*3*size,cy);cv.print(s);}
void selectScene(int next){if(next<firstScene||next>=firstScene+sceneCount)return;if(mode==7)starPlayback.leave(millis());if(mode==8)life.leave();touchNavigation.sceneChanged();brightnessGesture.reset();brightnessHintUntil=0;previewMonth=previewDay=0;mode=next;switched=millis();sceneRotation.reset(switched);infoTarget=false;infoOpacity=0;fadeTick=millis();firstFrame=true;if(mode==2)watches.enter(millis());if(mode==7){if(!displaySleeping)starPlayback.enter(millis());starRenderer.resetTrails();}if(mode==8&&!displaySleeping)life.enter(millis(),clockSeconds^micros());Serial.print("MODE: ");Serial.println(names[mode-firstScene]);}
void switchMode(){selectScene(sceneControl.next(mode));}
void starSelectionChanged(){mediaOwner=-1;firstFrame=true;starRenderer.resetTrails();Serial.print("STAR SYSTEM ");Serial.println(StarSystems::Data::Titles[int(starPlayback.system)]);}
void toggleHistoricalTransfer(){starPlayback.toggleHistorical(millis());Serial.print("STAR HISTORICAL ");Serial.println(starPlayback.historical?"ON / artistic past transfer, not observed today":"OFF / present-day decretion disc");}
void starScene(){
 if(starPlayback.advance(millis()))starSelectionChanged();
 if(mediaOwner!=7){starRenderer.background(moonImage.getBuffer(),starPlayback.system);mediaOwner=7;}
 starRenderer.render(cv.getBuffer(),moonImage.getBuffer(),starPlayback,millis());
}
void lifeScene(){life.advance(millis());life.render(cv.getBuffer());}
uint32_t unixClock(){return clockSeconds+UserSettings::UnixAtCivilEpoch;} // Configured civil epoch -> Unix
void codexScene(){
 static uint32_t drawnSecond=0,drawnRevision=0;
 if(mediaOwner!=9||drawnSecond!=clockSeconds||drawnRevision!=codexUsage.revision()){
  CodexUsageView::render(moonImage.getBuffer(),codexUsage,unixClock());
  mediaOwner=9;drawnSecond=clockSeconds;drawnRevision=codexUsage.revision();
 }
 cv.drawRGBBitmap(0,0,moonImage.getBuffer(),240,240);
}
void printerScene(){
 static uint32_t drawnSecond=0,drawnRevision=0;
 if(mediaOwner!=10||drawnSecond!=clockSeconds||drawnRevision!=printerStatus.revision()){
  PrinterView::render(moonImage.getBuffer(),printerStatus,unixClock());
  mediaOwner=10;drawnSecond=clockSeconds;drawnRevision=printerStatus.revision();
 }
 cv.drawRGBBitmap(0,0,moonImage.getBuffer(),240,240);
}
void weatherScene(){
 static uint32_t drawnMinute=0,drawnRevision=0;static int drawnHorizon=0;
 if(mediaOwner!=11||drawnMinute!=clockSeconds/60||drawnRevision!=weatherStatus.revision()||drawnHorizon!=weatherHorizon){
  WeatherView::render(moonImage.getBuffer(),weatherStatus,unixClock(),weatherHorizon);
  mediaOwner=11;drawnMinute=clockSeconds/60;drawnRevision=weatherStatus.revision();drawnHorizon=weatherHorizon;
 }
 cv.drawRGBBitmap(0,0,moonImage.getBuffer(),240,240);
}
void sceneTap(int tapX,int tapY){
 if(mode==2){auto action=WatchUI::tap(tapX,tapY);if(action==WatchUI::Tap::NextFace)nextWatchStyle();}
 else if(mode==4||mode==6){infoTarget=!infoTarget;fadeTick=millis();Serial.print("TAP INFO ");Serial.println(infoTarget);}
 else if(mode==5){compressedOrbits=!compressedOrbits;mediaOwner=-1;}
 else if(mode==9){codexUsage.next();}
 else if(mode==11){weatherHorizon=weatherHorizon==12?24:12;Serial.print("WEATHER HORIZON ");Serial.println(weatherHorizon);}
 else if(mode==8){life.reseed(millis(),clockSeconds^micros());Serial.println("LIFE reseeded");}
 else if(mode==7){
  auto action=StarSystems::tap(starPlayback.system,tapX,tapY);
  if(action==StarSystems::Tap::Next){starPlayback.next(millis());starSelectionChanged();}
  else if(action==StarSystems::Tap::Historical)toggleHistoricalTransfer();
 }
}
void touchSample(bool down,int touchX,int touchY){
  auto brightness=brightnessGesture.update(down,touchX,touchY,lighting.brightness(),millis());
  if(brightness.began){
   touchNavigation.sceneChanged();
   if(mode==2&&watches.title.active()){watches.title.dismiss();firstFrame=true;}
   if(traceTouch)Serial.println("BRIGHTNESS GRAB");
  }
  if(brightness.consumed)brightnessHintUntil=millis()+1400;
  if(brightness.changed)adjustBrightness(brightness.level-lighting.brightness());
  if(brightness.ended){printLighting();if(traceTouch)Serial.println("BRIGHTNESS RELEASE");}
  if(brightness.consumed){touchNavigation.update(false,0,0,millis());return;}
 auto event=touchNavigation.update(down,touchX,touchY,millis());
 if(event.kind==TouchNavigation::NextScene){Serial.println("TOP DOUBLE TAP: next scene");switchMode();}
 else if(event.kind==TouchNavigation::Tap)sceneTap(event.x,event.y);
}
void startTouchCalibration(){
 watches.demo=false;selectScene(2);touchCalibration.begin(screenOrientation.turns());
 touchNavigation.sceneChanged();brightnessGesture.reset();
 firstFrame=true;Serial.println("CALIBRATION START: hold each gold dot, then lift");
}
void renderTouchCalibration(){
 cv.fillScreen(BG);cv.drawCircle(120,120,113,0x4a69);
 text(120,85,"TOUCH SETUP",1,GOLD);
 char step[20];snprintf(step,sizeof(step),"DOT %d OF 4",touchCalibration.index()+1);text(120,104,step,1);
 text(120,123,touchCalibration.liftFinger()?"LIFT YOUR FINGER":"HOLD THE GOLD DOT",1);
 auto p=TouchCalibration::target(touchCalibration.index());
 uint16_t colour=touchCalibration.liftFinger()?0x07e0:GOLD;
 cv.drawCircle(p.x,p.y,13,colour);cv.fillCircle(p.x,p.y,7,colour);
 cv.drawLine(p.x-19,p.y,p.x-10,p.y,colour);cv.drawLine(p.x+10,p.y,p.x+19,p.y,colour);
 cv.drawLine(p.x,p.y-19,p.x,p.y-10,colour);cv.drawLine(p.x,p.y+10,p.x,p.y+19,colour);
}
void input(){
 // Continue fades while full-screen transfers service input between strips.
 serviceLighting();
 bool pressed=digitalRead(PIN_BUTTON)==LOW;
 if(!displaySleeping&&pressed&&!buttonWas&&millis()-switched>350){if(touchCalibration.active()){touchCalibration.cancel();firstFrame=true;}else switchMode();}buttonWas=pressed;
 if(!displaySleeping){
 bool packetReceived=false,contactPressed=false;int touchX=-1,touchY=-1;
 static uint32_t touchReadAt=0;
 // Latch short IRQ pulses even during drawing. Read quickly during contact,
 // but stop hammering the controller at 125 Hz while the panel is idle.
 // The slow fallback also catches a report missed during startup/recovery.
 bool reportWanted=touchReportPending||digitalRead(PIN_D7)==LOW||touchContact.active()||
  uint32_t(millis()-touchReadAt)>=250;
 if(reportWanted&&uint32_t(millis()-touchReadAt)>=displayBus.pollInterval()){
  touchReadAt=millis();
  noInterrupts();touchReportPending=false;interrupts();
  uint8_t t[5];
  bool good=readRegs(Wire,0x2e,0,t,5);
  displayBus.report(good,millis());
  if(!good)touchReportPending=true;
  if(good){
   packetReceived=true;contactPressed=t[0]==1;
   touchX=t[2];touchY=t[4];
  }
 }
 if(displayBus.recoveryDue(millis())){recoverDisplayBus();packetReceived=false;contactPressed=false;}
 auto contact=touchContact.update(packetReceived,contactPressed,touchX,touchY,millis());
 auto calibrated=touchCoordinates.map(contact.x,contact.y);
 auto mapped=screenOrientation.touch(calibrated.x,calibrated.y);
 static bool lastTouchDown=false;
 if(traceTouch&&contact.down!=lastTouchDown){Serial.print(contact.down?"TOUCH DOWN raw=":"TOUCH UP raw=");Serial.print(contact.x);Serial.print(",");Serial.print(contact.y);Serial.print(" mapped=");Serial.print(mapped.x);Serial.print(",");Serial.print(mapped.y);Serial.print(" ms=");Serial.println(millis());}
 lastTouchDown=contact.down;
 if(touchCalibration.active()){
  auto event=touchCalibration.update(contact.down,contact.x,contact.y,millis());
  if(event!=TouchCalibration::None)firstFrame=true;
  if(event==TouchCalibration::Captured){auto p=touchCalibration.rawPoint(touchCalibration.index());Serial.print("CALIBRATION DOT ");Serial.print(touchCalibration.index()+1);Serial.print(" raw=");Serial.print(p.x);Serial.print(",");Serial.println(p.y);}
  if(event==TouchCalibration::Complete){
   touchCoordinates.orientation=touchCalibration.result;Board::saveTouch(touchCoordinates.orientation);touchNavigation.sceneChanged();watches.enter(millis());
   Serial.print("CALIBRATION COMPLETE orientation=");Serial.print(touchCalibration.result);Serial.print(" error=");Serial.println(touchCalibration.error);
  }else if(event==TouchCalibration::Failed){Serial.print("CALIBRATION FAILED error=");Serial.println(touchCalibration.error);}
 }else touchSample(contact.down,mapped.x,mapped.y);
 }
 while(Serial.available()){char c=Serial.read();
 static uint16_t hostEpoch=0;
 if(hostEpoch!=Serial.epoch()){receivingPacket=receivingTime=false;packetLength=timeInputLength=0;hostEpoch=Serial.epoch();}
 if(receivingPacket){
  if(c=='\n'){packet[packetLength]=0;receivingPacket=false;handleSunPacket();}
  else if(c!='\r'&&packetLength<int(sizeof(packet))-1)packet[packetLength++]=c;
  else if(packetLength>=int(sizeof(packet))-1){receivingPacket=false;Serial.println("ERR packet too long");}
  continue;
 }
 if(c=='@'){receivingPacket=true;packetLength=0;continue;}

 if(receivingTime){
  if(c=='\n'||c=='\r'){receivingTime=false;setClockFromSerial();}
  else if(timeInputLength<31)timeInput[timeInputLength++]=c;
  else{receivingTime=false;Serial.println("Clock command too long");}
  continue;
 }
 if(c=='T'){receivingTime=true;timeInputLength=0;continue;}
 if(c=='?'){Serial.print("FIRMWARE ");Serial.println(firmwareTag);Serial.diagnostics();
#if defined(ARDUINO_ARCH_ESP32)
 Serial.wifiStatus();
#endif
 printOrientation();Serial.print("TOUCH MAP ");Serial.println(touchCoordinates.orientation);printDisplayBus();printClock();printLighting();printDisplaySchedule();Serial.print("BACKLIGHT PWM ");Serial.print(appliedPWM);Serial.println("/4095");printSpaceStatus();continue;}
 if(c=='c'){startTouchCalibration();continue;}
 if(c=='+'||c=='-'){adjustBrightness(c=='+'?5:-5);continue;}
 if(c=='v'){lighting.overrideMode=AppearanceOverride((int(lighting.overrideMode)+1)%3);serviceLighting();printLighting();continue;}
 if(c=='a'){lighting.overrideMode=AppearanceOverride::Automatic;serviceLighting();printLighting();continue;}
 if(c=='t'){traceTouch=!traceTouch;Serial.print("TOUCH TRACE ");Serial.println(traceTouch?"ON":"OFF");continue;}
 if(c=='i'){sceneTap(120,120);continue;}
 if(c=='d'&&mode==2){toggleWatchDemo();continue;}
 if(c=='d'&&mode==7){starPlayback.toggleDemo(millis());starRenderer.resetTrails();Serial.print("STAR DEMO ");Serial.println(starPlayback.demo?"ON":"OFF");continue;}
 if(c=='h'&&mode==7){if(starPlayback.system==StarSystems::System::HR)toggleHistoricalTransfer();continue;}
 if(c=='w'){selectScene(11);}else if(c=='p'){selectScene(10);}else if(c=='0'){selectScene(9);}else if(c>='3'&&c<='9'){selectScene(c-'1');}else if(c=='n')switchMode();else if(c=='r')simulateDisplayReset();else if(c=='j'){selectScene(3);previewMonth=6;previewDay=21;}else if(c=='e'){selectScene(3);previewMonth=9;previewDay=23;}else if(c=='l'){selectScene(3);}}
 Serial.endResponse();
}
bool acceleration(){
#if !defined(ARDUINO_ARCH_ESP32)
 uint8_t d[6];if(imuOK&&readRegs(Wire1,0x6a,0x28,d,6)){ax=(int16_t)(d[0]|d[1]<<8)*0.000061f;ay=(int16_t)(d[2]|d[3]<<8)*0.000061f;az=(int16_t)(d[4]|d[5]<<8)*0.000061f;return true;}
#endif
 return false;
}
void clockFace(){
 clockTick();
 if(!watches.enabledMask){
  watches.title.dismiss();cv.fillScreen(0);text(120,97,"NO CLOCK FACES",1,WHITE);text(120,119,"Choose faces in the",1,WHITE);text(120,135,"Mac helper menu",1,WHITE);return;
 }
 if(watches.title.tick(millis())){firstFrame=true;Serial.println("WATCH TITLE END");}
 if(watches.advanceDemo(millis()))watchSelectionChanged();
 if(mediaOwner!=2){WatchFace::dial(moonImage.getBuffer(),watches.style,lighting.appearance);mediaOwner=2;}
 if(watches.title.active()){
  if(watches.title.needsPaint()){
   WatchUI::title(cv.getBuffer(),moonImage.getBuffer(),watches.style,watches.demo,lighting.appearance);watches.title.didPaint();
   Serial.print("WATCH TITLE ");Serial.println(WatchFace::name(watches.style));
  }
 }else{
  double seconds=double(clockSeconds%86400)+clockFractionMs/1000.0;
  WatchFace::hands(cv.getBuffer(),moonImage.getBuffer(),seconds,watches.style,lighting.appearance);
 }
}
void brightnessOverlay(){
 if(mode==2&&watches.title.active())return;
 if(brightnessHintUntil&&int32_t(brightnessHintUntil-millis())>0){
   bool dark=lighting.appearance==WatchAppearance::Dark;
   uint16_t paper=dark?WatchFace::rgb(13,20,26):WatchFace::rgb(244,236,218),ink=dark?0xe71b:0x2105;
   cv.fillRoundRect(51,170,138,30,7,paper);cv.drawRoundRect(51,170,138,30,7,0x8c0c);
   char label[24];snprintf(label,sizeof(label),"BRIGHTNESS %d%%",lighting.brightness());
   WatchFace::Detail::centred(cv.getBuffer(),ClockFonts::Small,label,120,187,ink);
   cv.drawFastHLine(64,193,112,0x8c0c);
   cv.drawFastHLine(64,193,(112*lighting.brightness()+50)/100,ink);
 }
}
void analemma(){
 using namespace SeasonalAnalemma;
 auto date=currentClock();int year=date.year,month=date.month,day=date.day;bool valid=clockReady;
 if(previewMonth){year=2026;month=previewMonth;day=previewDay;valid=true; text(120,8,"PREVIEW",1,GOLD);}
 static int cachedYear=0;static Seasons dates;
 if(cachedYear!=year){dates=seasons(year);cachedYear=year;}
 // Preserve the 1080x1920 portrait-video proportions with uniform scaling.
 const float cx=120,cy=99.5f,ry=86,rx=ry*(1080.0f*0.050f)/(1920.0f*0.055f);
 Point last=point(0);
 for(int i=1;i<=360;i++){
  Point next=point(i*2*M_PI/360);
  cv.drawLine(cx+rx*last.x,cy+ry*last.y,cx+rx*next.x,cy+ry*next.y,WHITE);last=next;
 }
 Point marker=point(phase(year,month,day,dates));
 int px=lroundf(cx+rx*marker.x),py=lroundf(cy+ry*marker.y);
 cv.fillCircle(px,py,7,BG);cv.fillCircle(px,py,5,GOLD);cv.fillCircle(px-1,py-1,1,WHITE);
 char label[24];
 int juneDay=dates.june-dayOfYear(year,6,1)+1,decDay=dates.december-dayOfYear(year,12,1)+1;
 snprintf(label,sizeof(label),"%02d JUN",juneDay);text(120,18,label,1);
 snprintf(label,sizeof(label),"%02d DEC",decDay);text(120,210,label,1);
 const char* months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
 snprintf(label,sizeof(label),"%02d %s %d",day,months[month-1],year);text(120,221,label,1,GOLD);
 if(valid){
  int today=dayOfYear(year,month,day);
  const char* event=nullptr;
  if(today==dates.march||today==dates.september)event="EQUINOX";
  else if(today==dates.june||today==dates.december)event="SOLSTICE";
  if(event){cv.setTextSize(1);cv.setTextColor(WHITE);cv.setCursor(143,96);cv.print(event);}
 }
 if(!valid)text(120,11,"SET RTC DATE",1,GOLD);
}

void moon(){
 auto date=currentClock();bool valid=clockReady;int yy=date.year,mm=date.month,dd=date.day,hh=date.hour,mi=date.minute,ss=date.second;
 static double renderedJD=-1;
 double jd=MoonPhase::julian(yy,mm,dd,hh,mi,ss);
 if(renderedJD<0||mediaOwner!=4||fabs(jd-renderedJD)>1.0/1440){
  renderedJD=jd;mediaOwner=4;moonFacts=Celestial::moon(date);moonFactsEpoch=clockEpoch(date);auto phase=MoonPhase::calculate(jd);phase.lit=moonFacts.lit;phase.inc=acos(2*phase.lit-1);
  moonImage.fillScreen(0);
  // Optional observer orientation, configured locally.
  float q=UserSettings::HasObserver ? MoonPhase::parallactic(jd,phase,UserSettings::Latitude,UserSettings::Longitude):0;
  float limb=phase.angle-q;
  float lightX=-sinf(limb)*sin(phase.inc),lightY=-cosf(limb)*sin(phase.inc),lightZ=cos(phase.inc);
  float cq=cosf(q),sq=sinf(q);
  for(int y=6;y<234;y++)for(int x=6;x<234;x++){
   float nx=(x-119.5f)/113.0f,ny=(y-119.5f)/113.0f,r2=nx*nx+ny*ny;if(r2>1)continue;
   float z=sqrtf(1-r2),illum=nx*lightX+ny*lightY+z*lightZ;
   float tx=cq*nx+sq*ny,ty=-sq*nx+cq*ny;
   int ix=constrain(int(lroundf(119.5f+113*tx)),0,239),iy=constrain(int(lroundf(119.5f+113*ty)),0,239);
   float texture=moonTexture[iy*240+ix];
   float brightness=illum>0?(0.65f+0.35f*sqrtf(illum)):.035f;
   int grey=constrain(int(texture*brightness),0,255);
   moonImage.drawPixel(x,y,((grey>>3)<<11)|((grey>>2)<<5)|(grey>>3));
  }
  Serial.print("Moon illuminated: ");Serial.print(phase.lit*100,1);Serial.println("%");
 }
 cv.drawRGBBitmap(0,0,moonImage.getBuffer(),240,240);
 if(!valid)text(120,210,"SET RTC DATE",1,GOLD);
 if(infoOpacity)moonInformation(date);
}
uint16_t colourFade(uint16_t a,uint16_t b,uint8_t alpha){
 int r=((a>>11)&31)*(255-alpha)+((b>>11)&31)*alpha;
 int g=((a>>5)&63)*(255-alpha)+((b>>5)&63)*alpha;
 int bl=(a&31)*(255-alpha)+(b&31)*alpha;
 return ((r/255)<<11)|((g/255)<<5)|(bl/255);
}
void advanceInfoFade(){uint32_t now=millis(),dt=now-fadeTick;fadeTick=now;int step=max(1,int(min(dt,2000UL)*255/1600));int target=infoTarget?255:0;if(infoOpacity<target)infoOpacity=min(target,int(infoOpacity)+step);else if(infoOpacity>target)infoOpacity=max(target,int(infoOpacity)-step);}
void dimForInformation(){
 uint8_t amount=uint16_t(infoOpacity)*195/255;
 uint16_t* pixels=cv.getBuffer();for(int i=0;i<240*240;i++)pixels[i]=colourFade(pixels[i],0,amount);
}
void overlayText(int cx,int top,const char* value,uint16_t colour,bool bold=false){
 if(!infoOpacity)return;
 textMask.fillScreen(0);textMask.setFont(bold?&FreeSansBold9pt7b:nullptr);textMask.setTextSize(1);textMask.setTextColor(1);
 int16_t bx,by;uint16_t w,h;textMask.getTextBounds(value,0,0,&bx,&by,&w,&h);
 int left=cx-int(w)/2;int baseline=top-by;textMask.setCursor(left-bx,baseline);textMask.print(value);
 for(int yy=max(0,top);yy<min(240,top+int(h));yy++)for(int xx=max(0,left);xx<min(240,left+int(w));xx++)if(textMask.getPixel(xx,yy))cv.drawPixel(xx,yy,colourFade(cv.getPixel(xx,yy),colour,infoOpacity));
}
void dateLabel(uint32_t epoch,char* out,size_t n,bool withTime=true){
 if(!epoch){snprintf(out,n,"Unavailable");return;}
 auto d=clockFromEpoch(epoch);const char* months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
 if(withTime)snprintf(out,n,"%02d %s %02d:%02d %s",d.day,months[d.month-1],d.hour,d.minute,UserSettings::TimeZoneLabel);else snprintf(out,n,"%02d %s %04d",d.day,months[d.month-1],d.year);
}
void moonInformation(const ClockDate& date){
 dimForInformation();char line[48];
 overlayText(120,41,UserSettings::LocationLabel,CYAN);
 overlayText(120,62,Celestial::phaseName(moonFacts.phase),WHITE,true);
 snprintf(line,sizeof(line),"%.1f%% ILLUMINATED",moonFacts.lit*100);overlayText(120,88,line,WHITE);
 overlayText(120,109,moonFacts.fullToday?"TODAY'S TRADITIONAL NAME":"NEXT FULL MOON",GOLD);
 overlayText(120,127,moonFacts.name,moonFacts.fullToday?0xfca0:0xffb5,true);
 dateLabel(moonFacts.fullEpoch,line,sizeof(line));overlayText(120,151,line,WHITE);
 const char* special=moonFacts.supermoon?"SUPERMOON":(moonFacts.blue?"BLUE MOON":"");
 if(*special){snprintf(line,sizeof(line),"%s%s",moonFacts.fullToday?"":"NEXT: ",special);overlayText(120,171,line,moonFacts.blue?CYAN:0xf81f,true);}
 snprintf(line,sizeof(line),"DISTANCE %.0f KM",moonFacts.distance);overlayText(120,198,line,CYAN);
}
void solarPoint(const Celestial::XYZ& p,int& x,int& y){double r=hypot(p.x,p.y);double radius=compressedOrbits?108*log1p(r/.20)/log1p(30.5/.20):108*r/30.5;double k=r?radius/r:0;x=lround(120+p.x*k);y=lround(120-p.y*k);}
void solarSystem(){
 auto date=currentClock();auto now=Celestial::time(date);static double renderedTime=-1;static bool oldScale=false;
 if(mediaOwner!=5||fabs(now.ut-renderedTime)>1.0/1440||oldScale!=compressedOrbits){
  mediaOwner=5;renderedTime=now.ut;oldScale=compressedOrbits;moonImage.fillScreen(0);
  double centuries=now.tt/36525;const uint16_t colours[]={0xbdd7,0xff33,0x3e9f,0xfac6,0xe530,0xff15,0x5ffe,0x327f};
  for(int i=0;i<8;i++){
   int px=0,py=0;
   for(int step=0;step<=180;step++){auto p=Celestial::orbit(i,centuries,step*2*M_PI/180);int x,y;solarPoint(p,x,y);if(step)moonImage.drawLine(px,py,x,y,0x2106);px=x;py=y;}
  }
  moonImage.fillCircle(120,120,compressedOrbits?5:2,GOLD);
  const char* labels[]={"Me","V","E","Ma","J","S","U","N"};int labelX[8]={},labelY[8]={},labelW[8]={};
  for(int i=0;i<8;i++){
   auto p=Celestial::planet(i,now);int x,y;solarPoint(p,x,y);int r=compressedOrbits?(i==4?5:(i>=5?4:2)):2;
   moonImage.fillCircle(x,y,r,colours[i]);if(i==5&&compressedOrbits)moonImage.drawLine(x-7,y+3,x+7,y-3,colours[i]);
   if(compressedOrbits){
    int tx=constrain(x+6,10,215),ty=constrain(y-4,12,215),width=strlen(labels[i])*6;
    for(int attempt=0;attempt<8;attempt++){bool overlap=false;for(int j=0;j<i;j++)if(tx<labelX[j]+labelW[j]+2&&tx+width+2>labelX[j]&&ty<labelY[j]+9&&ty+9>labelY[j])overlap=true;if(!overlap)break;ty=constrain(ty-10,12,215);}
    labelX[i]=tx;labelY[i]=ty;labelW[i]=width;
    if(ty!=y-4)moonImage.drawLine(x+r,y,tx-2,ty+4,0x4208);
    moonImage.setFont(nullptr);moonImage.setTextSize(1);moonImage.setTextColor(colours[i]);moonImage.setCursor(tx,ty);moonImage.print(labels[i]);
   }
   Serial.print("PLANET ");Serial.print(i);Serial.print(" ");Serial.print(p.x,7);Serial.print(" ");Serial.println(p.y,7);
  }
  moonImage.setFont(nullptr);moonImage.setTextSize(1);moonImage.setTextColor(0x7bef);const char* scale=compressedOrbits?"LOG DISTANCE":"TRUE AU SCALE";moonImage.setCursor(120-strlen(scale)*3,220);moonImage.print(scale);
 }
 cv.drawRGBBitmap(0,0,moonImage.getBuffer(),240,240);
}
void sun(){
 bool live=sunReady&&!sunReceiving;
#if defined(ARDUINO_ARCH_ESP32)
 if(live&&mediaOwner!=6){if(sunStore.load(moonImage.getBuffer()))mediaOwner=6;else sunReady=live=false;}
 const SunMeta& m=live?sunMeta:sunSnapshotMeta;
 cv.drawRGBBitmap(0,0,live?moonImage.getBuffer():sunSnapshot,240,240);
#else
 const SunMeta& m=live?sunMeta:sunSnapshotMeta;
 cv.drawRGBBitmap(0,0,live?sunImage.getBuffer():sunSnapshot,240,240);
#endif
 if(!live){text(120,109,"SUN DATA NOT CONNECTED",1,GOLD);text(120,130,"Configure the Mac helper",1,WHITE);}
 if(infoOpacity){
  dimForInformation();SunInformation::Panel panel(m,clockSeconds);
  const uint16_t colours[]={WHITE,GOLD,CYAN,0xfca0};
  for(int i=0;i<panel.count;i++){const auto& row=panel.rows[i];overlayText(120,row.top,row.text,colours[row.tone],row.bold);}
 }else if(sunReceiving)text(120,220,"UPDATING",1,GOLD);
 else if(clockSeconds>m.imageEpoch+24*3600)text(120,220,"OLDER IMAGE - TAP INFO",1,GOLD);
}
uint16_t rowCRC(const uint8_t* data,size_t n){uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=uint16_t(data[i])<<8;for(int b=0;b<8;b++)c=(c&0x8000)?(c<<1)^0x1021:c<<1;}return c;}
int hexDigit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
void applyMenuControl(const SceneControl::Settings& next){
  auto old=sceneControl.current();bool changed=sceneControl.apply(next);
  if(changed){
   if(old.clockMask!=next.clockMask){watches.setMask(next.clockMask,millis());watchSelectionChanged();}
   if(old.mask!=next.mask||old.carousel!=next.carousel||old.seconds!=next.seconds)sceneRotation.reset(millis());
   sceneRotation.configure(next.seconds*1000u,millis());
   lighting.overrideMode=AppearanceOverride(next.appearance);
   if(next.brightness&&sceneControl.session()!=0&&next.targetBoot==sceneControl.session()){lighting.dayBrightness=next.day;lighting.nightBrightness=next.night;}
   if(sceneControl.selectRequested())selectScene(next.scene);
   serviceDisplaySchedule();serviceLighting();
  }
}
void handleSunPacket(){
 if(packet[0]=='O'){
  uint8_t wanted=4;if(!ScreenOrientation::parse(packet,wanted)){Serial.println("ERR orientation packet");return;}
  if(wanted<4&&touchCalibration.active()){Serial.println("ERR orientation calibration active");return;}
  if(wanted<4&&screenOrientation.set(wanted)){
   Board::saveRotation(wanted);lcd.setRotation(wanted);firstFrame=true;
   touchNavigation.sceneChanged();brightnessGesture.reset();touchContact=TouchContact();
  }
  printOrientation();return;
 }

#if defined(ARDUINO_ARCH_ESP32)
 if(packet[0]=='N'&&packet[1]=='|'){
  bool ok=Serial.provisionWifi(packet);memset(packet,0,sizeof(packet));
  Serial.println(ok?"ACK N saved; restarting":"ERR WiFi setup rejected");
  if(ok){delay(200);ESP.restart();}return;
 }
#endif
 if(packet[0]=='W'&&packet[1]=='|'){
  if(weatherStatus.accept(packet,unixClock()))Serial.println("ACK W");else Serial.println("ERR weather packet");
  return;
 }
 if(packet[0]=='C'&&packet[1]=='|'){
  SceneControl::Settings next;
  if(!sceneControl.parse(packet,next)){Serial.println("ERR control packet");return;}
  applyMenuControl(next);
  Serial.print("ACK C|");Serial.print(next.revision);Serial.print("|");Serial.print(mode);Serial.print("|");Serial.print(next.mask);Serial.print("|");Serial.print(next.carousel?1:0);Serial.print("|");Serial.print(next.seconds);Serial.print("|");Serial.print(lighting.dayBrightness);Serial.print("|");Serial.print(lighting.nightBrightness);Serial.print("|");Serial.print(int(lighting.overrideMode));Serial.print("|");Serial.print(displaySleeping?1:0);Serial.print("|");Serial.print(next.power);Serial.print("|");Serial.print(sceneControl.session());Serial.print("|");Serial.println(watches.enabledMask);
  return;
 }
 if(packet[0]=='M'&&packet[1]=='|'){
  if(printerStatus.accept(packet,millis(),unixClock()))Serial.println("ACK M");else Serial.println("ERR printer packet");
  return;
 }
 if(packet[0]=='U'&&packet[1]=='|'){
  if(codexUsage.accept(packet,millis(),unixClock()))Serial.println("ACK U");else Serial.println("ERR usage packet");
  return;
 }
 sunPacketAt=millis();
 if(packet[0]=='B'&&packet[1]=='|'){
  char* fields[13];int count=0;char* p=strtok(packet,"|");while(p&&count<13){fields[count++]=p;p=strtok(nullptr,"|");}
  if(count!=13){Serial.println("ERR metadata");return;}
  pendingSunMeta.imageEpoch=strtoul(fields[1],nullptr,10);pendingSunMeta.fetchedEpoch=strtoul(fields[2],nullptr,10);pendingSunMeta.kpEpoch=strtoul(fields[3],nullptr,10);pendingSunMeta.scalesEpoch=strtoul(fields[4],nullptr,10);pendingSunMeta.flareEpoch=strtoul(fields[5],nullptr,10);pendingSunMeta.xrayEpoch=strtoul(fields[6],nullptr,10);pendingSunMeta.kp=atof(fields[7]);pendingSunMeta.g=atoi(fields[8]);pendingSunMeta.r=atoi(fields[9]);pendingSunMeta.s=atoi(fields[10]);snprintf(pendingSunMeta.xray,12,"%s",fields[11]);snprintf(pendingSunMeta.flare,12,"%s",fields[12]);
#if defined(ARDUINO_ARCH_ESP32)
  if(!sunStore.start()){Serial.println("ERR Sun storage unavailable");return;}
#endif
  memset(sunRows,0,sizeof(sunRows));sunRowCount=0;sunReceiving=true;Serial.println("ACK B");return;
 }
 if(packet[0]=='P'&&packet[1]=='|'&&sunReceiving){
  char* rowText=strtok(packet+2,"|");char* colText=strtok(nullptr,"|");char* hex=strtok(nullptr,"|");char* checksum=strtok(nullptr,"|");
  int row=rowText?atoi(rowText):-1,col=colText?atoi(colText):-1;
  if(row<0||row>=240||col<0||col>=240||col%32||!hex||!checksum){Serial.println("ERR patch");return;}
  int pixels=min(32,240-col),bytes=pixels*2;if(strlen(hex)!=size_t(bytes*2)||strlen(checksum)!=4){Serial.println("ERR patch length");return;}
  uint8_t decoded[64];for(int i=0;i<bytes;i++){int a=hexDigit(hex[2*i]),b=hexDigit(hex[2*i+1]);if(a<0||b<0){Serial.println("ERR hex");return;}decoded[i]=(a<<4)|b;}
  if(rowCRC(decoded,bytes)!=strtoul(checksum,nullptr,16)){Serial.println("ERR CRC");return;}
#if defined(ARDUINO_ARCH_ESP32)
  uint16_t words[32];for(int x=0;x<pixels;x++)words[x]=(decoded[2*x]<<8)|decoded[2*x+1];
  if(!sunStore.write(row,col,words,pixels)){sunReceiving=false;sunStore.cancel();Serial.println("ERR Sun storage write");return;}
#else
  for(int x=0;x<pixels;x++)sunImage.getBuffer()[row*240+col+x]=(decoded[2*x]<<8)|decoded[2*x+1];
#endif
  bool complete=sunRows[row]==255;sunRows[row]|=1<<(col/32);if(!complete&&sunRows[row]==255)sunRowCount++;
  Serial.print("ACK P ");Serial.print(row);Serial.print(" ");Serial.println(col);return;
 }
 if(packet[0]=='R'&&packet[1]=='|'&&sunReceiving){
  char* rowText=strtok(packet+2,"|");char* hex=strtok(nullptr,"|");char* checksum=strtok(nullptr,"|");int row=rowText?atoi(rowText):-1;
  if(row<0||row>=240||!hex||strlen(hex)!=960||!checksum||strlen(checksum)!=4){Serial.println("ERR row");return;}
  uint8_t decoded[480];for(int i=0;i<480;i++){int a=hexDigit(hex[2*i]),b=hexDigit(hex[2*i+1]);if(a<0||b<0){Serial.println("ERR hex");return;}decoded[i]=(a<<4)|b;}
  if(rowCRC(decoded,480)!=strtoul(checksum,nullptr,16)){Serial.println("ERR CRC");return;}
#if defined(ARDUINO_ARCH_ESP32)
  uint16_t words[240];for(int x=0;x<240;x++)words[x]=(decoded[2*x]<<8)|decoded[2*x+1];
  if(!sunStore.write(row,0,words,240)){sunReceiving=false;sunStore.cancel();Serial.println("ERR Sun storage write");return;}
#else
  for(int x=0;x<240;x++)sunImage.getBuffer()[row*240+x]=(decoded[2*x]<<8)|decoded[2*x+1];
#endif
  if(sunRows[row]!=255){sunRows[row]=255;sunRowCount++;}Serial.print("ACK R ");Serial.println(row);return;
 }
 if(packet[0]=='E'&&sunReceiving&&sunRowCount==240){
#if defined(ARDUINO_ARCH_ESP32)
  if(!sunStore.commit()){sunReceiving=false;Serial.println("ERR Sun commit");return;}if(mode==6)mediaOwner=-1;
#endif
  sunMeta=pendingSunMeta;sunReady=true;sunReceiving=false;Serial.println("ACK E");return;}
 Serial.println("ERR incomplete solar update");
}
void printSpaceStatus(){
 Serial.print("SCENE ");Serial.println(names[mode-firstScene]);
 Serial.print("WEATHER ready=");Serial.print(weatherStatus.ready());Serial.print(" horizon=");Serial.print(weatherHorizon);Serial.print(" stale=");Serial.print(weatherStatus.stale(unixClock()));Serial.print(" observed=");Serial.print(weatherStatus.current().observed);Serial.print(" temperature10=");Serial.print(weatherStatus.current().temperature10);Serial.print(" uv10=");Serial.println(weatherStatus.current().uv10);
 if(mode==2){Serial.print("WATCH FACE ");Serial.println(WatchFace::name(watches.style));Serial.print("WATCH DEMO ");Serial.println(watches.demo?"ON":"OFF");Serial.print("WATCH TITLE ACTIVE ");Serial.println(watches.title.active());}
 if(mode==7){Serial.print("STAR SYSTEM ");Serial.println(StarSystems::Data::Titles[int(starPlayback.system)]);Serial.print("STAR DEMO ");Serial.println(starPlayback.demo?"ON":"OFF");Serial.print("STAR HISTORICAL ");Serial.println(starPlayback.historical?"ON / artistic past transfer":"OFF / present-day");Serial.println("STAR SCHEMATIC expanded separations / source clocks; not a live ephemeris");}
 if(mode==8){Serial.print("LIFE rules=B3/S23 grid=128x128 viewport=60x60 interval_ms=150 generation=");Serial.print(life.generation());Serial.print(" population=");Serial.print(life.alive());Serial.print(" seeds=");Serial.println(life.seeds());}
 Serial.print("PRINTER ready=");Serial.print(printerStatus.ready());Serial.print(" state=");Serial.print(int(printerStatus.current().state));Serial.print(" progress=");Serial.print(printerStatus.current().progress);Serial.print(" remaining_s=");Serial.print(printerStatus.current().remaining);Serial.print(" finish_unix=");Serial.print(printerStatus.current().finish);Serial.print(" age_s=");Serial.print(printerStatus.age());Serial.print(" stale=");Serial.println(printerStatus.stale());
 Serial.print("CODEX windows=");Serial.print(codexUsage.count());Serial.print(" selected=");Serial.print(codexUsage.selected());Serial.print(" age_s=");Serial.print(codexUsage.age());Serial.print(" stale=");Serial.print(codexUsage.stale(unixClock()));
 if(codexUsage.count()){Serial.print(" remaining_tenths=");Serial.print(codexUsage.current().remainingTenths);Serial.print(" label=");Serial.print(codexUsage.current().label);Serial.print(" reset_unix=");Serial.print(codexUsage.current().reset);}Serial.println();
 Serial.print("SUN rows=");Serial.print(sunRowCount);Serial.print(" ready=");Serial.print(sunReady);Serial.print(" imageEpoch=");Serial.print(sunReady?sunMeta.imageEpoch:sunSnapshotMeta.imageEpoch);Serial.print(" info=");Serial.println(infoTarget);
 if(mode==6){
#if defined(ARDUINO_ARCH_ESP32)
  if(sunReady&&mediaOwner==6){Serial.print("SUN STORAGE CRC16 ");Serial.println(rowCRC(reinterpret_cast<const uint8_t*>(moonImage.getBuffer()),240*240*2),HEX);}
#endif
  SunInformation::Panel panel(sunReady&&!sunReceiving?sunMeta:sunSnapshotMeta,clockSeconds);
  for(int i=0;i<panel.count;i++){Serial.print("SUN INFO ");Serial.println(panel.rows[i].text);}
 }
 if(mode==4){Serial.print("MOON ");Serial.print(Celestial::phaseName(moonFacts.phase));Serial.print(" lit=");Serial.print(moonFacts.lit*100,2);Serial.print(" next=");Serial.println(moonFacts.name);}
}

#if !defined(ARDUINO_ARCH_ESP32)
__attribute__((always_inline)) inline void lcdPixelBit(uint16_t word,uint16_t mask){
 if(word&mask)NRF_P1->OUTSET=1UL<<6;else NRF_P1->OUTCLR=1UL<<6;
 NRF_P1->OUTSET=1UL<<4;NRF_P1->OUTCLR=1UL<<4;
}
__attribute__((always_inline)) inline void lcdPixel(uint16_t word){
 // Constant masks eliminate the shift and loop branch for every SPI bit.
 lcdPixelBit(word,0x8000);lcdPixelBit(word,0x4000);lcdPixelBit(word,0x2000);lcdPixelBit(word,0x1000);
 lcdPixelBit(word,0x0800);lcdPixelBit(word,0x0400);lcdPixelBit(word,0x0200);lcdPixelBit(word,0x0100);
 lcdPixelBit(word,0x0080);lcdPixelBit(word,0x0040);lcdPixelBit(word,0x0020);lcdPixelBit(word,0x0010);
 lcdPixelBit(word,0x0008);lcdPixelBit(word,0x0004);lcdPixelBit(word,0x0002);lcdPixelBit(word,0x0001);
}
__attribute__((always_inline)) inline void lcdByte(uint8_t word){
 lcdPixelBit(word,0x80);lcdPixelBit(word,0x40);lcdPixelBit(word,0x20);lcdPixelBit(word,0x10);
 lcdPixelBit(word,0x08);lcdPixelBit(word,0x04);lcdPixelBit(word,0x02);lcdPixelBit(word,0x01);
}
void lcdCommandFast(uint8_t command){
 NRF_P1->OUTCLR=1UL<<29;lcdByte(command);NRF_P1->OUTSET=1UL<<29;
}
void lcdWindowFast(int left,int top,int right,int bottom){
 // Same CASET/RASET/RAMWR sequence as Adafruit_GC9A01A::setAddrWindow,
 // using the already verified GPIO SPI path for its eleven command/data bytes.
 lcdCommandFast(GC9A01A_CASET);lcdPixel(left);lcdPixel(right);
 lcdCommandFast(GC9A01A_RASET);lcdPixel(top);lcdPixel(bottom);
 lcdCommandFast(GC9A01A_RAMWR);
}
void drawRegion(int left,int top,int right,int bottom){
 lcd.startWrite();lcdWindowFast(left,top,right,bottom);
 // The verified software SPI pin mapping, with direct GPIO writes for pixels.
 // P1.06=MOSI and P1.04=SCK. Keep clock low between bits (SPI mode 0).
 const uint16_t* pixels=cv.getBuffer();int drawingMode=mode;uint8_t drawingRotation=screenOrientation.turns();
 for(int yy=top;yy<=bottom;yy++){
 for(int xx=left;xx<=right;xx++){
  int i=yy*240+xx;uint16_t word=pixels[i];
  lcdPixel(word);
 }
 if((yy-top+1)%8==0&&yy<bottom){
  lcd.endWrite();accelerationFresh=acceleration();input();
  if(mode!=drawingMode||screenOrientation.turns()!=drawingRotation)return;
  lcd.startWrite();lcdWindowFast(left,yy+1,right,bottom);
 }
 }
 lcd.endWrite();
}
#else
void drawRegion(int left,int top,int right,int bottom){
 int drawingMode=mode;uint8_t drawingRotation=screenOrientation.turns();lcd.startWrite();lcd.setAddrWindow(left,top,right-left+1,bottom-top+1);
 for(int yy=top;yy<=bottom;yy++){
  lcd.writePixels(cv.getBuffer()+yy*240+left,right-left+1,true,false);
  if((yy-top+1)%8==0&&yy<bottom){lcd.endWrite();input();if(mode!=drawingMode||screenOrientation.turns()!=drawingRotation)return;lcd.startWrite();lcd.setAddrWindow(left,yy+1,right-left+1,bottom-yy);}
 }
 lcd.endWrite();
}
#endif
uint32_t lastRecovery=0,recoveryStarted=0;
bool waitingForWake=false;
int recoveryRow=240;
void serviceDisplayRecovery(){
 if(displaySleeping)return;
 uint32_t now=millis();
 if(!waitingForWake&&recoveryRow>=240&&now-lastRecovery>=3000){
  lcd.restoreConfiguration();lcd.setRotation(screenOrientation.turns());
  recoveryStarted=millis();lastRecovery=recoveryStarted;waitingForWake=true;
 }
 if(waitingForWake&&millis()-recoveryStarted>=150){
  lcd.sendCommand(GC9A01A_DISPON);waitingForWake=false;recoveryRow=0;
  Serial.println("Display recovery: repainting");
 }
}
void serviceUsbPower(bool force){
 if(!force&&uint32_t(millis()-powerPollAt)<1000)return;
 powerPollAt=millis();uint8_t status=0;
#if defined(ARDUINO_ARCH_ESP32)
 // USB host attachment is observable with the hardware CDC build. A charger/battery cannot be
 // distinguished without an extra sense circuit, so unknown power fails awake.
 usbPowerReadOK=HWCDC::isConnected();if(usbPowerReadOK)status=1;
#else
 usbPowerReadOK=npm1300_vbus_status(&status);
#endif
 if(usbPowerReadOK)usbPowerStatus=status;
 usbPower.observe(usbPowerReadOK,(status&1)!=0,millis());
}
void printDisplaySchedule(){
 Serial.print("USB POWER ");Serial.print(usbPower.connected(millis())?"PRESENT":"ABSENT/UNKNOWN");
 Serial.print(" status=");Serial.print(usbPowerStatus);Serial.print(" read_ok=");Serial.println(usbPowerReadOK);
 Serial.print("DISPLAY SCHEDULE ");Serial.println(displaySleeping?"ASLEEP / USB 01:00-07:00 configured time":"AWAKE / USB 01:00-07:00 configured time");
 Serial.println("WATCH ROTATION hourly at :30 while clock visible");
 Serial.print(sceneControl.current().carousel?"SCENE ROTATION ON interval_s=":"SCENE ROTATION HOLD interval_s=");Serial.print(sceneRotation.interval()/1000);Serial.print(" remaining_s=");Serial.println((sceneRotation.remaining(millis())+999)/1000);
}
void serviceDisplaySchedule(){
 const bool sleep=sceneControl.displayOff(clockSeconds,usbPower.connected(millis()));
 if(sleep!=displaySleeping){
  displaySleeping=sleep;
  touchContact=TouchContact();touchNavigation.sceneChanged();brightnessGesture.reset();
  backlight.off(millis());appliedPWM=0;analogWrite(PIN_D6,0);
  if(sleep){
   lcd.sendCommand(GC9A01A_DISPOFF);lcd.sendCommand(GC9A01A_SLPIN);
   if(mode==7)starPlayback.leave(millis());
   if(mode==8)life.leave();
   waitingForWake=false;recoveryRow=240;
   Serial.println("DISPLAY scheduled sleep");
  }else{
   lcd.restoreConfiguration();lcd.setRotation(screenOrientation.turns());delay(150);lcd.sendCommand(GC9A01A_DISPON);
   waitingForWake=false;recoveryRow=240;lastRecovery=millis();firstFrame=true;mediaOwner=-1;scheduledWakeRepaint=true;
   if(mode==2)watches.enter(millis());
   if(mode==7){starPlayback.enter(millis());starRenderer.resetTrails();}
   if(mode==8)life.enter(millis(),clockSeconds^micros());
   Serial.println("DISPLAY scheduled wake");
  }
 }
 if(halfPastRotation.observe(clockSeconds,mode==2&&!displaySleeping&&!watches.demo&&!touchCalibration.active())){
  watches.next(millis(),true);watchSelectionChanged();Serial.println("WATCH HALF PAST: next face");
 }
 if(sceneRotation.observe(millis(),sceneControl.current().carousel&&sceneControl.current().mask!=0&&!displaySleeping&&!touchCalibration.active())){
  switchMode();Serial.println("SCENE AUTO: next scene / 15 minutes");
 }
}
void simulateDisplayReset(){
 lcd.sendCommand(GC9A01A_SWRESET);
 lastRecovery=millis();waitingForWake=false;recoveryRow=240;
 Serial.println("Display-only reset: waiting for automatic recovery");
}
void setup(){
 pinMode(PIN_D6,OUTPUT);digitalWrite(PIN_D6,LOW);
 Board::setupRadioAntenna();Serial.begin(115200);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
 if(!psramFound()||!cv.begin(true)||!moonImage.begin(true)||!sunStore.begin()){
  while(true){Serial.println("ERROR S3 framebuffer/PSRAM allocation: enable OPI PSRAM");delay(1000);}
 }
#endif
 bool bluetoothStarted=Serial.beginBluetooth();
#if defined(ARDUINO_ARCH_ESP32)
 Serial.beginWifi();
#endif
 pinMode(PIN_BUTTON,INPUT_PULLUP);pinMode(PIN_D7,INPUT_PULLUP);
#if !defined(ARDUINO_ARCH_ESP32)
 pinMode(PIN_IMU_CS,OUTPUT);digitalWrite(PIN_IMU_CS,HIGH);delay(10);
 npm1300_begin();bool rail=npm1300_imu_mic_power_enable(true);delay(50);
 Wire1.begin();Wire1.setClock(400000);uint8_t who=0;
 imuOK=rail&&readRegs(Wire1,0x6a,15,&who,1)&&who==0x6a;
 if(imuOK){writeReg(Wire1,0x6a,0x12,0x44);writeReg(Wire1,0x6a,0x10,0x40);}
#else
 touchCoordinates.orientation=Board::loadTouch();
#endif
 screenOrientation.set(Board::loadRotation());
 Board::beginDisplayBus();delay(20);
 attachInterrupt(digitalPinToInterrupt(PIN_D7),touchReportReady,FALLING);

 clockSeconds=clockEpoch(buildClock);clockTickMs=millis();clockReady=true;
 clockPollMs=millis()-1000;serviceClock();printClock();
 serviceUsbPower(true);displaySleeping=sceneControl.displayOff(clockSeconds,usbPower.connected(millis()));
 // Keep the panel dark until its first complete frame is ready.
 scheduledWakeRepaint=!displaySleeping;
#if defined(ARDUINO_ARCH_ESP32)
 SPI.begin(D8,D9,D10,D1);
#endif
 pinMode(PIN_D6,OUTPUT);Board::pwmBegin();serviceLighting();lcd.begin(16000000);lcd.setRotation(screenOrientation.turns());
 if(displaySleeping){lcd.sendCommand(GC9A01A_DISPOFF);lcd.sendCommand(GC9A01A_SLPIN);}
 if(!cv.getBuffer()||!moonImage.getBuffer()||!textMask.getBuffer()
#if !defined(ARDUINO_ARCH_ESP32)
 ||!sunImage.getBuffer()
#endif
 ){Serial.println("ERROR framebuffer allocation");while(1)delay(1000);}
 watches.enter(millis());sceneRotation.reset(millis());Serial.print("Playground ready. IMU=");Serial.print(imuOK);Serial.print(" RTC=");Serial.println(rtcOK);
 Serial.print("FIRMWARE ");Serial.println(firmwareTag);
 Serial.println(bluetoothStarted?"BLUETOOTH ready":"BLUETOOTH unavailable");
}
void loop(){Serial.serviceBluetooth();
 static bool wasPairing=false;bool pairing=Serial.pairingVisible();
 if(pairing!=wasPairing){wasPairing=pairing;firstFrame=true;}
 printerStatus.tick(millis());codexUsage.tick(millis());serviceClock();serviceUsbPower(false);serviceDisplaySchedule();serviceLighting();serviceDisplayRecovery();accelerationFresh=acceleration();input();
 if(sunReceiving&&millis()-sunPacketAt>5000){sunReceiving=false;sunReady=false;
#if defined(ARDUINO_ARCH_ESP32)
 sunStore.cancel();
#endif
 Serial.println("ERR solar transfer timeout");}
 // Give service discovery/pairing the radio time it needs. Keep input and
 // the running clock serviced, and draw a changed pairing card once.
 if(Serial.authenticating()&&!firstFrame){delay(1);return;}
 if(displaySleeping){delay(2);return;}
 if(millis()-lastFrame<Board::FrameIntervalMs)return;lastFrame=millis();uint32_t frameStart=micros();int frameMode=mode;
 advanceInfoFade();if(mode!=2&&mode!=7&&mode!=8)cv.fillScreen(BG);

 if(Serial.pairingVisible()){
  cv.fillScreen(0);text(120,70,"PAIR BLUETOOTH",1,GOLD);
  text(120,100,"ENTER THIS CODE ON MAC",1);text(120,135,Serial.pairingPIN(),3);
  text(120,175,"XIAO ROUND DISPLAY",1);
 }
 else if(touchCalibration.active())renderTouchCalibration();
 else switch(mode){case 2:clockFace();break;case 3:analemma();break;case 4:moon();break;case 5:solarSystem();break;case 6:sun();break;case 7:starScene();break;case 8:lifeScene();break;case 9:codexScene();break;case 10:printerScene();break;case 11:weatherScene();break;}
 if(!touchCalibration.active())brightnessOverlay();
 auto frameWatchStyle=watches.style;auto frameStarSystem=starPlayback.system;

 uint32_t drawStart=millis(),renderCost=micros()-frameStart;
 static uint32_t watchFrames=0,watchUs=0,watchRenderUs=0,watchMaxUs=0,watchPixels=0,watchWindows=0,watchSince=0;
 static int metricsMode=-1;
 if(metricsMode!=mode){metricsMode=mode;watchSince=0;watchFrames=watchUs=watchRenderUs=watchMaxUs=watchPixels=watchWindows=0;}
 bool forceFrame=firstFrame;firstFrame=false;
 int rectangles=0;
 bool complete=frameTiles.flush(cv.getBuffer(),forceFrame,[&](int l,int t,int r,int b){
  watchPixels+=(r-l+1)*(b-t+1);watchWindows++;drawRegion(l,t,r,b);
  if(++rectangles%4==0){accelerationFresh=acceleration();input();}
  return mode==frameMode&&!firstFrame;
 });
 if(!complete)firstFrame=true;
 if(recoveryRow<240){
  int rows=(mode==2||mode==7||mode==8)?4:8;drawRegion(0,recoveryRow,239,min(recoveryRow+rows-1,239));recoveryRow+=rows;
 }
 if(mode==2&&frameMode==2&&!firstFrame&&watches.style==frameWatchStyle)watches.title.didPresent(millis());
 if(mode==7&&frameMode==7&&!firstFrame&&starPlayback.system==frameStarSystem)starPlayback.didPresent(millis());
 if(scheduledWakeRepaint&&!firstFrame)scheduledWakeRepaint=false;

 if((mode==2||mode==7||mode==8)&&mode==frameMode){
  uint32_t duration=micros()-frameStart;if(!watchSince)watchSince=millis();
  watchFrames++;watchUs+=duration;watchRenderUs+=renderCost;watchMaxUs=max(watchMaxUs,duration);
  if(millis()-watchSince>=5000){
   Serial.print(mode==8?"LIFE fps=":mode==7?"STAR fps=":"WATCH fps=");Serial.print(watchFrames*1000.0f/(millis()-watchSince),1);
   Serial.print(" render+send avg_ms=");Serial.print(watchUs/(1000.0f*watchFrames),1);
   Serial.print(" render_ms=");Serial.print(watchRenderUs/(1000.0f*watchFrames),1);
   Serial.print(" max_ms=");Serial.print(watchMaxUs/1000.0f,1);
   Serial.print(" pixels/frame=");Serial.print(watchPixels/watchFrames);
   Serial.print(" windows/frame=");Serial.println(watchWindows/watchFrames);
   watchSince=millis();watchFrames=watchUs=watchRenderUs=watchMaxUs=watchPixels=watchWindows=0;
  }
 }else{watchSince=0;watchFrames=watchUs=watchRenderUs=watchMaxUs=watchPixels=watchWindows=0;}

 static uint32_t report=0;if(millis()-report>2000){report=millis();
 Serial.print("FRAME ms=");Serial.print(millis()-drawStart);Serial.print(" accel=");Serial.print(ax,3);Serial.print(",");Serial.print(ay,3);Serial.print(",");Serial.println(az,3);}

}
