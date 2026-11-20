#pragma once
#include <Adafruit_GFX.h>
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include <esp_heap_caps.h>

// Global constructors run before Arduino's PSRAM setup. Allocate explicitly
// from setup(): callers choose internal SRAM or external PSRAM explicitly.
class DisplayCanvas16:public GFXcanvas16 {
public:
 DisplayCanvas16(uint16_t w,uint16_t h):GFXcanvas16(w,h,false){}
 bool begin(bool external){
  if(buffer)return true;
  uint32_t caps=MALLOC_CAP_8BIT|(external?MALLOC_CAP_SPIRAM:MALLOC_CAP_INTERNAL);
  buffer=static_cast<uint16_t*>(heap_caps_calloc(size_t(WIDTH)*HEIGHT,sizeof(uint16_t),caps));
  buffer_owned=buffer!=nullptr;return buffer!=nullptr;
 }
};
#else
using DisplayCanvas16=GFXcanvas16;
#endif
