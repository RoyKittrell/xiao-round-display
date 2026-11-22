#pragma once
#include <stddef.h>
#include <stdint.h>

// Bluetooth CSC Set Cumulative Value. This changes the sensor counter only,
// never the physical crank count, calibration or saved ride distance.
struct CscControlRequest {
 uint8_t opcode=0,result=3;uint32_t value=0;
 bool applies()const{return opcode==1&&result==1;}
 static CscControlRequest decode(const uint8_t* data,size_t size){
  CscControlRequest r;if(!size)return r;r.opcode=data[0];
  if(r.opcode!=1){r.result=2;return r;}
  if(size!=5)return r;
  r.result=1;for(int i=0;i<4;i++)r.value|=uint32_t(data[i+1])<<(8*i);
  return r;
 }
 void response(uint8_t out[3])const{out[0]=0x10;out[1]=opcode;out[2]=result;}
};
