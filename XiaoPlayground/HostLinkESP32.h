#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <esp_heap_caps.h>
#include "WifiLink.h"

// Same authenticated GATT protocol as Nordic, implemented with the ESP32 core's
// NimBLE backend. Callbacks only touch bounded queues protected by a spinlock.
class HostLink:public Stream {
 static HostLink* instance;
 BLEServer* server=nullptr;BLECharacteristic* rx=nullptr;BLECharacteristic* tx=nullptr;
 uint8_t inputBytes[1536]={},outputBytes[2048]={};
 uint16_t readAt=0,readEnd=0,writeAt=0,writeEnd=0,inputEpoch=0;
 portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
 bool started=false,lineActive=false;
 uint8_t replyTransport=0,lineTransport=0;WifiLink wifi;uint16_t wifiEpoch=0;
 volatile bool connected=false,authenticated=false,pinVisible=false,notifySubscribed=false,resetPending=false;
 volatile uint16_t activeHandle=0xffff;
 uint32_t lastByte=0,pinStarted=0,connects=0;char pin[7]={};
 class ServerCallbacks:public BLEServerCallbacks {
  void onConnect(BLEServer* server,ble_gap_conn_desc* desc)override{auto& h=*instance;if(h.connected){server->disconnect(desc->conn_handle);return;}h.activeHandle=desc->conn_handle;h.connected=true;++h.connects;}
  void onDisconnect(BLEServer*,ble_gap_conn_desc* desc)override{
   if(desc->conn_handle!=instance->activeHandle)return;
   auto& h=*instance;portENTER_CRITICAL(&h.mux);h.connected=h.authenticated=h.pinVisible=false;
   h.notifySubscribed=false;h.activeHandle=0xffff;h.resetPending=true;
   h.readAt=h.readEnd=h.writeAt=h.writeEnd=0;++h.inputEpoch;portEXIT_CRITICAL(&h.mux);
   BLEDevice::startAdvertising();
  }
 } serverCallbacks;
 class SecurityCallbacks:public BLESecurityCallbacks {
  void onPassKeyNotify(uint32_t key)override{auto& h=*instance;snprintf(h.pin,sizeof(h.pin),"%06lu",(unsigned long)key);h.pinStarted=millis();h.pinVisible=true;}
  bool onSecurityRequest()override{return true;}
  bool onConfirmPIN(uint32_t)override{return false;} // Display-only passkey, never silent numeric confirmation.
  void onAuthenticationComplete(ble_gap_conn_desc* desc,int status)override{
   auto& h=*instance;h.authenticated=status==0&&desc&&desc->conn_handle==h.activeHandle&&desc->sec_state.encrypted&&desc->sec_state.authenticated;
   h.pinVisible=false;if(!h.authenticated&&desc)h.server->disconnect(desc->conn_handle);
  }
 } securityCallbacks;
 class RxCallbacks:public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic,ble_gap_conn_desc* desc)override{
   auto& h=*instance;if(!desc||!desc->sec_state.encrypted||!desc->sec_state.authenticated)return;
   auto data=characteristic->getValue();portENTER_CRITICAL(&h.mux);
   size_t free=(h.readAt+sizeof(h.inputBytes)-h.readEnd-1)%sizeof(h.inputBytes);
   if(data.length()>free){h.readAt=h.readEnd=0;++h.inputEpoch;}
   else for(size_t i=0;i<data.length();i++){h.inputBytes[h.readEnd]=data[i];h.readEnd=(h.readEnd+1)%sizeof(h.inputBytes);}
   portEXIT_CRITICAL(&h.mux);
  }
 } rxCallbacks;
 class TxCallbacks:public BLECharacteristicCallbacks {
  void onSubscribe(BLECharacteristic*,ble_gap_conn_desc* desc,uint16_t value)override{if(desc&&desc->conn_handle==instance->activeHandle)instance->notifySubscribed=(value&1)!=0;}
 } txCallbacks;
public:
 void begin(unsigned long baud){Serial.begin(baud);Serial.setTxTimeoutMs(0);}
 void beginWifi(){wifi.begin();}
 bool provisionWifi(char* text){return replyTransport==0&&wifi.provision(text);}
 void wifiStatus(){wifi.status(*this);}
 bool beginBluetooth(){
  instance=this;if(!BLEDevice::init(Board::BluetoothName))return false;
  BLEDevice::setSecurityCallbacks(&securityCallbacks);BLESecurity::setCapability(ESP_IO_CAP_OUT);
  BLESecurity::setAuthenticationMode(true,true,true);BLESecurity::setKeySize(16);
  BLESecurity::regenPassKeyOnConnect(true);BLEDevice::setMTU(247);
  server=BLEDevice::createServer();server->setCallbacks(&serverCallbacks);
  auto service=server->createService("82a64c00-1e4f-4f34-b9e9-7c0191a0f725");
  rx=service->createCharacteristic("82a64c01-1e4f-4f34-b9e9-7c0191a0f725",BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ_AUTHEN|BLECharacteristic::PROPERTY_WRITE_AUTHEN);
  tx=service->createCharacteristic("82a64c02-1e4f-4f34-b9e9-7c0191a0f725",BLECharacteristic::PROPERTY_NOTIFY);
  rx->setCallbacks(&rxCallbacks);tx->setCallbacks(&txCallbacks);rx->setValue("");service->start();
  auto advertising=BLEDevice::getAdvertising();advertising->addServiceUUID(service->getUUID());advertising->setScanResponse(true);advertising->start();started=true;return true;
 }
 void serviceBluetooth(){
  wifi.service();if(wifiEpoch!=wifi.epoch()){wifiEpoch=wifi.epoch();++inputEpoch;if(lineTransport==2)lineActive=false;}
  if(!started)return;
  if(resetPending){if(lineTransport==1)lineActive=false;if(replyTransport==1)replyTransport=0;resetPending=false;}
  if(pinVisible&&uint32_t(millis()-pinStarted)>60000)pinVisible=false;
  if(lineActive&&uint32_t(millis()-lastByte)>5000){lineActive=false;++inputEpoch;}
  if(!authenticated||!notifySubscribed)return;
  uint8_t chunk[20];uint16_t n=0,start,next,connection=activeHandle;
  portENTER_CRITICAL(&mux);start=next=writeAt;while(next!=writeEnd&&n<sizeof(chunk)){chunk[n++]=outputBytes[next];next=(next+1)%sizeof(outputBytes);}portEXIT_CRITICAL(&mux);
  if(n){auto packet=ble_hs_mbuf_from_flat(chunk,n);if(!packet)return;
   int result=ble_gatts_notify_custom(connection,tx->getHandle(),packet);
   if(!result){portENTER_CRITICAL(&mux);if(activeHandle==connection&&writeAt==start)writeAt=next;portEXIT_CRITICAL(&mux);}
  }
 }
 bool authenticating()const{return connected&&!authenticated;}
 bool pairingVisible()const{return pinVisible;}
 const char* pairingPIN()const{return pin;}
 uint16_t epoch()const{return inputEpoch;}
 bool bluetoothReady()const{return started;}
 void endResponse(){replyTransport=0;}
 void diagnostics(){Serial.print("BOARD ");Serial.print(Board::Name);Serial.print(" BLE ready=");Serial.print(started);Serial.print(" connected=");Serial.print(connected);Serial.print(" authenticated=");Serial.print(authenticated);Serial.print(" free_heap=");Serial.print(ESP.getFreeHeap());Serial.print(" max_alloc=");Serial.print(ESP.getMaxAllocHeap());
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  Serial.print(" psram_bytes=");Serial.print(ESP.getPsramSize());Serial.print(" free_psram=");Serial.print(ESP.getFreePsram());Serial.print(" flash_bytes=");Serial.print(ESP.getFlashChipSize());
#endif
  Serial.println();}
 int queued(){portENTER_CRITICAL(&mux);int n=(readEnd+sizeof(inputBytes)-readAt)%sizeof(inputBytes);portEXIT_CRITICAL(&mux);return n;}
 int available()override{serviceBluetooth();if(lineActive)return lineTransport==1?queued():lineTransport==2?wifi.available():Serial.available();return Serial.available()?Serial.available():queued()?queued():wifi.available();}
 int read()override{
  uint8_t source=lineActive?lineTransport:Serial.available()?0:queued()?1:2;int ch=-1;
  if(source==1){portENTER_CRITICAL(&mux);if(readAt!=readEnd){ch=inputBytes[readAt];readAt=(readAt+1)%sizeof(inputBytes);}portEXIT_CRITICAL(&mux);}else if(source==2)ch=wifi.read();else ch=Serial.read();
  if(ch<0)return ch;if(source!=lineTransport){++inputEpoch;lineTransport=source;}
  replyTransport=source;lastByte=millis();if(!lineActive&&(ch=='@'||ch=='T'))lineActive=true;if(ch=='\n'||ch=='\r')lineActive=false;return ch;
 }
 int peek()override{return lineTransport==0?Serial.peek():-1;}
 void flush()override{serviceBluetooth();}
 size_t write(uint8_t ch)override{
  // Keep unread USB best-effort. Wireless replies go only to their requester.
  if(replyTransport==0&&Serial&&Serial.availableForWrite()>0)Serial.write(ch);
  if(replyTransport==1){portENTER_CRITICAL(&mux);auto next=(writeEnd+1)%sizeof(outputBytes);if(next!=writeAt){outputBytes[writeEnd]=ch;writeEnd=next;}portEXIT_CRITICAL(&mux);}
  if(replyTransport==2)wifi.write(ch);
  return 1;
 }
 using Print::write;
};
HostLink* HostLink::instance=nullptr;
