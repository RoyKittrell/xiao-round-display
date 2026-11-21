"""Use the real Gym mount policy against synthetic partitions; no hardware."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)
 (p/'esp_partition.h').write_text('''#pragma once
#include <cstddef>
#include <cstring>
struct esp_partition_t {size_t size;};
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2,ESP_OK=0;
extern unsigned char bytes[512];extern esp_partition_t part;
inline const esp_partition_t* esp_partition_find_first(int,int,void*){return &part;}
inline int esp_partition_read(const esp_partition_t*,size_t offset,void* out,size_t length){memcpy(out,bytes+offset,length);return 0;}
''')
 (p/'test.cpp').write_text('''#include <cassert>
#include <cstdint>
#include "esp_partition.h"
unsigned char bytes[512];esp_partition_t part{512};
struct FS {bool mounted=false;int formats=0;bool begin(bool format){assert(!format);return mounted;}bool format(){++formats;mounted=true;return true;}} LittleFS;
#include "PedalStorage.h"
int main(){
 memset(bytes,0xff,sizeof(bytes));assert(mountPedalStorage());assert(LittleFS.formats==1);
 LittleFS={};bytes[300]=0;assert(!mountPedalStorage());assert(!LittleFS.formats);
 LittleFS.mounted=true;assert(mountPedalStorage());assert(!LittleFS.formats);
}
''')
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(p),'-I'+str(ROOT/'device_profiles/gym/BikeDashboard'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS existing filesystem preserved; only completely erased storage initialized')
