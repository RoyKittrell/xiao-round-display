#pragma once
#include <stdint.h>

// CHSC6X IRQ pulses when a report is ready; it is NOT a held-contact level.
// Packet byte 0 is the contact state (1=down, 0=up), as in the Zephyr driver.
// Retain that state between reports, including while IRQ returns high.
class TouchContact {
 bool known=false;
 int lastX=0,lastY=0;
 uint32_t lastPacketAt=0;
 public:
 bool active() const{return known;}
 struct Sample {bool down;int x,y;};
 Sample update(bool packetReceived,bool pressed,int x,int y,uint32_t now){
  if(packetReceived){
   lastPacketAt=now;
   if(!pressed)known=false;
   else if(x>=0&&x<240&&y>=0&&y<240){lastX=x;lastY=y;known=true;}
  }
  // Do not retain a touch indefinitely through an I2C bus failure.
  if(!packetReceived&&known&&uint32_t(now-lastPacketAt)>=120)known=false;
  return {known,lastX,lastY};
 }
};
