#pragma once
#include <stdint.h>
#include <string.h>
class SceneControl {
public:
 static constexpr int Count=9,First=2;
 static constexpr uint32_t All=(1u<<Count)-1;
 struct Settings {uint32_t revision=0,mask=All,seconds=900;bool carousel=true;uint8_t day=0,night=0,appearance=0,power=0;int scene=-1;uint32_t selection=0;bool brightness=false;uint32_t targetBoot=0;};
private:
 Settings value_;uint32_t selected_=0,session_=0;
 static bool number(const char* s,uint32_t& n){if(!s||!*s)return false;uint64_t v=0;for(;*s;s++){if(*s<'0'||*s>'9')return false;v=v*10+*s-'0';if(v>0xffffffffu)return false;}n=v;return true;}
public:
 void setSession(uint32_t value){session_=value?value:1;}
 uint32_t session()const{return session_;}
 const Settings& current()const{return value_;}
 int next(int scene)const{for(int i=1;i<=Count;i++){int candidate=First+(scene-First+i)%Count;if(value_.mask&(1u<<(candidate-First)))return candidate;}return scene;}
 // C|1|revision|mask|carousel|seconds|day|night|appearance|power|scene|selectionToken|applyBrightness|targetBoot|CRC
 bool parse(const char* packet,Settings& out)const{
  if(!packet||strncmp(packet,"C|",2)||strlen(packet)>=240)return false;
  const char* tail=strrchr(packet,'|');if(!tail||strlen(tail+1)!=4)return false;
  uint16_t crc=0xffff,sent=0;for(auto p=packet;p<tail;p++){crc^=uint16_t(uint8_t(*p))<<8;for(int b=0;b<8;b++)crc=(crc&0x8000)?uint16_t((crc<<1)^0x1021):uint16_t(crc<<1);}
  for(int i=1;i<=4;i++){char c=tail[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(n<0)return false;sent=(sent<<4)|n;}if(crc!=sent)return false;
  char text[240];size_t len=tail-packet;memcpy(text,packet,len);text[len]=0;char* fields[14];int count=0;char* begin=text;
  for(char* p=text;;p++)if(*p=='|'||!*p){bool end=!*p;if(count>=14)return false;fields[count++]=begin;if(end)break;*p=0;begin=p+1;}
  if(count!=14)return false;uint32_t n[13];
  for(int i=1;i<14;i++){if(i==10&&!strcmp(fields[i],"-1")){n[i-1]=0xffffffffu;continue;}if(!number(fields[i],n[i-1]))return false;}
  if(n[0]!=1||!n[1]||(n[2]&~All)||n[3]>1||n[4]<60||n[4]>3600||n[5]>100||n[6]>100||n[7]>2||n[8]>2||n[11]>1)return false;
  if(n[9]!=0xffffffffu&&(n[9]<First||n[9]>=First+Count))return false;
  out.revision=n[1];out.mask=n[2];out.carousel=n[3];out.seconds=n[4];out.day=n[5];out.night=n[6];out.appearance=n[7];out.power=n[8];out.scene=n[9]==0xffffffffu?-1:int(n[9]);out.selection=n[10];out.brightness=n[11];out.targetBoot=n[12];return true;
 }
 bool apply(const Settings& value){if(value.revision==value_.revision)return false;value_=value;return true;}
 bool selectRequested(){if(value_.scene<0||!value_.selection||value_.selection==selected_)return false;selected_=value_.selection;return true;}
 bool displayOff(uint32_t localSeconds,bool usbPowered)const{if(value_.power==2)return true;if(value_.power==1)return false;return usbPowered&&localSeconds%86400>=3600&&localSeconds%86400<25200;}
};
