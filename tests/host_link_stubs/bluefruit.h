#pragma once
#include "Arduino.h"
#include <string>
inline constexpr int ERROR_NONE=0,BANDWIDTH_MAX=0,BANDWIDTH_NORMAL=1,SECMODE_NO_ACCESS=0,SECMODE_ENC_WITH_MITM=3,CHR_PROPS_READ=4,CHR_PROPS_WRITE=1,CHR_PROPS_NOTIFY=2,BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE=1;
struct BLEService{BLEService(const char*){}int begin(){return 0;}};
struct BLECharacteristic {
 using Callback=void(*)(uint16_t,BLECharacteristic*,uint8_t*,uint16_t);
 static inline BLECharacteristic* rx=nullptr;static inline std::vector<uint8_t> notifications;
 struct Handles{int value_handle=1;};Handles handles(){return Handles{};}
 int readPerm=-1,writePerm=-1;Callback callback=nullptr;
 BLECharacteristic(const char* id){if(std::string(id).find("82a64c01")==0)rx=this;}
 void setProperties(int){}void setPermission(int r,int w){readPerm=r;writePerm=w;}void setMaxLen(int){}void setWriteCallback(Callback c){callback=c;}int begin(){return 0;}bool notifyEnabled(){return true;}
 bool notify(uint8_t* p,uint16_t n){notifications.insert(notifications.end(),p,p+n);return true;}
};
struct Security {
 bool auth=false;bool(*passkey)(uint16_t,const uint8_t*,bool)=nullptr;void(*secured)(uint16_t)=nullptr;
 bool isAuthenticated(){return auth;}void setIOCaps(bool,bool,bool){}void setMITM(bool){}void setSecureConnectionsRequired(bool){}
 void setPairPasskeyCallback(decltype(passkey) p){passkey=p;}void setSecuredCallback(decltype(secured) p){secured=p;}
};
struct Peripheral{void setConnIntervalMS(int,int){}void(*connect)(uint16_t)=nullptr;void setConnectCallback(decltype(connect) p){connect=p;}void(*disconnect)(uint16_t,uint8_t)=nullptr;void setDisconnectCallback(decltype(disconnect) p){disconnect=p;}};
struct Advertising{void addFlags(int){}void addService(BLEService&){}void restartOnDisconnect(bool){}void setInterval(int,int){}void setFastTimeout(int){}bool start(int){return true;}void addName(){}};
struct Radio{struct Security Security;struct Peripheral Periph;struct Advertising Advertising,ScanResponse;bool link=false;bool connected(){return link;}void autoConnLed(bool){}void configPrphBandwidth(int){}bool begin(int,int){return true;}void setName(const char*){}void setTxPower(int){}};
inline Radio Bluefruit;
