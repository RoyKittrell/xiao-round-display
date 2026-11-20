#!/usr/bin/env python3
"""Exercise the actual S3 canvas and atomic Sun cache under ASan/UBSan."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
heap=r'''
#pragma once
#include <cstdlib>
#include <cstdint>
#include <vector>
constexpr uint32_t MALLOC_CAP_8BIT=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_SPIRAM=4;
inline int allocations=0,failAt=0;
inline std::vector<uint32_t> capabilities;
inline void* heap_caps_malloc(size_t n,uint32_t caps){
 ++allocations;capabilities.push_back(caps);if(failAt==allocations)return nullptr;return std::malloc(n);
}
inline void* heap_caps_calloc(size_t n,size_t size,uint32_t caps){
 ++allocations;capabilities.push_back(caps);if(failAt==allocations)return nullptr;return std::calloc(n,size);
}
'''
gfx=r'''
#pragma once
#include <cstdlib>
#include <cstdint>
// Only the GFXcanvas16 allocation/ownership contract is needed by the subclass.
class GFXcanvas16 {
protected:
 const int16_t WIDTH,HEIGHT;uint16_t* buffer=nullptr;bool buffer_owned=false;
public:
 GFXcanvas16(uint16_t w,uint16_t h,bool allocate):WIDTH(w),HEIGHT(h){if(allocate){buffer=static_cast<uint16_t*>(calloc(w*h,2));buffer_owned=true;}}
 ~GFXcanvas16(){if(buffer_owned)free(buffer);}
 uint16_t* getBuffer()const{return buffer;}
};
'''
checks=r'''
#include <cassert>
#include <climits>
#include <cstdio>
#include <algorithm>
#include "DisplayCanvas.h"
#include "SunImageStore.h"
int main(){
 {
  DisplayCanvas16 live(240,240),background(240,240);
  assert(!live.getBuffer()&&!background.getBuffer()&&allocations==0);
  assert(live.begin(false)&&background.begin(true));
  assert(capabilities[0]==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  assert(capabilities[1]==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  assert(live.getBuffer()[57599]==0&&background.getBuffer()[57599]==0);
  live.getBuffer()[57599]=0x1234;background.getBuffer()[57599]=0x5678;
  assert(live.begin(false)&&background.begin(true)&&allocations==2);
 }
 {
  DisplayCanvas16 canvas(240,240);failAt=allocations+1;
  assert(!canvas.begin(true)&&!canvas.getBuffer());failAt=0;assert(canvas.begin(true));
 }
 {
  SunImageStore failed;failAt=allocations+2;assert(!failed.begin());
  failAt=0;assert(failed.begin()); // Partial allocation failure must be retryable.
 }
 SunImageStore sun;
 std::vector<uint16_t> image(57600),next(57600),out(57602,0xbeef);
 for(int i=0;i<57600;i++){image[i]=uint16_t(i*3571+19);next[i]=uint16_t(~image[i]);}
 assert(!sun.load(out.data()+1)&&!sun.commit());assert(sun.begin());
 int fixed=allocations;assert(sun.start());
 for(int row=0;row<240;row++)for(int col=0;col<240;col+=32){
  int n=std::min(32,240-col);assert(sun.write(row,col,image.data()+row*240+col,n));
 }
 assert(sun.commit()&&!sun.commit());assert(sun.load(out.data()+1));
 assert(out.front()==0xbeef&&out.back()==0xbeef&&std::equal(image.begin(),image.end(),out.begin()+1));
 assert(!sun.write(0,0,image.data(),1));
 assert(sun.start());assert(sun.write(0,0,next.data(),240));
 assert(sun.load(out.data()+1)&&std::equal(image.begin(),image.end(),out.begin()+1));
 assert(!sun.write(-1,0,next.data(),1)&&!sun.write(240,0,next.data(),1));
 assert(!sun.write(0,-1,next.data(),1)&&!sun.write(0,240,next.data(),1));
 assert(!sun.write(0,239,next.data(),2)&&!sun.write(0,0,nullptr,1));
 assert(!sun.write(0,0,next.data(),0)&&!sun.write(0,0,next.data(),INT_MAX));
 sun.cancel();assert(!sun.commit());
 assert(sun.load(out.data()+1)&&std::equal(image.begin(),image.end(),out.begin()+1));
 for(int cycle=0;cycle<30;cycle++){
  const auto& source=cycle%2?image:next;assert(sun.start());
  for(int row=0;row<240;row++)assert(sun.write(row,0,source.data()+row*240,240));
  assert(sun.commit()&&sun.load(out.data()+1));
  assert(std::equal(source.begin(),source.end(),out.begin()+1));
  assert(out.front()==0xbeef&&out.back()==0xbeef&&allocations==fixed);
 }
 puts("PASS S3 memory: deferred/internal/PSRAM canvas allocation, allocation-failure retry, guarded Sun transfers, atomic commit/cancel, bounded lifetime allocations.");
}
'''
with tempfile.TemporaryDirectory(prefix='xiao-s3-memory-') as directory:
 p=Path(directory);(p/'esp_heap_caps.h').write_text(heap);(p/'Adafruit_GFX.h').write_text(gfx)
 (p/'check.cpp').write_text(checks)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-DARDUINO_ARCH_ESP32','-DCONFIG_IDF_TARGET_ESP32S3','-I'+str(p),'-I'+str(ROOT/'XiaoPlayground'),str(p/'check.cpp'),'-o',str(p/'check')],check=True)
 subprocess.run([str(p/'check')],check=True)
