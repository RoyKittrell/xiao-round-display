#pragma once
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_app_desc.h>
#include <esp_flash.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#ifndef XIAO_DEVICE_PROFILE
#define XIAO_DEVICE_PROFILE "desktop"
#endif
// Uses the provisioned TLS certificate and controller token, never plaintext OTA.
class WifiOta {
 static inline volatile bool busy=false;
 static inline volatile uint32_t restartAt=0;
 static bool writeAll(mbedtls_ssl_context* ssl,const char* text){
  size_t left=strlen(text);uint32_t end=millis()+10000;
  while(left&&int32_t(end-millis())>0){int n=mbedtls_ssl_write(ssl,(const uint8_t*)text,left);if(n>0){text+=n;left-=n;}else if(n!=MBEDTLS_ERR_SSL_WANT_READ&&n!=MBEDTLS_ERR_SSL_WANT_WRITE)return false;vTaskDelay(1);}return !left;
 }
 static int readSome(mbedtls_ssl_context* ssl,uint8_t* data,size_t n,uint32_t end){
  while(int32_t(end-millis())>0){int r=mbedtls_ssl_read(ssl,data,n);if(r>0)return r;if(r!=MBEDTLS_ERR_SSL_WANT_READ&&r!=MBEDTLS_ERR_SSL_WANT_WRITE)return -1;vTaskDelay(1);}return -1;
 }
 static bool line(mbedtls_ssl_context* ssl,char* out,size_t capacity){
  size_t n=0;uint32_t end=millis()+15000;
  while(n+1<capacity){uint8_t c;if(readSome(ssl,&c,1,end)!=1)return false;if(c=='\n'){out[n]=0;return true;}if(c<32||c>126)return false;out[n++]=c;}return false;
 }
 static void hex(const uint8_t* data,char out[65]){for(int n=0;n<32;n++)sprintf(out+n*2,"%02x",data[n]);out[64]=0;}
 static bool digestText(const char* s){if(strlen(s)!=64)return false;for(int n=0;n<64;n++)if(!((s[n]>='0'&&s[n]<='9')||(s[n]>='a'&&s[n]<='f')))return false;return true;}
 static bool layout(char out[65]){
  uint8_t block[256],hash[32];mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts(&sha,0);
  bool ok=true;for(size_t n=0;n<3072;n+=sizeof(block)){if(esp_flash_read(nullptr,block,CONFIG_PARTITION_TABLE_OFFSET+n,sizeof(block))!=ESP_OK){ok=false;break;}mbedtls_sha256_update(&sha,block,sizeof(block));}
  mbedtls_sha256_finish(&sha,hash);mbedtls_sha256_free(&sha);hex(hash,out);return ok;
 }
 static bool markerPresent(const esp_partition_t* part,size_t size){
  const char* marker="XIAO-PROFILE:" XIAO_DEVICE_PROFILE ":v1";
  char block[512+64]={};size_t carry=0;
  for(size_t n=0;n<size;){size_t count=min(size-n,size_t(512));if(esp_partition_read(part,n,block+carry,count)!=ESP_OK)return false;size_t total=carry+count;
   for(size_t p=0;p+strlen(marker)<=total;p++)if(!memcmp(block+p,marker,strlen(marker)))return true;
   carry=min(strlen(marker)-1,total);memmove(block,block+total-carry,carry);n+=count;
  }return false;
 }
public:
 static inline void (*beforeRestart)()=nullptr;
 static bool acknowledge(mbedtls_ssl_context* ssl){return writeAll(ssl,"OK\n");}
 static bool active(){return busy;}
 static bool rebootDue(){return restartAt&&int32_t(millis()-restartAt)>=0;}
 static void service(){if(rebootDue()){if(beforeRestart)beforeRestart();ESP.restart();}}
 static void status(Print& p){
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  char elf[65];hex(esp_app_get_description()->app_elf_sha256,elf);
  p.printf("OTA supported=1 version=1 profile=%s elf=%s address=0x%lx busy=%d\n",XIAO_DEVICE_PROFILE,elf,(unsigned long)esp_ota_get_running_partition()->address,busy);
#else
  p.println("OTA supported=0");
#endif
 }
 static void serve(mbedtls_ssl_context* ssl){
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  const esp_partition_t* running=esp_ota_get_running_partition();const esp_partition_t* next=esp_ota_get_next_update_partition(nullptr);
  char partitionHash[65],elf[65],hello[360];hex(esp_app_get_description()->app_elf_sha256,elf);
  uint64_t mac=ESP.getEfuseMac();char serial[13];snprintf(serial,sizeof(serial),"%02x%02x%02x%02x%02x%02x",int(mac&255),int((mac>>8)&255),int((mac>>16)&255),int((mac>>24)&255),int((mac>>32)&255),int((mac>>40)&255));
  if(!next||!running||next->address==running->address||!layout(partitionHash)){writeAll(ssl,"ERR layout\n");return;}
  snprintf(hello,sizeof(hello),"OTA version=1 family=esp32s3 profile=%s serial=%s elf=%s partitions=%s slot=%s capacity=%lu marker=XIAO-PROFILE:%s:v1\n",XIAO_DEVICE_PROFILE,serial,elf,partitionHash,running->label,(unsigned long)next->size,XIAO_DEVICE_PROFILE);
  if(!writeAll(ssl,hello))return;
  char request[300],shaText[65],layoutText[65],profile[16],expectedElf[65],extra;unsigned long size=0;
  if(!line(ssl,request,sizeof(request)))return;
  if(busy){writeAll(ssl,"ERR busy\n");return;}
  // Closing after HELLO is a read-only availability/build check.
  if(sscanf(request,"UPDATE 1 %lu %64s %64s %15s %64s %c",&size,shaText,layoutText,profile,expectedElf,&extra)!=5||!digestText(shaText)||!digestText(layoutText)||!digestText(expectedElf)||strcmp(layoutText,partitionHash)||strcmp(profile,XIAO_DEVICE_PROFILE)||size<256||size>next->size){writeAll(ssl,"ERR request\n");return;}
  esp_ota_handle_t handle=0;if(esp_ota_begin(next,size,&handle)!=ESP_OK){writeAll(ssl,"ERR begin\n");return;}
  busy=true;bool ok=writeAll(ssl,"READY\n");mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts(&sha,0);
  uint8_t block[2048];size_t received=0;uint32_t totalEnd=millis()+180000;
  while(ok&&received<size&&int32_t(totalEnd-millis())>0){int n=readSome(ssl,block,min(size_t(size-received),sizeof(block)),millis()+15000);if(n<=0){ok=false;break;}if(esp_ota_write(handle,block,n)!=ESP_OK){ok=false;break;}mbedtls_sha256_update(&sha,block,n);received+=n;}
  uint8_t digest[32];char actual[65];mbedtls_sha256_finish(&sha,digest);mbedtls_sha256_free(&sha);hex(digest,actual);
  if(!ok||received!=size||strcmp(actual,shaText)||!markerPresent(next,size)){esp_ota_abort(handle);busy=false;writeAll(ssl,"ERR incomplete-or-hash\n");return;}
  if(esp_ota_end(handle)!=ESP_OK){busy=false;writeAll(ssl,"ERR image\n");return;}
  esp_app_desc_t description={};char newElf[65];
  if(esp_ota_get_partition_description(next,&description)!=ESP_OK){busy=false;writeAll(ssl,"ERR description\n");return;}
  hex(description.app_elf_sha256,newElf);
  if(strcmp(newElf,expectedElf)||esp_ota_set_boot_partition(next)!=ESP_OK){busy=false;writeAll(ssl,"ERR activation\n");return;}
  restartAt=millis()+1800;writeAll(ssl,"COMMITTED\n");
#else
  writeAll(ssl,"ERR unsupported\n");
#endif
 }
};
