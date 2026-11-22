#pragma once
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "BrightnessGesture.h"
#include "Backlight.h"
#include "TouchContact.h"

Adafruit_GC9A01A lcd(D1, D3, -1);
class GymCanvas:public GFXcanvas16 {
public:
  GymCanvas():GFXcanvas16(240,240,false){}
  bool begin(){buffer=(uint16_t*)ps_calloc(240*240,sizeof(uint16_t));buffer_owned=true;return buffer!=nullptr;}
};
GymCanvas canvas;
uint16_t *previousFrame=nullptr;
uint8_t *edgeMask=nullptr;
uint8_t edgeStrength=0;
int glowBrightness=25,glowCentre=60,glowHighFloor=50;
uint16_t greenPalette[256][16];
int paletteStrength=-1;
float smoothGlow=0;
uint32_t glowAt=0;
uint16_t tile[240*16];
bool displayReady=false, firstFrame=true, touchOK=false;
uint32_t frames=0, frameAt=0, touchAt=0, touchReads=0, touchFailures=0;
uint32_t settingsAt=0, rideStart=0, lastUiPulse=0, uiInterval=0;
uint32_t milestoneAt=0, lastMilestone=0;
uint32_t fpsAt=0, fpsFrames=0;
float uiFps=0;
int resistance=1;
bool showRpm=false,imperialUnits=false;int ridePage=0;
RideStats demoRide;
char displayTitle[25]="BIKE LAB";
bool demoMode=false;
uint32_t demoStart=0,demoLastPulse=0,demoPulses=0,demoInterval=800000;
bool touchHeld=false, settingsDirty=false;
int touchX=-1,touchY=-1;
uint8_t touchMap=0;
BrightnessGesture brightnessGesture;
Backlight screenBacklight;
TouchContact screenTouch;
int screenBrightness=60;
uint32_t brightnessHintUntil=0,brightnessSaveAt=0;
bool brightnessDirty=false;
void setScreenBrightness(int level) {
  level=constrain(level,0,100);if(level==screenBrightness)return;
  screenBrightness=level;brightnessDirty=true;brightnessSaveAt=millis();
  brightnessHintUntil=millis()+1400;
}
void serviceScreenBrightness() {
  if(displayReady&&!firstFrame)analogWrite(D6,screenBacklight.update(screenBrightness,millis()));
  if(brightnessDirty&&millis()-brightnessSaveAt>1500) {
    Preferences p;if(p.begin("bike-ui",false)){p.putUChar("backlight",screenBrightness);p.end();brightnessDirty=false;}
  }
}
constexpr uint16_t UI_BG=0x0000, UI_MUTED=0x7c30, UI_CYAN=0x07fa;
constexpr uint16_t UI_GOLD=0xfe60, UI_WHITE=0xef9e, UI_GREEN=0x3eaa;

void uiText(const char *s,int x,int y,const GFXfont *font,uint16_t colour) {
  canvas.setFont(font); canvas.setTextSize(1);canvas.setTextColor(colour);
  int16_t bx,by;uint16_t w,h;canvas.getTextBounds(s,0,0,&bx,&by,&w,&h);
  canvas.setCursor(x-w/2-bx,y); canvas.print(s);
}
void uiSmall(const char*s,int x,int y,uint16_t c) {uiText(s,x,y,nullptr,c);}

void buildGlowMask() {
  if(!edgeMask)return;
  float centre=glowCentre/100.0f;
  for(int y=0;y<240;y++)for(int x=0;x<240;x++) {
    float dx=x-119.5f,dy=y-119.5f,r=sqrtf(dx*dx+dy*dy);
    float radius=constrain(r/118.0f,0.0f,1.0f);
    float outer=constrain((120.0f-r)/1.5f,0.0f,1.0f);
    float weight=r<=120?(centre+(1-centre)*radius*radius)*outer:0;
    edgeMask[y*240+x]=uint8_t(weight*255);
  }
  paletteStrength=-1;
}
void setGlow(int brightness,int centre,int floor) {
  glowBrightness=constrain(brightness,1,100);glowCentre=constrain(centre,5,100);
  glowHighFloor=constrain(floor,0,100);
  Preferences p;if(p.begin("bike-ui",false)){
    p.putUChar("glow-peak",glowBrightness);p.putUChar("glow-centre",glowCentre);
    p.putUChar("glow-floor",glowHighFloor);p.end();
  }
  buildGlowMask();record("GLOW_UPDATED",micros(),digitalRead(SENSOR_PIN),0);
}

float pulseRate() {
  if(demoMode)return 60000000.0f/demoInterval;
  if (!haveClosure || !uiInterval || uint32_t(millis()-lastUiPulse)>max(4000UL,uiInterval/1000*3)) return 0;
  return 60000000.0f/uiInterval;
}
void demoService() {
  if(!demoMode)return;
  uint32_t now=millis();
  float rate=75+7*sinf((now-demoStart)*0.00022f);
  demoInterval=uint32_t(60000000.0f/rate);
  if(now-demoLastPulse>=demoInterval/1000){demoLastPulse=now;++demoPulses;demoRide.pulse(esp_timer_get_time(),60000000.0/demoInterval,estimatedWatts(60000000.0/demoInterval,resistance),hostLink.metresPerRevolution());}
  demoRide.tick(esp_timer_get_time());
}
void setDemo(bool enabled) {
  demoMode=enabled;demoStart=demoLastPulse=millis();demoPulses=0;demoRide.reset();
  Preferences p;if(p.begin("bike-ui",false)){p.putBool("demo",enabled);p.end();}
  record(enabled?"DEMO_ON":"DEMO_OFF",micros(),digitalRead(SENSOR_PIN),0);
}
void uiPulse(uint32_t interval) {
  uint32_t now=millis();lastUiPulse=now;
  if(!rideStart) rideStart=now;
  uiInterval=interval;
  if(closures && closures%100==0 && lastMilestone!=closures) {
    lastMilestone=closures;milestoneAt=now;
  }
}
void setResistance(int value) {
  value=constrain(value,1,8);if(value==resistance)return;
  resistance=value;hostLink.setResistance(resistance); settingsDirty=true; settingsAt=millis();
  char event[24];snprintf(event,sizeof(event),"RESISTANCE_%d",resistance);
  record(event,micros(),digitalRead(SENSOR_PIN),0);
}
void setUnits(bool imperial){imperialUnits=imperial;Preferences p;if(p.begin("bike-ui",false)){p.putBool("imperial",imperial);p.end();}}
void rideStatus(){
  auto& ride=hostLink.ride();auto& d=ride.data();
  hostLink.printf("RIDE units=%s mm_per_rev=%lu provisional=1 distance_m=%.3f speed_kph=%.3f revolutions=%llu elapsed_s=%.1f moving_s=%.1f work_kj=%.3f avg_w=%.2f avg_rpm=%.2f avg_kph=%.2f overall_w=%.2f overall_kph=%.2f max_w=%.0f max_rpm=%.2f max_kph=%.2f wheel_mm=2100\n",
    imperialUnits?"imperial":"metric",(unsigned long)hostLink.distanceMillimetres(),d.metres,hostLink.realSpeedKph(),(unsigned long long)d.revolutions,d.elapsedUs/1e6,d.movingUs/1e6,d.joules/1000,ride.averageWatts(),ride.averageRpm(),ride.averageKph(),ride.overallWatts(),ride.overallKph(),d.maxWatts,d.maxRpm,d.maxKph);
}
void toggleMetric(){
  showRpm=!showRpm;Preferences p;
  if(p.begin("bike-ui",false)){p.putBool("show-rpm",showRpm);p.end();}
}
bool setDisplayTitleHex(const char* text){
  size_t n=strlen(text);if(n<2||n>48||n%2)return false;
  char title[25]={};
  auto hex=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
  for(size_t i=0;i<n/2;i++){
    int a=hex(text[i*2]),b=hex(text[i*2+1]);if(a<0||b<0)return false;
    int c=a*16+b;if(c<32||c>126)return false;title[i]=char(c);
  }
  bool nonspace=false;for(size_t i=0;i<n/2;i++)if(title[i]!=' ')nonspace=true;
  if(!nonspace)return false;
  Preferences p;if(!p.begin("bike-ui",false))return false;
  bool saved=p.putString("title",title)==n/2;p.end();
  if(saved)memcpy(displayTitle,title,sizeof(displayTitle));return saved;
}
void titleStatus(){
  hostLink.print("TITLE hex=");for(size_t i=0;displayTitle[i];i++)hostLink.printf("%02X",uint8_t(displayTitle[i]));hostLink.println();
}
void touchService() {
  if(!displayReady)return;
  uint32_t now=millis();if(now-touchAt<15)return;touchAt=now;
  // CHSC6X: direct five-byte report, no register-write prefix.
  bool good=Wire.requestFrom(uint8_t(0x2e),uint8_t(5))==5;
  bool down=false;int x=-1,y=-1;
  if(good) {
    uint8_t p[5];for(int i=0;i<5;i++)p[i]=Wire.read();
    touchOK=true;++touchReads;
    down=p[0]==1;x=p[2];y=p[4];
    if(touchMap&4){int t=x;x=y;y=t;}
    if(touchMap&1)x=239-x;if(touchMap&2)y=239-y;
    touchX=x;touchY=y;
  } else {++touchFailures;touchOK=false;}
  auto contact=screenTouch.update(good,down,x,y,now);
  auto brightness=brightnessGesture.update(contact.down,contact.x,contact.y,screenBrightness,now);
  if(brightness.consumed)brightnessHintUntil=now+1400;
  if(brightness.changed)setScreenBrightness(brightness.level);
  if(brightness.ended)Serial.printf("BRIGHTNESS level=%d\n",screenBrightness);
  if(!brightness.consumed&&!hostLink.pairingVisible()) {
    x=contact.x;y=contact.y;
    if(contact.down&&!touchHeld&&x>=0&&x<240&&y>=0&&y<240) {
      if(demoMode && y>=25 && y<=60 && x>=60 && x<=180){setDemo(false);}
      else if(y>=211&&x>=50&&x<=190){ridePage=(ridePage+1)%4;}
      else if(ridePage==0&&x>=40&&x<=200&&y>=70&&y<=125){toggleMetric();}
      else if((x-60)*(x-60)+(y-177)*(y-177)<=24*24)setResistance(resistance-1);
      else if((x-180)*(x-180)+(y-177)*(y-177)<=24*24)setResistance(resistance+1);
    }
    }
  touchHeld=contact.down;
  if(settingsDirty&&now-settingsAt>1500) {
    Preferences p;if(p.begin("bike-ui",false)){p.putUChar("resistance",resistance);p.end();settingsDirty=false;}
  }
}
void uiSetup() {
  canvas.begin();
  Preferences p;
  if(p.begin("bike-ui",true)){
    resistance=constrain(p.getUChar("resistance",1),1,8);
    showRpm=p.getBool("show-rpm",false);imperialUnits=p.getBool("imperial",false);
    hostLink.setDistanceMM(p.getUInt("distance-mm",5000));
    String title=p.getString("title",displayTitle);if(title.length()>=1&&title.length()<=24)title.toCharArray(displayTitle,sizeof(displayTitle));
    glowBrightness=constrain(p.getUChar("glow-peak",25),1,100);
    glowCentre=constrain(p.getUChar("glow-centre",60),5,100);
    glowHighFloor=constrain(p.getUChar("glow-floor",50),0,100);p.end();
  }
  if(p.begin("bike-ui",true)){screenBrightness=constrain(p.getUChar("backlight",60),0,100);p.end();}
  bool demoOnce=false;
  if(p.begin("bike-ui",false)){demoOnce=p.getBool("demo",false);p.end();}
  if(demoOnce)setDemo(true);
  if(p.begin("xiao-display",true)){touchMap=p.getUChar("touch-map",0);p.end();}
  previousFrame=(uint16_t*)ps_malloc(240*240*2);
  edgeMask=(uint8_t*)ps_malloc(240*240);
  if(!canvas.getBuffer()||!previousFrame||!edgeMask)return;
  // A quiet full-face radial wash: strongest at the edge, softly reaching the
  // centre. Brightness is limited separately to 25% of the initial green rim.
  buildGlowMask();
  Wire.setPins(D4,D5);Wire.setTimeOut(8);Wire.begin(D4,D5,100000);
  pinMode(D7,INPUT_PULLUP);
  // D2 remains the sensor input. Never initialize SD or its chip-select.
  SPI.begin(D8,D9,D10,D1);
  pinMode(D6,OUTPUT);analogWriteResolution(D6,12);analogWriteFrequency(D6,2000);
  analogWrite(D6,0);
  screenBacklight.off(millis());
  // 40MHz is this driver's standard ESP32 SPI setting.
  lcd.begin(40000000);lcd.setRotation(0);lcd.fillScreen(UI_BG);
  displayReady=true;fpsAt=millis();
}
void sendChangedStripes() {
  auto buf=canvas.getBuffer();
  for(int y=0;y<240;y+=16) {
    bool dirty[15]={};
    for(int block=0;block<15;block++) {
      dirty[block]=firstFrame;
      for(int row=y;row<y+16&&!dirty[block];row++)
        dirty[block]=memcmp(buf+row*240+block*16,previousFrame+row*240+block*16,32)!=0;
    }
    // Send separate edge tiles so a pulsing rim doesn't retransmit the black
    // centre of every row. Adjacent changed tiles form a single SPI transfer.
    for(int block=0;block<15;) {
      if(!dirty[block]){++block;continue;}
      int left=block*16;while(block<15&&dirty[block])++block;
      int width=block*16-left;
      for(int row=0;row<16;row++)memcpy(tile+row*width,buf+(y+row)*240+left,width*2);
      lcd.startWrite();lcd.setAddrWindow(left,y,width,16);
      lcd.writePixels(tile,width*16);lcd.endWrite();
      for(int row=0;row<16;row++)memcpy(previousFrame+(y+row)*240+left,buf+(y+row)*240+left,width*2);
    }
  }
  if(firstFrame)firstFrame=false;
}
void uiDraw() {
  uint32_t now=millis();if(!displayReady||now-frameAt<33)return;frameAt=now;
  canvas.fillScreen(UI_BG);canvas.setTextWrap(false);
  float rate=pulseRate();
  uint32_t visiblePulses=demoMode?demoPulses:closures;
  uint32_t visibleLastPulse=demoMode?demoLastPulse:lastUiPulse;
  bool healthy=storageOK&&!logFull&&!queueDrops&&sensorTaskOK;
  uint32_t age=now-visibleLastPulse;
  uint32_t period=demoMode?demoInterval:uiInterval;
  float periodMs=period?period/1000.0f:1000.0f;
  float cadence=60000.0f/periodMs;
  // At 60 pulses/min or below, fade to black between beats. By 90/min,
  // retain a 50% floor between beats. Both use the same quiet 25% peak cap.
  float floor=glowHighFloor/100.0f*constrain((cadence-60)/30.0f,0.0f,1.0f);
  float target=0;
  if(demoMode||haveClosure) {
    float phase=constrain(age/(periodMs*0.85f),0.0f,1.0f);
    float beat=0.5f+0.5f*cosf(PI*phase);
    float fade=constrain((age-periodMs*1.15f)/constrain(periodMs*0.8f,250.0f,900.0f),0.0f,1.0f);
    fade=fade*fade*(3-2*fade);
    target=(floor+(1-floor)*beat)*(1-fade);
  }
  float dt=glowAt?uint32_t(now-glowAt):33;glowAt=now;
  smoothGlow+=(target-smoothGlow)*(1-expf(-dt/65.0f));
  if(target==0 && smoothGlow<0.005f)smoothGlow=0;
  edgeStrength=uint8_t(smoothGlow*255*glowBrightness/100.0f);
  if(paletteStrength!=edgeStrength) {
    for(int i=0;i<256;i++)for(int d=0;d<16;d++) {
      // Ordered spatial dithering avoids a hard black band at low RGB565 levels.
      uint32_t v=i*edgeStrength*256/255;
      int r=min(31,int((v*20/255+d*128)/2048));
      int g=min(63,int((v+d*64)/1024));
      int b=min(31,int((v*75/255+d*128)/2048));
      greenPalette[i][d]=(r<<11)|(g<<5)|b;
    }
    paletteStrength=edgeStrength;
  }
  if(edgeStrength) {
    static const uint8_t bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    auto pixels=canvas.getBuffer();
    for(int y=0;y<240;y++)for(int x=0;x<240;x++) {
      int i=y*240+x;if(edgeMask[i])pixels[i]=greenPalette[edgeMask[i]][bayer[(y%4)*4+x%4]];
    }
  }
  uiSmall(displayTitle,120,26,UI_MUTED);
  canvas.fillCircle(87,46,3,demoMode?UI_GOLD:(healthy?UI_GREEN:0xf800));
  uiSmall(demoMode?"DEMO DATA":(healthy?"RECORDING":"LOG ERROR"),126,43,demoMode?UI_GOLD:(healthy?UI_GREEN:0xf800));
  float rpm=demoMode?rate/hostLink.pulsesPerRevolution():hostLink.realCadence();
  int watts=demoMode?estimatedWatts(rpm,resistance):hostLink.realPower();
  auto& ride=demoMode?demoRide:hostLink.ride();auto& totals=ride.data();
  double speed=rpm*hostLink.metresPerRevolution()*0.06;
  double speedUnit=imperialUnits?1/1.609344:1,distanceUnit=imperialUnits?1/1609.344:1/1000.0;
  const char* speedLabel=imperialUnits?"MPH":"KM/H";const char* distanceLabel=imperialUnits?"MI":"KM";
  char number[32];
  if(ridePage==0){
    char secondary[32];if(showRpm)snprintf(secondary,sizeof(secondary),"%d EST W",watts);else snprintf(secondary,sizeof(secondary),"%.0f RPM",rpm);
    uiSmall(secondary,120,64,UI_CYAN);
    if(showRpm)snprintf(number,sizeof(number),"%.0f",rpm);else snprintf(number,sizeof(number),"%d",watts);
    uiText(number,120,111,&FreeSansBold24pt7b,UI_WHITE);
    uiSmall(showRpm?"RPM":"ESTIMATED WATTS",120,118,UI_WHITE);
    snprintf(number,sizeof(number),"%.1f %s   %.3f %s",speed*speedUnit,speedLabel,totals.metres*distanceUnit,distanceLabel);
    uiSmall(number,120,133,UI_CYAN);
  }else{
    auto row=[&](int y,const char* label,const char* value){uiSmall(label,73,y,UI_MUTED);uiSmall(value,166,y,UI_WHITE);};
    auto duration=[&](double seconds){snprintf(number,sizeof(number),"%02lu:%02lu:%02lu",(unsigned long)seconds/3600,(unsigned long)seconds/60%60,(unsigned long)seconds%60);};
    if(ridePage==1){
      duration(totals.elapsedUs/1e6);row(61,"ELAPSED",number);
      duration(totals.movingUs/1e6);row(76,"MOVING",number);
      duration(totals.elapsedUs>totals.movingUs?(totals.elapsedUs-totals.movingUs)/1e6:0);row(91,"PAUSED",number);
      snprintf(number,sizeof(number),"%.3f %s",totals.metres*distanceUnit,distanceLabel);row(106,"EST DIST",number);
      snprintf(number,sizeof(number),"%.2f KJ",totals.joules/1000);row(121,"EST WORK",number);
      snprintf(number,sizeof(number),"%llu",(unsigned long long)totals.revolutions);row(136,"TURNS",number);
    }else if(ridePage==2){
      snprintf(number,sizeof(number),"%.0f W",ride.averageWatts());row(61,"MOV AVG W",number);
      snprintf(number,sizeof(number),"%.0f RPM",ride.averageRpm());row(76,"AVG RPM",number);
      snprintf(number,sizeof(number),"%.1f %s",ride.averageKph()*speedUnit,speedLabel);row(91,"MOV SPEED",number);
      snprintf(number,sizeof(number),"%.0f W",ride.overallWatts());row(106,"RIDE AVG W",number);
      snprintf(number,sizeof(number),"%.1f %s",ride.overallKph()*speedUnit,speedLabel);row(121,"RIDE SPEED",number);
      snprintf(number,sizeof(number),"%.2f M/TURN",hostLink.metresPerRevolution());row(136,"EST DIST",number);
    }else{
      snprintf(number,sizeof(number),"%.0f W",totals.maxWatts);row(65,"PEAK W",number);
      snprintf(number,sizeof(number),"%.0f RPM",totals.maxRpm);row(84,"PEAK RPM",number);
      snprintf(number,sizeof(number),"%.1f %s",totals.maxKph*speedUnit,speedLabel);row(103,"PEAK SPEED",number);
      snprintf(number,sizeof(number),"%.2f M",hostLink.metresPerRevolution());row(122,"M / TURN",number);
      row(136,"WATCH WHEEL","2100 MM");
    }
  }
  canvas.drawFastHLine(51,142,138,0x21a6);
  uiSmall("RESISTANCE",120,151,UI_GOLD);
  for(int x:{60,180}) {
    bool disabled=(x==60&&resistance==1)||(x==180&&resistance==8);
    canvas.fillCircle(x,177,21,0x1146);canvas.drawCircle(x,177,21,disabled?0x3227:UI_CYAN);
    canvas.fillRoundRect(x-8,176,17,3,1,disabled?UI_MUTED:UI_WHITE);
    if(x==180)canvas.fillRoundRect(x-1,169,3,17,1,disabled?UI_MUTED:UI_WHITE);
  }
  snprintf(number,sizeof(number),"%d",resistance);
  uiText(number,120,187,&FreeSansBold18pt7b,UI_GOLD);
  uiSmall("SPEED / DIST / POWER EST",120,207,UI_MUTED);
  const char* pages[]={"LIVE  >","RIDE  >","AVERAGES  >","PEAKS  >"};uiSmall(pages[ridePage],120,221,UI_CYAN);
  if(ridePage==0&&!demoMode&&milestoneAt&&now-milestoneAt<1400) {
    canvas.fillRoundRect(29,76,182,55,12,0x1146);
    snprintf(number,sizeof(number),"%lu PULSES!",(unsigned long)lastMilestone);
    uiText(number,120,109,&FreeSansBold9pt7b,UI_CYAN);
  }
  if(brightnessHintUntil&&int32_t(brightnessHintUntil-now)>0) {
    canvas.fillRoundRect(43,168,154,34,7,0x08a3);
    char hint[24];snprintf(hint,sizeof(hint),"BRIGHTNESS %d%%",screenBrightness);
    uiSmall(hint,120,180,UI_WHITE);
    canvas.drawFastHLine(62,192,116,0x21a6);
    canvas.drawFastHLine(62,192,116*screenBrightness/100,UI_WHITE);
  }
  if(hostLink.pairingVisible()) {
    canvas.fillCircle(120,120,119,0x0822);
    uiSmall("BLUETOOTH PAIRING",120,68,UI_CYAN);
    uiSmall("ENTER CODE ON YOUR MAC",120,91,UI_WHITE);
    uiText(hostLink.pairingPIN(),120,140,&FreeSansBold24pt7b,UI_WHITE);
    uiSmall("PEDAL LOGGING CONTINUES",120,169,UI_MUTED);
  }
  sendChangedStripes();++frames;
  if(now-fpsAt>=1000){uiFps=(frames-fpsFrames)*1000.0f/(now-fpsAt);fpsFrames=frames;fpsAt=now;}
}
