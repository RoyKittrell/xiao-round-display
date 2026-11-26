#pragma once
#include <Arduino.h>
#include <Wire.h>
#if defined(ARDUINO_ARCH_ESP32)
#if !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#error Select the XIAO ESP32-C6 or XIAO ESP32-S3 board profile.
#endif
#include <Preferences.h>
#include <driver/gpio.h>
constexpr uint8_t PIN_D1=D1,PIN_D3=D3,PIN_D4=D4,PIN_D5=D5,PIN_D6=D6,PIN_D7=D7,PIN_D8=D8,PIN_D10=D10;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr uint8_t PIN_BUTTON=0;
#else
constexpr uint8_t PIN_BUTTON=9;
#endif
namespace Board {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr bool Esp32C6=false;
constexpr const char* Name="ESP32-S3";
constexpr const char* BluetoothName="XIAO Round Display S3";
constexpr uint32_t FrameIntervalMs=33;
#else
constexpr bool Esp32C6=true;
constexpr const char* Name="ESP32-C6";
constexpr const char* BluetoothName="XIAO Round Display C6";
constexpr uint32_t FrameIntervalMs=40;
#endif
inline uint8_t loadTouch(){Preferences p;if(!p.begin("xiao-display",true))return 0;auto v=p.getUChar("touch-map",0);p.end();return v<8?v:0;}
inline void saveTouch(uint8_t v){Preferences p;if(p.begin("xiao-display",false)){p.putUChar("touch-map",v);p.end();}}
inline uint8_t loadRotation(){Preferences p;if(!p.begin("xiao-display",true))return 0;auto v=p.getUChar("screen-turns",0);p.end();return v<4?v:0;}
inline void saveRotation(uint8_t v){Preferences p;if(p.begin("xiao-display",false)){p.putUChar("screen-turns",v);p.end();}}
inline void pwmBegin(){analogWriteResolution(PIN_D6,12);analogWriteFrequency(PIN_D6,2000);}
inline void setupRadioAntenna(){
#if defined(CONFIG_IDF_TARGET_ESP32C6)
 pinMode(WIFI_ENABLE,OUTPUT);digitalWrite(WIFI_ENABLE,LOW);pinMode(WIFI_ANT_CONFIG,OUTPUT);digitalWrite(WIFI_ANT_CONFIG,LOW);
#endif
 // S3 uses the attached U.FL antenna, with no RF selection GPIOs.
}
inline int pinLevel(uint8_t pin){return gpio_get_level(gpio_num_t(pin));}
inline void beginDisplayBus(){Wire.setPins(PIN_D4,PIN_D5);Wire.setTimeOut(20);Wire.begin(PIN_D4,PIN_D5,100000);}
}
#else
#include "npm1300.h"
namespace Board {
constexpr bool Esp32C6=false;
constexpr uint32_t FrameIntervalMs=40;
inline uint8_t loadTouch(){return 0;}
inline void saveTouch(uint8_t){}
inline uint8_t loadRotation(){return 0;}
inline void saveRotation(uint8_t){}
inline void pwmBegin(){analogWriteResolution(12);analogWriteFrequency(2000);}
inline void setupRadioAntenna(){}
inline int pinLevel(uint8_t pin){return digitalRead(pin);}
inline void beginDisplayBus(){Wire.setPins(PIN_D4,PIN_D5);Wire.begin();Wire.setClock(100000);pinMode(PIN_D4,INPUT_PULLUP);pinMode(PIN_D5,INPUT_PULLUP);}
}
#endif
