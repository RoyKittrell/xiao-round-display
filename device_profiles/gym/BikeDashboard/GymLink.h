#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <esp_timer.h>
#include <services/gatt/ble_svc_gatt.h>
#include "WifiLink.h"
#include "CyclingCadence.h"
#include "EstimatedPower.h"

// USB/TLS carry controls. Bluetooth exposes only the standard read/notify CSC
// sensor, with normal no-input/no-output bonding and no writable control GATT.
class HostLink:public Stream {
  static HostLink* instance;
  BLEServer* server=nullptr;
  BLECharacteristic* measurement=nullptr;
  BLECharacteristic* powerMeasurement=nullptr;
  int resistanceLevel=1;
  uint16_t featureHandle=0;
  CyclingCadence cadence;
  WifiLink wifi;
  bool started=false,lineActive=false;
  uint8_t replyTransport=0,lineTransport=0;
  uint16_t inputEpoch=0,wifiEpoch=0;
  uint32_t lastByte=0,sentAt=0,powerSentAt=0;
  volatile bool connected=false,subscribed=false,powerSubscribed=false,encrypted=false;
  volatile uint16_t connection=0xffff;
  volatile int securityStatus=-1;
  volatile uint32_t connects=0,disconnects=0;
  class ServerCallbacks:public BLEServerCallbacks {
    void onConnect(BLEServer* s,ble_gap_conn_desc* d)override{
      auto& h=*instance;if(h.connected){s->disconnect(d->conn_handle);return;}
      h.connection=d->conn_handle;h.connected=true;++h.connects;
    }
    void onDisconnect(BLEServer*,ble_gap_conn_desc* d)override{
      auto& h=*instance;if(d->conn_handle!=h.connection)return;
      h.connected=h.subscribed=h.powerSubscribed=h.encrypted=false;h.connection=0xffff;++h.disconnects;
      BLEDevice::startAdvertising();
    }
  } serverCallbacks;
  class SecurityCallbacks:public BLESecurityCallbacks {
    bool onSecurityRequest()override{return true;}
    void onAuthenticationComplete(ble_gap_conn_desc* d,int status)override{
      auto& h=*instance;h.securityStatus=status;
      h.encrypted=status==0&&d&&d->conn_handle==h.connection&&d->sec_state.encrypted;
    }
  } securityCallbacks;
  class MeasurementCallbacks:public BLECharacteristicCallbacks {
    void onSubscribe(BLECharacteristic* c,ble_gap_conn_desc* d,uint16_t value)override{
      if(d&&d->conn_handle==instance->connection){
        if(c==instance->measurement)instance->subscribed=(value&1)!=0;
        if(c==instance->powerMeasurement)instance->powerSubscribed=(value&1)!=0;
      }
    }
  } measurementCallbacks;
public:
  void begin(unsigned long baud){Serial.begin(baud);Serial.setTxTimeoutMs(0);}
  void beginWifi(){wifi.begin();}
  bool provisionWifi(char* text){return replyTransport==0&&wifi.provision(text);}
  bool isUSBResponse()const{return replyTransport==0;}
  void wifiStatus(){wifi.status(*this);}
  bool beginBluetooth(){
    instance=this;if(!BLEDevice::init(Board::BluetoothName))return false;
    BLEDevice::setSecurityCallbacks(&securityCallbacks);
    BLESecurity::setCapability(ESP_IO_CAP_NONE);
    BLESecurity::setAuthenticationMode(true,false,true);
    BLESecurity::setKeySize(16);
    server=BLEDevice::createServer();server->setCallbacks(&serverCallbacks);
    auto service=server->createService(BLEUUID(uint16_t(0x1816)));
    measurement=service->createCharacteristic(BLEUUID(uint16_t(0x2a5b)),BLECharacteristic::PROPERTY_NOTIFY);
    measurement->setCallbacks(&measurementCallbacks);
    auto feature=service->createCharacteristic(BLEUUID(uint16_t(0x2a5c)),BLECharacteristic::PROPERTY_READ);
    uint8_t flags[2]={2,0};feature->setValue(flags,2);
    auto location=service->createCharacteristic(BLEUUID(uint16_t(0x2a5d)),BLECharacteristic::PROPERTY_READ);
    uint8_t other=0;location->setValue(&other,1);
    if(!service->start())return false;
    auto powerService=server->createService(BLEUUID(uint16_t(0x1818)));
    powerMeasurement=powerService->createCharacteristic(BLEUUID(uint16_t(0x2a63)),BLECharacteristic::PROPERTY_NOTIFY);
    powerMeasurement->setCallbacks(&measurementCallbacks);
    auto powerFeature=powerService->createCharacteristic(BLEUUID(uint16_t(0x2a65)),BLECharacteristic::PROPERTY_READ);
    uint8_t powerFlags[4]={8,0,0,0}; // Crank revolution data supported only.
    powerFeature->setValue(powerFlags,4);
    auto powerLocation=powerService->createCharacteristic(BLEUUID(uint16_t(0x2a5d)),BLECharacteristic::PROPERTY_READ);
    powerLocation->setValue(&other,1);
    if(!powerService->start())return false;
    auto advertising=BLEDevice::getAdvertising();advertising->addServiceUUID(powerService->getUUID());
    advertising->addServiceUUID(service->getUUID());
    advertising->setScanResponse(true);if(!advertising->start())return false;
    featureHandle=feature->getHandle();ble_svc_gatt_changed(1,0xffff);
    started=featureHandle&&featureHandle!=0xffff;return started;
  }
  void serviceBluetooth(){
    wifi.service();
    if(wifiEpoch!=wifi.epoch()){
      wifiEpoch=wifi.epoch();++inputEpoch;if(lineTransport==2)lineActive=false;
    }
    if(lineActive&&uint32_t(millis()-lastByte)>5000){lineActive=false;++inputEpoch;}
    if(started&&connected&&subscribed&&uint32_t(millis()-sentAt)>=1000){
      uint8_t data[5];cadence.measurement(data);
      auto packet=ble_hs_mbuf_from_flat(data,sizeof(data));
      if(packet&&!ble_gatts_notify_custom(connection,measurement->getHandle(),packet))sentAt=millis();
    }
    if(started&&connected&&powerSubscribed&&uint32_t(millis()-powerSentAt)>=1000){
      uint8_t data[8];powerPacket(data);
      auto packet=ble_hs_mbuf_from_flat(data,sizeof(data));
      if(packet&&!ble_gatts_notify_custom(connection,powerMeasurement->getHandle(),packet))powerSentAt=millis();
    }
  }
  void setResistance(int n){resistanceLevel=n;}
  float realCadence()const{return cadence.rpm(esp_timer_get_time());}
  int16_t realPower()const{return estimatedWatts(realCadence(),resistanceLevel);}
  void powerPacket(uint8_t data[8])const{
    int16_t watts=realPower();
    data[0]=0x20;data[1]=0; // Crank revolution data present (bit 5).
    data[2]=uint8_t(watts);data[3]=uint8_t(uint16_t(watts)>>8);
    data[4]=uint8_t(cadence.count());data[5]=uint8_t(cadence.count()>>8);
    data[6]=uint8_t(cadence.time());data[7]=uint8_t(cadence.time()>>8);
  }
  bool pairingVisible()const{return false;}
  const char* pairingPIN()const{return "";}
  uint16_t epoch()const{return inputEpoch;}
  bool bluetoothReady()const{return started;}
  bool cyclingReady()const{return started;}
  void crankPulse(uint32_t eventUs){
    uint64_t now=esp_timer_get_time();cadence.pulse(now-uint32_t(uint32_t(now)-eventUs));
  }
  void setPulsesPerRevolution(uint8_t n){cadence.calibrate(n);}
  uint8_t pulsesPerRevolution()const{return cadence.calibration();}
  void bluetoothStatus(){
    print("BLE ready=");print(started);print(" connected=");print(connected);
    print(" encrypted=");print(encrypted);print(" security_status=");print(securityStatus);
    print(" connects=");print(connects);print(" disconnects=");println(disconnects);
  }
  void cyclingStatus(){
    print("CSC ready=");print(started);print(" subscribed=");print(subscribed);
    print(" revolutions=");print(cadence.count());print(" event_time=");print(cadence.time());
    print(" ppr=");print(cadence.calibration());print(" feature_handle=");println(featureHandle);
  }
  void powerStatus(){
    printf("POWER estimated=1 watts=%d cadence=%.1f resistance=%d subscribed=%d model=squared-rpm-v1\n",realPower(),realCadence(),resistanceLevel,powerSubscribed);
  }
  void endResponse(){replyTransport=0;lineActive=false;}
  int available()override{
    serviceBluetooth();if(lineActive)return lineTransport==2?wifi.available():Serial.available();
    return Serial.available()?Serial.available():wifi.available();
  }
  int read()override{
    uint8_t source=lineActive?lineTransport:Serial.available()?0:2;
    int c=source==2?wifi.read():Serial.read();if(c<0)return c;
    if(source!=lineTransport){++inputEpoch;lineTransport=source;}
    replyTransport=source;lastByte=millis();
    if(!lineActive&&c!='\n'&&c!='\r')lineActive=true;
    if(c=='\n'||c=='\r')lineActive=false;return c;
  }
  int peek()override{return lineTransport==0?Serial.peek():-1;}
  void flush()override{serviceBluetooth();}
  size_t write(uint8_t c)override{
    if(replyTransport==0&&Serial&&Serial.availableForWrite()>0)Serial.write(c);
    if(replyTransport==2)wifi.write(c);return 1;
  }
  using Print::write;
};
HostLink* HostLink::instance=nullptr;
