#pragma once
#include <stdint.h>
#include <string.h>
// Turns are clockwise from the panel's original USB-out orientation.
class ScreenOrientation {
 uint8_t value_=0;
public:
 struct Point {int x,y;};
 uint8_t turns()const{return value_;}
 bool set(uint8_t value){if(value>3||value==value_)return false;value_=value;return true;}
 static Point toPanel(int x,int y,uint8_t turns){
  switch(turns&3){case 1:return {239-y,x};case 2:return {239-x,239-y};case 3:return {y,239-x};default:return {x,y};}
 }
 Point touch(int x,int y)const{
  if(x<0||x>239||y<0||y>239)return {-1,-1};
  return toPanel(x,y,(4-value_)&3);
 }
 // O|0..3|CRC sets an absolute orientation; O|4|CRC only reads it.
 static bool parse(const char* packet,uint8_t& value){
  if(!packet||strlen(packet)!=8||packet[0]!='O'||packet[1]!='|'||packet[2]<'0'||packet[2]>'4'||packet[3]!='|')return false;
  uint16_t crc=0xffff,sent=0;
  for(int i=0;i<3;i++){crc^=uint16_t(uint8_t(packet[i]))<<8;for(int b=0;b<8;b++)crc=crc&0x8000?uint16_t((crc<<1)^0x1021):uint16_t(crc<<1);}
  for(int i=4;i<8;i++){char c=packet[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(n<0)return false;sent=uint16_t((sent<<4)|n);}
  if(crc!=sent)return false;value=uint8_t(packet[2]-'0');return true;
 }
};
