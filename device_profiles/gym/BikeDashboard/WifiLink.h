#pragma once
// ESP32-only station setup + certificate-pinned TLS stream. Provisioned over
// USB; neither credentials nor private key are compiled into shared firmware.
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <lwip/tcp.h>
#include <fcntl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include "WifiOta.h"
class WifiLink {
 struct Config {uint32_t version;char ssid[33],password[65],token[65];uint16_t certSize,keySize;uint8_t cert[640],key[256];} config={};
 portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
 uint8_t in[2048]={},out[4096]={};uint16_t ri=0,re=0,wi=0,we=0;
 volatile uint16_t generation=0;volatile bool online=false,session=false;volatile int tlsError=0;
 bool configured=false,mdns=false;uint32_t retryAt=0,taskAttempt=0;TaskHandle_t task=nullptr,otaTask=nullptr;
 volatile bool otaOnline=false;uint32_t otaAttempt=0;
 char hostname[40]={};
 static int rng(void*,unsigned char* p,size_t n){esp_fill_random(p,n);return 0;}
 static int sendTLS(void* ctx,const unsigned char* p,size_t n){int r=send(*static_cast<int*>(ctx),p,n,0);if(r<0)return errno==EAGAIN||errno==EWOULDBLOCK?MBEDTLS_ERR_SSL_WANT_WRITE:MBEDTLS_ERR_NET_SEND_FAILED;return r;}
 static int recvTLS(void* ctx,unsigned char* p,size_t n){int r=recv(*static_cast<int*>(ctx),p,n,0);if(r<0)return errno==EAGAIN||errno==EWOULDBLOCK?MBEDTLS_ERR_SSL_WANT_READ:MBEDTLS_ERR_NET_RECV_FAILED;return r;}
 static void entry(void* p){static_cast<WifiLink*>(p)->run();}
 void clear(){portENTER_CRITICAL(&mux);session=false;ri=re=wi=we=0;++generation;portEXIT_CRITICAL(&mux);}
 static void otaEntry(void* p){static_cast<WifiLink*>(p)->run(true);}
 void run(bool update=false){
  mbedtls_x509_crt cert;mbedtls_pk_context key;mbedtls_ssl_config cfg;
  mbedtls_x509_crt_init(&cert);mbedtls_pk_init(&key);mbedtls_ssl_config_init(&cfg);
  int result=mbedtls_x509_crt_parse_der(&cert,config.cert,config.certSize);
  if(!result)result=mbedtls_pk_parse_key(&key,config.key,config.keySize,nullptr,0,rng,nullptr);
  if(!result)result=mbedtls_ssl_config_defaults(&cfg,MBEDTLS_SSL_IS_SERVER,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
  if(!result){mbedtls_ssl_conf_rng(&cfg,rng,nullptr);mbedtls_ssl_conf_authmode(&cfg,MBEDTLS_SSL_VERIFY_NONE);mbedtls_ssl_conf_min_tls_version(&cfg,MBEDTLS_SSL_VERSION_TLS1_2);result=mbedtls_ssl_conf_own_cert(&cfg,&cert,&key);}
  int listener=-1;
  if(!result){listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener>=0){int yes=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));sockaddr_in addr={};addr.sin_family=AF_INET;addr.sin_port=htons(update?7444:7443);addr.sin_addr.s_addr=INADDR_ANY;if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))||listen(listener,1)){close(listener);listener=-1;}else fcntl(listener,F_SETFL,O_NONBLOCK);}}
  if(update)otaOnline=listener>=0;else{tlsError=result;online=listener>=0;}
  while(listener>=0){
   int fd=accept(listener,nullptr,nullptr);if(fd<0){vTaskDelay(pdMS_TO_TICKS(30));continue;}
   int noDelay=1;setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&noDelay,sizeof(noDelay));
   fcntl(fd,F_SETFL,O_NONBLOCK);mbedtls_ssl_context ssl;mbedtls_ssl_init(&ssl);
   result=mbedtls_ssl_setup(&ssl,&cfg);mbedtls_ssl_set_bio(&ssl,&fd,sendTLS,recvTLS,nullptr);
   uint32_t deadline=millis()+12000;
   if(!result)do{result=mbedtls_ssl_handshake(&ssl);if(result==MBEDTLS_ERR_SSL_WANT_READ||result==MBEDTLS_ERR_SSL_WANT_WRITE)vTaskDelay(pdMS_TO_TICKS(5));else break;}while(int32_t(deadline-millis())>0&&WiFi.status()==WL_CONNECTED);
   tlsError=result;bool authorized=false;char auth[72]={};size_t count=0;
   deadline=millis()+5000;
   if(!result)while(int32_t(deadline-millis())>0&&count<sizeof(auth)-1){
    unsigned char c;int n=mbedtls_ssl_read(&ssl,&c,1);
    if(n==1){if(c=='\n'){uint8_t diff=0;for(size_t i=0;i<64;i++)diff|=uint8_t(auth[i+5]^config.token[i]);authorized=count==69&&!memcmp(auth,"AUTH ",5)&&diff==0;break;}auth[count++]=char(c);}
    else if(n==MBEDTLS_ERR_SSL_WANT_READ||n==MBEDTLS_ERR_SSL_WANT_WRITE)vTaskDelay(pdMS_TO_TICKS(5));else break;
   }
   memset(auth,0,sizeof(auth));
   if(update){
    if(authorized&&WifiOta::acknowledge(&ssl))WifiOta::serve(&ssl);
    shutdown(fd,SHUT_RDWR);close(fd);mbedtls_ssl_free(&ssl);vTaskDelay(pdMS_TO_TICKS(30));continue;
   }
   clear();
   if(authorized){portENTER_CRITICAL(&mux);out[0]='O';out[1]='K';out[2]='\n';we=3;session=true;portEXIT_CRITICAL(&mux);}
   uint8_t tx[512],rx[512];size_t pending=0;uint32_t last=millis();
   while(authorized&&WiFi.status()==WL_CONNECTED&&uint32_t(millis()-last)<45000){
    if(!pending){
     // Print writes one byte at a time. Send whole reply lines together so a
     // task switch midway through ACK cannot create tiny delayed TLS records.
     portENTER_CRITICAL(&mux);uint16_t next=wi;size_t count=0;bool complete=false;
     while(next!=we&&count<sizeof(tx)){tx[count++]=out[next];next=(next+1)%sizeof(out);if(tx[count-1]=='\n'){complete=true;break;}}
     if(complete||count==sizeof(tx)){pending=count;wi=next;}portEXIT_CRITICAL(&mux);
    }
    if(pending){int n=mbedtls_ssl_write(&ssl,tx,pending);if(n>0){pending-=n;if(pending)memmove(tx,tx+n,pending);}else if(n!=MBEDTLS_ERR_SSL_WANT_READ&&n!=MBEDTLS_ERR_SSL_WANT_WRITE)break;}
    size_t room;portENTER_CRITICAL(&mux);room=(ri+sizeof(in)-re-1)%sizeof(in);portEXIT_CRITICAL(&mux);
    if(room){int n=mbedtls_ssl_read(&ssl,rx,min(room,sizeof(rx)));if(n>0){last=millis();portENTER_CRITICAL(&mux);for(int i=0;i<n;i++){in[re]=rx[i];re=(re+1)%sizeof(in);}portEXIT_CRITICAL(&mux);}else if(n!=MBEDTLS_ERR_SSL_WANT_READ&&n!=MBEDTLS_ERR_SSL_WANT_WRITE)break;}
    vTaskDelay(pdMS_TO_TICKS(2));
   }
   clear();shutdown(fd,SHUT_RDWR);close(fd);mbedtls_ssl_free(&ssl);vTaskDelay(pdMS_TO_TICKS(50));
  }
  mbedtls_ssl_config_free(&cfg);mbedtls_pk_free(&key);mbedtls_x509_crt_free(&cert);if(update)otaTask=nullptr;else task=nullptr;vTaskDelete(nullptr);
 }
 static int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
 static int unhex(const char* src,uint8_t* dst,size_t cap){size_t n=strlen(src);if(n%2||n/2>cap)return -1;for(size_t i=0;i<n/2;i++){int a=nibble(src[i*2]),b=nibble(src[i*2+1]);if(a<0||b<0)return -1;dst[i]=(a<<4)|b;}return n/2;}
public:
 void begin(){
  uint64_t mac=ESP.getEfuseMac();snprintf(hostname,sizeof(hostname),"xiao-display-%02x%02x%02x",int((mac>>24)&255),int((mac>>32)&255),int((mac>>40)&255));
  Preferences prefs;if(prefs.begin("xiao-wifi",true)){if(prefs.getBytesLength("config")==sizeof(config))prefs.getBytes("config",&config,sizeof(config));prefs.end();}
  configured=config.version==1&&config.ssid[0]&&config.certSize>0&&config.certSize<=sizeof(config.cert)&&config.keySize>0&&config.keySize<=sizeof(config.key)&&strnlen(config.token,sizeof(config.token))==64&&config.ssid[32]==0&&config.password[64]==0;
  if(configured){WiFi.persistent(false);WiFi.setHostname(hostname);WiFi.mode(WIFI_STA);WiFi.begin(config.ssid,config.password);WiFi.setAutoReconnect(true);retryAt=millis();}
 }
 void service(){
  WifiOta::service();
  if(!configured)return;
  if(WiFi.status()==WL_CONNECTED){
   if(!mdns){mdns=MDNS.begin(hostname);if(mdns)MDNS.addService("xiao-display","tcp",7443);}
   if(!task&&!online&&(!taskAttempt||uint32_t(millis()-taskAttempt)>30000)){taskAttempt=millis();xTaskCreate(entry,"XIAO WiFi TLS",12288,this,1,&task);}
#if defined(CONFIG_IDF_TARGET_ESP32S3)
   if(!otaTask&&!otaOnline&&(!otaAttempt||uint32_t(millis()-otaAttempt)>30000)){otaAttempt=millis();xTaskCreate(otaEntry,"XIAO OTA TLS",16384,this,1,&otaTask);}
#endif
  }else{if(mdns){MDNS.end();mdns=false;}if(uint32_t(millis()-retryAt)>30000){retryAt=millis();WiFi.reconnect();}}
 }
 // Only a trusted physical USB client can replace provisioning, never a LAN peer.
 bool provision(char* text){
  char* last=strrchr(text,'|');if(!last||strlen(last+1)!=4)return false;uint16_t crc=0xffff;for(char* p=text;p<last;p++){crc^=uint16_t(uint8_t(*p))<<8;for(int b=0;b<8;b++)crc=(crc&0x8000)?(crc<<1)^0x1021:crc<<1;}char* end=nullptr;unsigned long supplied=strtoul(last+1,&end,16);if(*end||supplied!=crc)return false;*last=0;
  char* parts[7]={};int n=0;char* save=nullptr;for(char* p=strtok_r(text,"|",&save);p&&n<7;p=strtok_r(nullptr,"|",&save))parts[n++]=p;
  if(n!=7||strcmp(parts[0],"N")||strcmp(parts[1],"1"))return false;
  Config next={};next.version=1;int ssid=unhex(parts[2],reinterpret_cast<uint8_t*>(next.ssid),32);int pass=unhex(parts[3],reinterpret_cast<uint8_t*>(next.password),64);
  int cert=unhex(parts[5],next.cert,sizeof(next.cert)),key=unhex(parts[6],next.key,sizeof(next.key));
  if(ssid<1||pass<8||cert<1||key<1||strlen(parts[4])!=64||memchr(next.ssid,0,ssid)||memchr(next.password,0,pass))return false;
  uint8_t check[32];if(unhex(parts[4],check,32)!=32)return false;strcpy(next.token,parts[4]);next.certSize=cert;next.keySize=key;
  Preferences prefs;bool ok=prefs.begin("xiao-wifi",false);if(ok){ok=prefs.putBytes("config",&next,sizeof(next))==sizeof(next);prefs.end();}memset(&next,0,sizeof(next));return ok;
 }
 void status(Print& p){WifiOta::status(p);p.print("WIFI configured=");p.print(configured);p.print(" connected=");p.print(WiFi.status()==WL_CONNECTED);p.print(" host=");p.print(hostname);p.print(".local ip=");p.print(WiFi.localIP());p.print(" rssi=");p.print(WiFi.status()==WL_CONNECTED?WiFi.RSSI():0);p.print(" tls=");p.print(online);p.print(" authenticated=");p.print(session);p.print(" tls_error=");p.println(tlsError);}
 uint16_t epoch()const{return generation;}
 int available(){portENTER_CRITICAL(&mux);int n=(re+sizeof(in)-ri)%sizeof(in);portEXIT_CRITICAL(&mux);return n;}
 int read(){portENTER_CRITICAL(&mux);int c=-1;if(ri!=re){c=in[ri];ri=(ri+1)%sizeof(in);}portEXIT_CRITICAL(&mux);return c;}
 void write(uint8_t c){portENTER_CRITICAL(&mux);if(session){auto next=(we+1)%sizeof(out);if(next!=wi){out[we]=c;we=next;}}portEXIT_CRITICAL(&mux);}
};
