#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <esp_timer.h>
#include <services/gatt/ble_svc_gatt.h>
#define XIAO_DEVICE_PROFILE "gym"
#include "WifiLink.h"
#include "CyclingCadence.h"
#include "EstimatedPower.h"
#include "RideStats.h"
#include "VirtualWheel.h"
#include "CscControl.h"
#include <atomic>

// USB/TLS carry controls. Bluetooth exposes only the standard read/notify CSC
// sensors (real cranks, virtual wheel and estimated power), plus the standard
// wheel-counter control point. No writable application/settings control GATT.
class HostLink:public Stream {
  static HostLink* instance;
  BLEServer* server=nullptr;
  BLECharacteristic* measurement=nullptr;
  BLECharacteristic* powerMeasurement=nullptr;
  BLECharacteristic* cscControl=nullptr;
  std::atomic<bool> controlSubscribed{false},controlBusy{false},controlPending{false};
  portMUX_TYPE controlMux=portMUX_INITIALIZER_UNLOCKED;
  CscControlRequest controlRequest;
  uint32_t controlRequests=0,controlApplied=0;
  int resistanceLevel=1;
  uint16_t featureHandle=0;
  CyclingCadence cadence;
  RideStats rideTotals;VirtualWheel wheel;double bluetoothJoules=0;
  uint32_t distanceMM=5000,rideSavedAt=0;bool wasMoving=false;
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
      portENTER_CRITICAL(&h.controlMux);
      h.controlSubscribed=false;h.controlPending=false;h.controlBusy=false;
      portEXIT_CRITICAL(&h.controlMux);
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
  class ControlCallbacks:public BLECharacteristicCallbacks {
    void onSubscribe(BLECharacteristic*,ble_gap_conn_desc* d,uint16_t value)override{
      if(d&&d->conn_handle==instance->connection)instance->controlSubscribed=(value&2)!=0;
    }
    void onWrite(BLECharacteristic* c,ble_gap_conn_desc* d)override{
      auto& h=*instance;if(!d||d->conn_handle!=h.connection)return;
      auto value=c->getValue();
      portENTER_CRITICAL(&h.controlMux);
      if(h.controlSubscribed&&!h.controlBusy){
        h.controlBusy=true;h.controlRequest=CscControlRequest::decode((const uint8_t*)value.c_str(),value.length());
        ++h.controlRequests;h.controlPending=true;
      }
      portEXIT_CRITICAL(&h.controlMux);
    }
    void onStatus(BLECharacteristic*,Status status,uint32_t)override{
      if(status!=SUCCESS_NOTIFY)instance->controlBusy=false;
    }
  } controlCallbacks;
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
    uint8_t flags[2]={3,0};feature->setValue(flags,2);
    auto location=service->createCharacteristic(BLEUUID(uint16_t(0x2a5d)),BLECharacteristic::PROPERTY_READ);
    uint8_t other=0;location->setValue(&other,1);
    cscControl=service->createCharacteristic(BLEUUID(uint16_t(0x2a55)),BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_INDICATE);
    cscControl->setCallbacks(&controlCallbacks);
    if(!service->start())return false;
    auto powerService=server->createService(BLEUUID(uint16_t(0x1818)));
    powerMeasurement=powerService->createCharacteristic(BLEUUID(uint16_t(0x2a63)),BLECharacteristic::PROPERTY_NOTIFY);
    powerMeasurement->setCallbacks(&measurementCallbacks);
    auto powerFeature=powerService->createCharacteristic(BLEUUID(uint16_t(0x2a65)),BLECharacteristic::PROPERTY_READ);
    uint8_t powerFlags[4]={140,0,0,0}; // Wheel + crank data; wheel is virtual (2100 mm).
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
    // Handle counter changes on the same task as pedal events. Indications use
    // NimBLE's asynchronous path so the display/sensor loop never waits for ACK.
    if(controlPending){
      uint8_t response[3];bool send=false;uint16_t peer=0xffff;
      portENTER_CRITICAL(&controlMux);
      if(controlPending&&connected&&controlSubscribed){
        controlPending=false;peer=connection;send=true;
        if(controlRequest.applies()){wheel.setCumulative(controlRequest.value);++controlApplied;}
        controlRequest.response(response);
      }else{controlPending=false;controlBusy=false;}
      portEXIT_CRITICAL(&controlMux);
      if(send){
        auto packet=ble_hs_mbuf_from_flat(response,sizeof(response));
        if(!packet||ble_gatts_indicate_custom(peer,cscControl->getHandle(),packet))controlBusy=false;
      }
    }
    rideTotals.tick(esp_timer_get_time());
    bool moving=realCadence()>0;
    if(rideTotals.dirty&&((wasMoving&&!moving)||uint32_t(millis()-rideSavedAt)>=30000))saveRide();
    wasMoving=moving;
    wifi.service();
    if(wifiEpoch!=wifi.epoch()){
      wifiEpoch=wifi.epoch();++inputEpoch;if(lineTransport==2)lineActive=false;
    }
    if(lineActive&&uint32_t(millis()-lastByte)>5000){lineActive=false;++inputEpoch;}
    if(started&&connected&&subscribed&&uint32_t(millis()-sentAt)>=1000){
      uint8_t data[11];wheel.csc(data,cadence.count(),cadence.time());
      auto packet=ble_hs_mbuf_from_flat(data,sizeof(data));
      if(packet&&!ble_gatts_notify_custom(connection,measurement->getHandle(),packet))sentAt=millis();
    }
    if(started&&connected&&powerSubscribed&&uint32_t(millis()-powerSentAt)>=1000){
      uint8_t data[16];powerPacket(data);
      auto packet=ble_hs_mbuf_from_flat(data,sizeof(data));
      if(packet&&!ble_gatts_notify_custom(connection,powerMeasurement->getHandle(),packet))powerSentAt=millis();
    }
  }
  void setResistance(int n){resistanceLevel=n;}
  float realCadence()const{return cadence.rpm(esp_timer_get_time());}
  int16_t realPower()const{return estimatedWatts(realCadence(),resistanceLevel);}
  void powerPacket(uint8_t data[16])const{wheel.power(data,realPower(),cadence.count(),cadence.time(),uint16_t(uint64_t(bluetoothJoules/1000)));}
  RideStats& ride(){return rideTotals;}
  uint32_t distanceMillimetres()const{return distanceMM;}
  double metresPerRevolution()const{return distanceMM/1000.0;}
  void setDistanceMM(uint32_t mm){if(mm>=100&&mm<=100000)distanceMM=mm;}
  float realSpeedKph()const{return realCadence()*metresPerRevolution()*0.06;}
  void saveRide(){
    Preferences p;if(p.begin("bike-ride",false)){auto data=rideTotals.data();if(p.putBytes("totals",&data,sizeof(data))==sizeof(data)){rideTotals.dirty=false;rideSavedAt=millis();}p.end();}
  }
  void newRide(){rideTotals.reset();saveRide();} // Watch counters remain monotonic.
  bool pairingVisible()const{return false;}
  const char* pairingPIN()const{return "";}
  uint16_t epoch()const{return inputEpoch;}
  bool bluetoothReady()const{return started;}
  bool cyclingReady()const{return started;}
  void crankPulse(uint32_t eventUs){
    uint64_t now=esp_timer_get_time();uint64_t event=now-uint32_t(uint32_t(now)-eventUs);
    if(cadence.pulse(event)){
      float rpm=cadence.rpm(event);int watts=estimatedWatts(rpm,resistanceLevel);
      double before=rideTotals.data().joules;rideTotals.pulse(event,rpm,watts,metresPerRevolution());bluetoothJoules+=rideTotals.data().joules-before;wheel.pulse(event,rpm,metresPerRevolution());
    }
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
    print(" ppr=");print(cadence.calibration());print(" feature_handle=");print(featureHandle);
    printf(" wheel=%lu control_subscribed=%d control_requests=%lu control_applied=%lu\n",(unsigned long)wheel.count(),int(controlSubscribed.load()),(unsigned long)controlRequests,(unsigned long)controlApplied);
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
