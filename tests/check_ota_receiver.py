"""Compile the actual firmware receiver with fake partitions/TLS and real SHA256."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='xiao-ota-receiver-') as tmp:
 p=Path(tmp);(p/'mbedtls').mkdir()
 (p/'Arduino.h').write_text(r'''#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>
using std::min;
#define CONFIG_IDF_TARGET_ESP32S3 1
#define CONFIG_PARTITION_TABLE_OFFSET 0x8000
#define XIAO_DEVICE_PROFILE "gym"
uint32_t ticks=1;inline uint32_t millis(){return ticks++;}inline void vTaskDelay(int){}
struct Print{template<class...T>void printf(const char*,T...){}void println(const char*){}};
struct Esp{uint64_t getEfuseMac(){return 0x554433221100ULL;}void restart(){}}ESP;
''')
 (p/'esp_app_desc.h').write_text('''#pragma once
#include <cstdint>
struct esp_app_desc_t {uint8_t app_elf_sha256[32];};
inline const esp_app_desc_t* esp_app_get_description(){static esp_app_desc_t d={};return &d;}
''')
 (p/'esp_ota_ops.h').write_text(r'''#pragma once
#include "Arduino.h"
#include "esp_app_desc.h"
#define ESP_OK 0
struct esp_partition_t{uint32_t address,size;const char* label;};
esp_partition_t running={0x10000,0x330000,"ota_0"},next={0x340000,0x330000,"ota_1"};
using esp_ota_handle_t=uint32_t;
std::vector<uint8_t> flashed;int begins=0,aborts=0,ends=0,activations=0;bool writeFails=false,imageFails=false;
inline const esp_partition_t* esp_ota_get_running_partition(){return &running;}
inline const esp_partition_t* esp_ota_get_next_update_partition(void*){return &next;}
inline int esp_ota_begin(const esp_partition_t*,size_t,esp_ota_handle_t* h){++begins;flashed.clear();*h=1;return 0;}
inline int esp_ota_write(esp_ota_handle_t,const void* d,size_t n){if(writeFails)return 1;auto b=(const uint8_t*)d;flashed.insert(flashed.end(),b,b+n);return 0;}
inline int esp_ota_abort(esp_ota_handle_t){++aborts;return 0;}
inline int esp_ota_end(esp_ota_handle_t){++ends;return imageFails?1:0;}
inline int esp_ota_set_boot_partition(const esp_partition_t*){++activations;return 0;}
inline int esp_partition_read(const esp_partition_t*,size_t off,void* d,size_t n){if(off+n>flashed.size())return 1;memcpy(d,flashed.data()+off,n);return 0;}
inline int esp_ota_get_partition_description(const esp_partition_t*,esp_app_desc_t* d){memset(d->app_elf_sha256,0x42,32);return 0;}
''')
 (p/'esp_flash.h').write_text('''#pragma once
inline int esp_flash_read(void*,void* out,unsigned,size_t n){memset(out,0xff,n);return 0;}
''')
 (p/'mbedtls/ssl.h').write_text(r'''#pragma once
#define MBEDTLS_ERR_SSL_WANT_READ -2
#define MBEDTLS_ERR_SSL_WANT_WRITE -3
struct mbedtls_ssl_context{std::string input,output;size_t at=0;};
inline int mbedtls_ssl_read(mbedtls_ssl_context* s,unsigned char* out,size_t n){n=min(n,min(size_t(17),s->input.size()-s->at));if(!n)return 0;memcpy(out,s->input.data()+s->at,n);s->at+=n;return int(n);}
inline int mbedtls_ssl_write(mbedtls_ssl_context* s,const unsigned char* d,size_t n){n=min(n,size_t(13));s->output.append((const char*)d,n);return int(n);}
''')
 (p/'mbedtls/sha256.h').write_text(r'''#pragma once
#include <CommonCrypto/CommonDigest.h>
using mbedtls_sha256_context=CC_SHA256_CTX;
inline void mbedtls_sha256_init(mbedtls_sha256_context*){}
inline void mbedtls_sha256_free(mbedtls_sha256_context*){}
inline int mbedtls_sha256_starts(mbedtls_sha256_context* c,int){return CC_SHA256_Init(c);}
inline int mbedtls_sha256_update(mbedtls_sha256_context* c,const unsigned char* p,size_t n){return CC_SHA256_Update(c,p,CC_LONG(n));}
inline int mbedtls_sha256_finish(mbedtls_sha256_context* c,unsigned char* p){return CC_SHA256_Final(p,c);}
''')
 (p/'test.cpp').write_text(r'''
#include <cassert>
#include "WifiOta.h"
std::string hash(const std::string& s){uint8_t out[32];CC_SHA256(s.data(),CC_LONG(s.size()),out);char hex[65];for(int n=0;n<32;n++)sprintf(hex+n*2,"%02x",out[n]);return hex;}
void reset(){begins=aborts=ends=activations=0;writeFails=imageFails=false;}
int main(){
 std::string data(3000,'x');data.replace(500,19,"XIAO-PROFILE:gym:v1");
 auto layout=hash(std::string(3072,char(0xff)));auto digest=hash(data);std::string elf;for(int n=0;n<32;n++)elf+="42";
 auto request=[&](std::string sha,std::string profile="gym"){return "UPDATE 1 "+std::to_string(data.size())+" "+sha+" "+layout+" "+profile+" "+elf+"\n";};

 reset();mbedtls_ssl_context badHash{request(std::string(64,'0'))+data};WifiOta::serve(&badHash);assert(activations==0&&aborts==1&&!WifiOta::active());
 reset();mbedtls_ssl_context partial{request(digest)+data.substr(0,100)};WifiOta::serve(&partial);assert(activations==0&&aborts==1);
 reset();mbedtls_ssl_context wrongProfile{request(digest,"desktop")+data};WifiOta::serve(&wrongProfile);assert(begins==0&&activations==0);
 reset();auto missing=data;missing.replace(500,18,"not-a-gym-image!!!!");mbedtls_ssl_context marker{request(hash(missing))+missing};WifiOta::serve(&marker);assert(activations==0&&aborts==1);
 reset();writeFails=true;mbedtls_ssl_context writeError{request(digest)+data};WifiOta::serve(&writeError);assert(activations==0&&aborts==1);
 reset();imageFails=true;mbedtls_ssl_context invalid{request(digest)+data};WifiOta::serve(&invalid);assert(activations==0&&ends==1);
 reset();auto badLayout=request(digest);badLayout.replace(badLayout.find(layout),64,std::string(64,'1'));mbedtls_ssl_context mismatch{badLayout+data};WifiOta::serve(&mismatch);assert(begins==0&&activations==0);
 reset();mbedtls_ssl_context good{request(digest)+data};WifiOta::serve(&good);assert(activations==1&&aborts==0&&flashed.size()==data.size());assert(good.output.find("COMMITTED\n")!=std::string::npos);
 reset();mbedtls_ssl_context concurrent{request(digest)+data};WifiOta::serve(&concurrent);assert(begins==0&&activations==0);

}
''')
 subprocess.run(['clang++','-std=c++17','-Wno-deprecated-declarations','-fsanitize=address,undefined','-I'+str(p),'-I'+str(ROOT/'device_profiles/gym/BikeDashboard'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS actual OTA receiver: fragmentation, SHA256, profile/marker/layout rejection, partial transfers, write/image errors and activation ordering')
