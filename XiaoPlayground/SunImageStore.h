#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include <esp_heap_caps.h>
#include <string.h>

// Keep committed and incoming images separate, so a truncated transfer cannot
// damage the last complete frame. Allocate once; no per-row/frame allocation.
class SunImageStore {
 static constexpr size_t Pixels=240*240;
 uint16_t* current=nullptr;
 uint16_t* pending=nullptr;
 bool receiving=false,ready=false;
public:
 ~SunImageStore(){free(current);free(pending);}
 SunImageStore()=default;
 SunImageStore(const SunImageStore&)=delete;
 SunImageStore& operator=(const SunImageStore&)=delete;
 bool begin(){
  if(current&&pending)return true;
  auto a=static_cast<uint16_t*>(heap_caps_malloc(Pixels*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  auto b=static_cast<uint16_t*>(heap_caps_malloc(Pixels*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!a||!b){free(a);free(b);return false;}
  current=a;pending=b;return true;
 }
 bool start(){if(!begin())return false;receiving=true;return true;}
 bool write(int row,int column,const uint16_t* pixels,int count){
  if(!receiving||row<0||row>=240||column<0||column>=240||count<=0||count>240-column||!pixels)return false;
  memcpy(pending+row*240+column,pixels,size_t(count)*sizeof(uint16_t));return true;
 }
 // The packet receiver verifies all 240 row bitmaps before calling commit().
 bool commit(){if(!receiving)return false;auto previous=current;current=pending;pending=previous;receiving=false;ready=true;return true;}
 void cancel(){receiving=false;}
 bool load(uint16_t* out){if(!ready||!out)return false;memcpy(out,current,Pixels*sizeof(uint16_t));return true;}
};
#else
#include <LittleFS.h>
// The C6 has no PSRAM. Store a downloaded Sun image in the wear-levelled
// filesystem, then lend the existing scene-background canvas when it is shown.
// A complete transfer is committed by rename; incomplete data is never shown.
class SunImageStore {
 File incoming;bool mounted=false;
public:
 bool begin(){if(!mounted)mounted=LittleFS.begin(true);return mounted;}
 bool start(){if(!begin())return false;if(incoming)incoming.close();LittleFS.remove("/sun.pending");incoming=LittleFS.open("/sun.pending","w");return bool(incoming);}
 bool write(int row,int column,const uint16_t* pixels,int count){
  if(!incoming||row<0||row>=240||column<0||column+count>240)return false;
  size_t offset=(row*240+column)*2;if(incoming.position()!=offset&&!incoming.seek(offset))return false;
  return incoming.write(reinterpret_cast<const uint8_t*>(pixels),count*2)==size_t(count*2);
 }
 bool commit(){if(!incoming)return false;incoming.flush();bool valid=incoming.size()==240*240*2;incoming.close();if(!valid)return false;
  LittleFS.remove("/sun.previous");if(LittleFS.exists("/sun.current"))LittleFS.rename("/sun.current","/sun.previous");
  if(!LittleFS.rename("/sun.pending","/sun.current")){LittleFS.rename("/sun.previous","/sun.current");return false;}
  LittleFS.remove("/sun.previous");return true;
 }
 void cancel(){if(incoming)incoming.close();}
 bool load(uint16_t* out){if(!mounted)return false;File f=LittleFS.open("/sun.current","r");if(!f||f.size()!=240*240*2)return false;return f.read(reinterpret_cast<uint8_t*>(out),240*240*2)==240*240*2;}
};
#endif
#endif
