#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <initializer_list>

class WeatherStatus {
public:
 static constexpr int Hours=30;
 static constexpr uint32_t StaleAfter=2700;
 struct Snapshot {uint32_t observed=0,base=0;int offset=0,temperature10=-9999,uv10=-1;char city[25]={};uint8_t probabilities[Hours]={},codes[Hours]={},days[Hours]={};};
 enum Icon {Unknown,Clear,PartlyCloudy,Cloudy,Fog,Rain,Snow,Thunder};
 struct Window {int first=-1,last=-1,threshold=0;bool missing=false;};
private:
 Snapshot value_;bool ready_=false;uint32_t revision_=0;
 static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
 static bool number(const char* s,int64_t& out,int64_t lo,int64_t hi){if(!s||!*s)return false;bool negative=*s=='-';if(negative)s++;if(!*s)return false;int64_t v=0;for(;*s;s++){if(*s<'0'||*s>'9'||v>5000000000ll)return false;v=v*10+*s-'0';}out=negative?-v:v;return out>=lo&&out<=hi;}
 static bool bytes(const char* s,uint8_t* out,int count,int max){if(int(strlen(s))!=count*2)return false;for(int i=0;i<count;i++){int a=hex(s[i*2]),b=hex(s[i*2+1]);if(a<0||b<0)return false;int n=a*16+b;if(n>max&&n!=255)return false;out[i]=n;}return true;}
public:
 const Snapshot& current()const{return value_;}bool ready()const{return ready_;}uint32_t revision()const{return revision_;}
 bool stale(uint32_t now)const{return !ready_||now+120ull<value_.observed||(now>=value_.observed&&now-value_.observed>=StaleAfter);}
 int index(uint32_t time)const{return !ready_||time<value_.base?-1:int((time-value_.base)/3600);}
 int probability(int i)const{return i<0||i>=Hours||value_.probabilities[i]==255?-1:value_.probabilities[i];}
 static Icon classify(uint8_t c){if(c==0)return Clear;if(c==1||c==2)return PartlyCloudy;if(c==3)return Cloudy;if(c==45||c==48)return Fog;if(c>=95&&c<=99)return Thunder;if((c>=71&&c<=77)||c==85||c==86)return Snow;if((c>=51&&c<=67)||(c>=80&&c<=82))return Rain;return Unknown;}
 Icon icon(uint32_t now)const{int begin=index(now);if(begin<0)return Unknown;int counts[8]={};for(int k=begin;k<begin+3&&k<Hours;k++)counts[classify(value_.codes[k])]++;if(counts[Thunder])return Thunder;if(counts[Snow])return Snow;if(counts[Rain])return Rain;Icon best=Unknown;for(int k=1;k<5;k++)if(counts[k]>counts[best])best=Icon(k);return best;}
 bool isDay(uint32_t now)const{int i=index(now);return i<0||i>=Hours||value_.days[i]!=0;}
 Window window(uint32_t now,int horizon)const{
  Window out;int start=index(now);if(start<0){out.missing=true;return out;}
  int end=index(now+uint32_t(horizon)*3600-1);
  for(int i=start;i<=end;i++)if(probability(i)<0)out.missing=true;
  for(int threshold:{60,30}){for(int i=start;i<=end;i++)if(probability(i)>=threshold){out.first=i;out.last=i+1;out.threshold=threshold;while(out.last<=end&&probability(out.last)>=threshold)out.last++;return out;}}
  return out;
 }
 bool accept(const char* packet,uint32_t now){
  if(!packet||strncmp(packet,"W|",2)||strlen(packet)>=400)return false;
  const char* tail=strrchr(packet,'|');if(!tail||strlen(tail+1)!=4)return false;
  uint16_t crc=0xffff,sent=0;for(auto p=packet;p<tail;p++){crc^=uint16_t(uint8_t(*p))<<8;for(int b=0;b<8;b++)crc=(crc&0x8000)?uint16_t((crc<<1)^0x1021):uint16_t(crc<<1);}
  for(int i=1;i<=4;i++){int n=hex(tail[i]);if(n<0)return false;sent=(sent<<4)|n;}if(crc!=sent)return false;
  char body[400];size_t len=tail-packet;memcpy(body,packet,len);body[len]=0;char* f[11];int count=0;char* start=body;
  for(char* p=body;;p++)if(*p=='|'||!*p){bool end=!*p;if(count>=11)return false;f[count++]=start;if(end)break;*p=0;start=p+1;}
  if(count!=11||strcmp(f[1],"1"))return false;Snapshot s;int64_t n;
  if(!number(f[2],n,946684800ll,4102444800ll))return false;s.observed=n;
  if(now&&uint64_t(s.observed)>uint64_t(now)+120)return false;
  if(ready_&&s.observed+120ull<value_.observed)return false;
  if(!number(f[3],n,946684800ll,4102444800ll))return false;s.base=n;
  if(s.base>s.observed||s.observed-s.base>=3600)return false;
  if(!number(f[4],n,-43200,50400))return false;s.offset=n;
  if((int64_t(s.base)+s.offset)%3600)return false;
  if(!number(f[5],n,-9999,650)||(n<-900&&n!=-9999))return false;s.temperature10=n;
  if(!number(f[6],n,-1,500))return false;s.uv10=n;
  size_t cityLen=strlen(f[7]);if(!cityLen||cityLen%2||cityLen>48)return false;
  for(size_t i=0;i<cityLen/2;i++){int a=hex(f[7][i*2]),b=hex(f[7][i*2+1]);if(a<0||b<0)return false;int c=a*16+b;if(c<32||c>126)return false;s.city[i]=c;}s.city[cityLen/2]=0;
  if(!bytes(f[8],s.probabilities,Hours,100)||!bytes(f[9],s.codes,Hours,99)||!bytes(f[10],s.days,Hours,1))return false;
  value_=s;ready_=true;revision_++;return true;
 }
};
