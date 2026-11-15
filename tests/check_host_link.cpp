#include "../XiaoPlayground/HostLink.h"
#include <cassert>
#include <string>
void receive(const std::string& s){auto p=BLECharacteristic::rx;p->callback(0,p,(uint8_t*)s.data(),s.size());}
int main(){
 HostLink link;link.begin(115200);assert(link.beginBluetooth());
 assert(BLECharacteristic::rx->writePerm==SECMODE_ENC_WITH_MITM);
 receive("n");assert(link.available()==0); // Reject unpaired peers even if a stack callback arrives.
 Bluefruit.link=true;assert(link.authenticating());Bluefruit.Security.auth=true;assert(!link.authenticating());
 Serial.in={'@','C'};assert(link.read()=='@');auto usbEpoch=link.epoch();
 receive("@U\n");assert(link.read()=='C'&&link.available()==0); // Never splice BLE into an unfinished USB packet.
 Serial.in={'\n'};assert(link.read()=='\n');assert(link.read()=='@'&&link.epoch()!=usbEpoch);
 assert(link.read()=='U');assert(link.read()=='\n');
 for(char c:std::string("ACK U\n"))link.write(c);
 link.endResponse();link.serviceBluetooth();
 assert(std::string(BLECharacteristic::notifications.begin(),BLECharacteristic::notifications.end())=="ACK U\n");
 auto n=BLECharacteristic::notifications.size();link.write('x');link.serviceBluetooth();assert(BLECharacteristic::notifications.size()==n);
 receive("@C");assert(link.read()=='@');auto epoch=link.epoch();now+=5001;link.serviceBluetooth();assert(link.epoch()!=epoch);
 receive(std::string(2000,'z'));assert(link.available()==0); // Overflow cancels parser epoch and clears partial data.
 const uint8_t pin[]={'0','1','2','3','4','5'};
 assert(!Bluefruit.Security.passkey(0,pin,true));assert(!link.pairingVisible());
 assert(Bluefruit.Security.passkey(0,pin,false)&&link.pairingVisible());assert(std::string(link.pairingPIN())=="012345");
 Bluefruit.Security.secured(0);assert(!link.pairingVisible());
 Bluefruit.Periph.disconnect(0,0);assert(link.available()==0);
 puts("PASS production HostLink: authenticated RX, USB/BLE isolation, fragment timeout, overflow reset, reply routing and pairing display lifecycle");
}
