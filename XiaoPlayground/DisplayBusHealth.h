#pragma once
#include <stdint.h>

// A powered-off panel or a failed repeated-start transaction must not leave
// touch unavailable indefinitely. Recover only after sustained failures;
// throttle retries while offline and rate-limit controller restarts.
class DisplayBusHealth {
 uint32_t firstFailure=0,lastRecovery=0;
 uint16_t consecutive=0;
 bool recoveredBefore=false,seenGood=false;
 public:
 uint32_t reads=0,failures=0,recoveries=0,lastGood=0;
 void report(bool good,uint32_t now){
  reads++;
  if(good){consecutive=0;lastGood=now;seenGood=true;return;}
  failures++;if(!consecutive)firstFailure=now;
  if(consecutive<65535)consecutive++;
 }
 uint32_t pollInterval() const{return consecutive>=4?80:8;}
 bool recoveryDue(uint32_t now) const{
  return consecutive>=4&&uint32_t(now-firstFailure)>=100&&
   (!recoveredBefore||uint32_t(now-lastRecovery)>=1000);
 }
 void didRecover(uint32_t now){
  consecutive=0;recoveries++;lastRecovery=now;recoveredBefore=true;
 }
 uint32_t goodAge(uint32_t now) const{return seenGood?uint32_t(now-lastGood):0xffffffffu;}
};
