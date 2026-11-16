// Headless preview of the production renderers. No device/account/network access.
#include <Adafruit_GFX.h>
#include "../XiaoPlayground/WatchFace.h"
#include "../XiaoPlayground/Celestial.h"
#include "../XiaoPlayground/SeasonalAnalemma.h"
#include "../XiaoPlayground/MoonPhase.h"
#include "../XiaoPlayground/MoonTexture.h"
#include "../XiaoPlayground/star_systems/Renderer.h"
#include "../XiaoPlayground/GameOfLife.h"
#include "../XiaoPlayground/CodexUsageRenderer.h"
#include "../XiaoPlayground/PrinterRenderer.h"
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
using std::min;using std::max;
template<class T>T constrain(T n,T lo,T hi){return min(hi,max(lo,n));}
GFXcanvas16 cv(240,240),moonImage(240,240);
const uint16_t BG=0x0843,WHITE=0xffff,GOLD=0xfd20;
ClockDate demoDate={2026,11,16,12,0,0};
ClockDate currentClock(){return demoDate;}
bool clockReady=true,compressedOrbits=true;
int previewMonth=0,previewDay=0,mediaOwner=-1,infoOpacity=0;
Celestial::MoonDetails moonFacts={};uint32_t moonFactsEpoch=0;
struct QuietSerial{template<class T>void print(T){}template<class T>void print(T,int){}template<class T>void println(T){}template<class T>void println(T,int){};} Serial;
void text(int cx,int cy,const char* s,int size=2,uint16_t col=WHITE){cv.setTextSize(size);cv.setTextColor(col);cv.setCursor(cx-strlen(s)*3*size,cy);cv.print(s);}
void moonInformation(const ClockDate&){} // The showcase uses the unoverlaid Moon scene.
#include "showcase_scenes.inc"
static std::string crc(std::string body){uint16_t c=0xffff;for(unsigned char v:body){c^=uint16_t(v)<<8;for(int i=0;i<8;i++)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}char tail[8];snprintf(tail,sizeof(tail),"|%04x",c);return body+tail;}
static std::string printerPacket(int progress,uint32_t now){
 std::string body="M|"+std::to_string(now)+"|2|"+std::to_string(progress)+"|2700|"+std::to_string(now+2700)+"|";
 const char* hex="0123456789abcdef";for(unsigned char c:std::string("Sample enclosure")){body+=hex[c>>4];body+=hex[c&15];}return crc(body);
}
int main(int argc,char** argv){
 assert(argc==2);std::string root=argv[1];FILE* manifest=fopen((root+"/frames.tsv").c_str(),"w");assert(manifest);
 int index=0,chapter=0;std::vector<uint16_t> background(57600),frame(57600);
 auto save=[&](const char* group,const char* title,const char* detail){
  char filename[40];snprintf(filename,sizeof(filename),"frame_%04d.ppm",index++);
  FILE* f=fopen((root+"/"+filename).c_str(),"wb");assert(f);fputs("P6\n240 240\n255\n",f);
  for(uint16_t c:frame){unsigned char p[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};fwrite(p,1,3,f);}fclose(f);
  fprintf(manifest,"%d\t%s\t%s\t%s\t%s\n",chapter,group,title,detail,filename);
 };
 const WatchFace::Style faces[]={WatchFace::Style::BigBen,WatchFace::Style::RoyalHolloway,WatchFace::Style::Aberdeen,WatchFace::Style::StMarks,WatchFace::Style::PragueAstronomical,WatchFace::Style::GrandCentral};
 for(auto face:faces){
  WatchFace::dial(background.data(),face,WatchAppearance::Light);auto& meta=WatchFace::definition(face);
  for(int i=0;i<12;i++){WatchFace::hands(frame.data(),background.data(),10*3600+8*60+i*.4,face,WatchAppearance::Light);save("LANDMARK CLOCKS",meta.name,meta.location);}chapter++;
 }
 WatchFace::dial(background.data(),WatchFace::Style::BigBen,WatchAppearance::Dark);
 for(int i=0;i<14;i++){WatchFace::hands(frame.data(),background.data(),10*3600+8*60+i*.4,WatchFace::Style::BigBen,WatchAppearance::Dark);save("DAY & NIGHT","A different mood","Automatic light and dark palettes");}chapter++;
 for(int i=0;i<24;i++){demoDate=clockFromEpoch(clockEpoch({2026,1,1,12,0,0})+i*15*86400);cv.fillScreen(BG);analemma();memcpy(frame.data(),cv.getBuffer(),115200);save("ASTRONOMY","The Analemma","A year of Sun positions / schematic");}chapter++;
 for(int i=0;i<26;i++){demoDate=clockFromEpoch(clockEpoch({2026,10,10,12,0,0})+i*86400);cv.fillScreen(BG);moon();memcpy(frame.data(),cv.getBuffer(),115200);save("ASTRONOMY","Moon phases","Textured lunar phases across a month");}chapter++;
 for(int i=0;i<24;i++){demoDate=clockFromEpoch(clockEpoch({2026,1,1,12,0,0})+i*15*86400);cv.fillScreen(BG);solarSystem();memcpy(frame.data(),cv.getBuffer(),115200);save("ASTRONOMY","The Solar System","Eight planets / compressed distances");}chapter++;
 StarSystems::Playback play;StarSystems::Renderer stars;play.enter(0);play.didPresent(0);stars.background(background.data(),play.system);
 for(int i=0;i<32;i++){uint32_t now=9000+i*250;play.tick(now);stars.render(frame.data(),background.data(),play,now);save("ORBITAL SYSTEMS","Six stars, one system","TYC 7037-89-1 / schematic, not to scale");}chapter++;
 GameOfLife life;life.enter(0,0x5a17u);
 for(int i=0;i<24;i++){life.advance((i+1)*150);life.render(frame.data());save("GENERATIVE PLAY","Conway's Game of Life","An evolving world beyond the glass");}chapter++;
 constexpr uint32_t now=1794823200; // Synthetic, fixed sample epoch; no account is read.
 for(int i=0;i<22;i++){CodexUsage usage;std::string body="U|"+std::to_string(now)+"|1|Weekly|680|10080|"+std::to_string(now+7200);assert(usage.accept(crc(body).c_str(),0,now));CodexUsageView::render(frame.data(),usage,now+i*60);save("LIVE-DATA SCENES","Codex usage","Remaining allowance and reset countdown");}chapter++;
 for(int i=0;i<24;i++){PrinterStatus printer;assert(printer.accept(printerPacket(36+i*2,now).c_str(),0,now));PrinterView::render(frame.data(),printer,now);save("LIVE-DATA SCENES","Bambu print monitor","Progress, finish time and print name");}chapter++;
 fclose(manifest);printf("Rendered %d synthetic preview frames from %d chapters\n",index,chapter);
}
