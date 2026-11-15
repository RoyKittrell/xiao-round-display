#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
class PrinterStatus {
public:
 enum State:uint8_t {Offline,Idle,Printing,Paused,Preparing,Finished,Error,Connecting};
 struct Snapshot {uint32_t observed=0,finish=0;State state=Connecting;int progress=-1,remaining=-1;char name[96]={};};
 static constexpr uint32_t StaleAfter=90;
private:
 Snapshot value_;bool ready_=false;uint32_t age_=0,seenAt_=0,revision_=0;
 static int digit(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
 static bool number(const char* s,uint32_t& v){if(!s||!*s)return false;uint64_t n=0;for(;*s;s++){if(*s<'0'||*s>'9')return false;n=n*10+*s-'0';if(n>0xffffffffu)return false;}v=n;return true;}
 static bool optional(const char* s,int& v,uint32_t maximum){if(!strcmp(s,"-1")){v=-1;return true;}uint32_t n;if(!number(s,n)||n>maximum)return false;v=n;return true;}
public:
 const Snapshot& current()const{return value_;}bool ready()const{return ready_;}
 uint32_t age()const{return age_;}uint32_t revision()const{return revision_;}
 bool stale()const{return !ready_||age_>=StaleAfter;}
 bool active()const{return value_.state==Printing||value_.state==Preparing;}
 void tick(uint32_t now){if(!ready_)return;uint32_t seconds=(now-seenAt_)/1000;seenAt_+=seconds*1000;age_=age_>0xffffffffu-seconds?0xffffffffu:age_+seconds;}
 bool accept(const char* packet,uint32_t now,uint32_t unixNow){
  if(!packet||strncmp(packet,"M|",2)||strlen(packet)>=400)return false;
  const char* tail=strrchr(packet,'|');if(!tail||strlen(tail+1)!=4)return false;
  uint16_t crc=0xffff,received=0;for(const char* p=packet;p<tail;p++){crc^=uint16_t(uint8_t(*p))<<8;for(int b=0;b<8;b++)crc=(crc&0x8000)?uint16_t((crc<<1)^0x1021):uint16_t(crc<<1);}
  for(int i=1;i<=4;i++){int n=digit(tail[i]);if(n<0)return false;received=(received<<4)|n;}if(received!=crc)return false;
  char body[400];size_t size=tail-packet;memcpy(body,packet,size);body[size]=0;
  char* fields[7];int count=0;char* start=body;
  for(char* p=body;;p++)if(*p=='|'||!*p){bool end=!*p;if(count>=7)return false;fields[count++]=start;if(end)break;*p=0;start=p+1;}
  if(count!=7)return false;Snapshot next;uint32_t state;
  if(!number(fields[1],next.observed)||next.observed<946684800u||next.observed>4102444800u||next.observed+120ull<value_.observed)return false;
  if(unixNow&&uint64_t(next.observed)>uint64_t(unixNow)+120)return false;
  if(!number(fields[2],state)||state>Connecting||!optional(fields[3],next.progress,100)||!optional(fields[4],next.remaining,2592000)||!number(fields[5],next.finish))return false;
  next.state=State(state);if(next.finish&&(next.finish<946684800u||next.finish>4102444800u))return false;
  if(next.state!=Printing&&next.state!=Preparing)next.finish=0; // Paused is never assigned a future ETA.
  size_t length=strlen(fields[6]);if(length%2||length>190)return false;
  for(size_t i=0;i<length/2;i++){int a=digit(fields[6][2*i]),b=digit(fields[6][2*i+1]);if(a<0||b<0)return false;int c=(a<<4)|b;if(c<32||c>126)return false;next.name[i]=c;}
  next.name[length/2]=0;
  value_=next;ready_=true;age_=unixNow>next.observed?unixNow-next.observed:0;seenAt_=now;revision_++;return true;
 }
};
