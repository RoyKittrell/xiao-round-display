#pragma once
#include <Arduino.h>
#include <bluefruit.h>

// One bounded command stream over USB or an authenticated BLE connection.
// No flash/update service: Bluetooth only carries the existing scene protocol.
class HostLink:public Stream {
 static HostLink* instance;
 BLEService service{"82a64c00-1e4f-4f34-b9e9-7c0191a0f725"};
 BLECharacteristic rx{"82a64c01-1e4f-4f34-b9e9-7c0191a0f725"};
 BLECharacteristic tx{"82a64c02-1e4f-4f34-b9e9-7c0191a0f725"};
 uint8_t inputBytes[1536]={},outputBytes[2048]={};
 uint16_t readAt=0,readEnd=0,writeAt=0,writeEnd=0;
 bool started=false,replyBluetooth=false,lineActive=false,lineBluetooth=false,pinVisible=false;
 uint32_t lastByte=0,pinStarted=0;uint16_t inputEpoch=0;
 char pin[7]={};uint32_t connects=0;uint8_t lastDisconnect=0;
 static void connected(uint16_t){++instance->connects;}
 static void received(uint16_t,BLECharacteristic*,uint8_t* data,uint16_t length){
  auto& h=*instance;
  if(!Bluefruit.Security.isAuthenticated())return;
  const auto free=uint16_t((h.readAt+sizeof(h.inputBytes)-h.readEnd-1)%sizeof(h.inputBytes));
  if(length>free){h.readAt=h.readEnd=0;h.lineActive=false;++h.inputEpoch;return;}
  for(uint16_t i=0;i<length;i++){h.inputBytes[h.readEnd]=data[i];h.readEnd=(h.readEnd+1)%sizeof(h.inputBytes);}
 }
 static bool passkey(uint16_t,const uint8_t digits[6],bool match){
  if(match)return false; // DisplayOnly never silently approves a comparison.
  memcpy(instance->pin,digits,6);instance->pin[6]=0;
  instance->pinVisible=true;instance->pinStarted=millis();return true;
 }
 static void secured(uint16_t){instance->pinVisible=false;}
 static void disconnected(uint16_t,uint8_t reason){
  instance->lastDisconnect=reason;
  auto& h=*instance;h.pinVisible=false;h.lineActive=false;h.replyBluetooth=false;
  h.readAt=h.readEnd=h.writeAt=h.writeEnd=0;++h.inputEpoch;
 }
 public:
 void begin(unsigned long baud){Serial.begin(baud);}
 bool beginBluetooth(){
  instance=this;Bluefruit.autoConnLed(false);Bluefruit.configPrphBandwidth(BANDWIDTH_NORMAL);
  if(!Bluefruit.begin(1,0))return false;
  Bluefruit.Periph.setConnIntervalMS(30,60);Bluefruit.setName("XIAO Round Display");Bluefruit.setTxPower(0);
  Bluefruit.Security.setIOCaps(true,false,false);Bluefruit.Security.setMITM(true);
  Bluefruit.Security.setSecureConnectionsRequired(true);
  Bluefruit.Security.setPairPasskeyCallback(passkey);Bluefruit.Security.setSecuredCallback(secured);
  Bluefruit.Periph.setConnectCallback(connected);Bluefruit.Periph.setDisconnectCallback(disconnected);
  if(service.begin()!=ERROR_NONE)return false;
  rx.setProperties(CHR_PROPS_READ|CHR_PROPS_WRITE);rx.setPermission(SECMODE_ENC_WITH_MITM,SECMODE_ENC_WITH_MITM);rx.setMaxLen(244);rx.setWriteCallback(received);
  tx.setProperties(CHR_PROPS_NOTIFY);tx.setPermission(SECMODE_ENC_WITH_MITM,SECMODE_NO_ACCESS);tx.setMaxLen(244);
  if(rx.begin()!=ERROR_NONE||tx.begin()!=ERROR_NONE)return false;
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(service);Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);Bluefruit.Advertising.setInterval(160,320);
  Bluefruit.Advertising.setFastTimeout(30);started=Bluefruit.Advertising.start(0);return started;
 }
 void serviceBluetooth(){
  if(!started)return;yield();
  if(pinVisible&&uint32_t(millis()-pinStarted)>60000)pinVisible=false;
  if(lineActive&&uint32_t(millis()-lastByte)>5000){lineActive=false;++inputEpoch;}
  if(writeAt==writeEnd||!Bluefruit.Security.isAuthenticated()||!tx.notifyEnabled())return;
  uint8_t chunk[20];uint16_t n=0,p=writeAt;
  while(p!=writeEnd&&n<sizeof(chunk)){chunk[n++]=outputBytes[p];p=(p+1)%sizeof(outputBytes);}
  if(tx.notify(chunk,n))writeAt=p;
 }
 void diagnostics(){
  Serial.print("BLE ready=");Serial.print(started);Serial.print(" links=");Serial.print(connects);
  Serial.print(" connected=");Serial.print(Bluefruit.connected());Serial.print(" authenticated=");Serial.print(Bluefruit.Security.isAuthenticated());
  Serial.print(" rx_handle=");Serial.print(rx.handles().value_handle);Serial.print(" tx_handle=");Serial.print(tx.handles().value_handle);Serial.print(" disconnect_reason=");Serial.print(lastDisconnect,HEX);Serial.print(" free_heap=");Serial.println(getFreeHeap());
 }
 bool authenticating()const{return started&&Bluefruit.connected()&&!Bluefruit.Security.isAuthenticated();}
 bool pairingVisible()const{return pinVisible;}
 const char* pairingPIN()const{return pin;}
 uint16_t epoch()const{return inputEpoch;}
 bool bluetoothReady()const{return started;}
 void endResponse(){replyBluetooth=false;}
 int available()override{
  serviceBluetooth();
  if(lineActive)return lineBluetooth ? (readEnd+sizeof(inputBytes)-readAt)%sizeof(inputBytes):Serial.available();
  return Serial.available()?Serial.available():(readEnd+sizeof(inputBytes)-readAt)%sizeof(inputBytes);
 }
 int read()override{
  bool bluetooth=lineActive?lineBluetooth:!Serial.available();
  int ch=-1;
  if(bluetooth){if(readAt!=readEnd){ch=inputBytes[readAt];readAt=(readAt+1)%sizeof(inputBytes);}}
  else ch=Serial.read();
  if(ch<0)return ch;
  if(bluetooth!=lineBluetooth){++inputEpoch;lineBluetooth=bluetooth;}
  replyBluetooth=bluetooth;lastByte=millis();
  if(!lineActive&&(ch=='@'||ch=='T'))lineActive=true;
  if(ch=='\n'||ch=='\r')lineActive=false;
  return ch;
 }
 int peek()override{return lineActive&&lineBluetooth ? (readAt==readEnd?-1:inputBytes[readAt]):Serial.peek();}
 void flush()override{Serial.flush();serviceBluetooth();}
 size_t write(uint8_t ch)override{
  Serial.write(ch);
  if(replyBluetooth){uint16_t next=(writeEnd+1)%sizeof(outputBytes);if(next!=writeAt){outputBytes[writeEnd]=ch;writeEnd=next;}}
  return 1;
 }
 using Print::write;
};
HostLink* HostLink::instance=nullptr;
