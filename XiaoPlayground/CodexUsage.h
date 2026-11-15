#pragma once
// Only sanitised display measurements arrive here, never account credentials.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

class CodexUsage {
public:
 static constexpr int MaxWindows=8;
 static constexpr uint32_t StaleAfter=300;
 struct Window {char label[23];uint16_t remainingTenths;uint32_t minutes,reset;};
private:
 Window windows_[MaxWindows]={};uint8_t count_=0,selected_=0;
 uint32_t observed_=0,age_=0,seenAt_=0,revision_=0;
 static bool number(const char* text,uint32_t& value){
  if(!text||!*text)return false;uint64_t n=0;
  for(const char* p=text;*p;p++){if(*p<'0'||*p>'9')return false;n=n*10+(*p-'0');if(n>0xffffffffu)return false;}
  value=uint32_t(n);return true;
 }
public:
 uint8_t count()const{return count_;}uint8_t selected()const{return selected_;}
 uint32_t observed()const{return observed_;}uint32_t revision()const{return revision_;}
 const Window& current()const{return windows_[selected_];}
 void next(){if(count_>1){selected_=(selected_+1)%count_;revision_++;}}
 // Called regularly even while hidden/asleep. Carry seconds across millis()
 // rollover and saturate very old readings instead of wrapping back to live.
 void tick(uint32_t now){if(!count_)return;uint32_t seconds=(now-seenAt_)/1000;
  seenAt_+=seconds*1000;age_=age_>0xffffffffu-seconds?0xffffffffu:age_+seconds;
 }
 uint32_t age()const{return age_;}
 bool stale(uint32_t unixNow)const{return !count_||age_>=StaleAfter||unixNow>=current().reset;}
 // U|observed Unix|count|label|remaining in tenths %|duration min|reset Unix...|CRC16
 bool accept(const char* packet,uint32_t now,uint32_t unixNow){
  if(!packet||packet[0]!='U'||packet[1]!='|'||strlen(packet)>=900)return false;
  const char* check=strrchr(packet,'|');if(!check||strlen(check+1)!=4)return false;
  uint16_t crc=0xffff;for(const char* p=packet;p<check;p++){crc^=uint16_t(uint8_t(*p))<<8;
   for(int b=0;b<8;b++)crc=(crc&0x8000)?uint16_t((crc<<1)^0x1021):uint16_t(crc<<1);}
  uint16_t sent=0;for(int i=1;i<=4;i++){char c=check[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(n<0)return false;sent=(sent<<4)|n;}
  if(crc!=sent)return false;
  char body[900];size_t length=check-packet;memcpy(body,packet,length);body[length]=0;
  // Keep empty fields so missing numbers/labels cannot shift later columns.
  char* fields[3+MaxWindows*4];int total=0;char* start=body;
  for(char* p=body;;p++){if(*p=='|'||!*p){bool end=!*p;if(total>=3+MaxWindows*4)return false;fields[total++]=start;if(end)break;*p=0;start=p+1;}}
  uint32_t observed,count;if(total<3||!number(fields[1],observed)||!number(fields[2],count)||count<1||count>MaxWindows||total!=int(3+4*count))return false;
  if(observed<946684800u||observed>4102444800u||observed+120ull<observed_)return false;
  if(unixNow&&uint64_t(observed)>uint64_t(unixNow)+120)return false;
  Window pending[MaxWindows]={};
  for(uint32_t i=0;i<count;i++){
   char* label=fields[3+i*4];size_t len=strlen(label);if(!len||len>22)return false;
   for(size_t j=0;j<len;j++)if(uint8_t(label[j])<32||uint8_t(label[j])>126)return false;
   uint32_t remaining,minutes,reset;
   if(!number(fields[4+i*4],remaining)||remaining>1000||!number(fields[5+i*4],minutes)||!minutes||minutes>5256000||!number(fields[6+i*4],reset)||reset<946684800u||reset>4102444800u)return false;
   strcpy(pending[i].label,label);pending[i].remainingTenths=remaining;pending[i].minutes=minutes;pending[i].reset=reset;
  }
  uint8_t selected=0;bool preserved=false;
  if(count_)for(uint32_t i=0;i<count;i++)if(!strcmp(windows_[selected_].label,pending[i].label)&&windows_[selected_].minutes==pending[i].minutes){selected=i;preserved=true;break;}
  if(!preserved)for(uint32_t i=1;i<count;i++)if(pending[i].remainingTenths<pending[selected].remainingTenths)selected=i;
  memcpy(windows_,pending,sizeof(windows_));count_=count;selected_=selected;observed_=observed;
  age_=unixNow>observed?unixNow-observed:0;seenAt_=now;revision_++;return true;
 }
 static void countdown(char* out,size_t n,uint32_t reset,uint32_t unixNow){
  if(reset<=unixNow){snprintf(out,n,"Awaiting refreshed quota");return;}
  uint32_t seconds=reset-unixNow;
  if(seconds>=86400)snprintf(out,n,"Resets in %lud %luh",(unsigned long)(seconds/86400),(unsigned long)(seconds/3600%24));
  else if(seconds>=3600)snprintf(out,n,"Resets in %luh %lum",(unsigned long)(seconds/3600),(unsigned long)(seconds/60%60));
  else if(seconds>=60)snprintf(out,n,"Resets in %lum %lus",(unsigned long)(seconds/60),(unsigned long)(seconds%60));
  else snprintf(out,n,"Resets in %lus",(unsigned long)seconds);
 }
};
