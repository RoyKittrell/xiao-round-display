#include <Arduino.h>
#include <LittleFS.h>
#include "PedalStorage.h"
namespace Board {
const char* Name="XIAO ESP32-S3 Gym";
char BluetoothName[24]="Gym Xiao";
}
#include "GymLink.h"
HostLink hostLink;
char provisionPacket[2400]={};
size_t provisionLength=0;
bool receivingProvision=false,commandOverflow=false;
uint16_t hostEpoch=0;

// Standard XIAO ESP32-S3: D2 = GPIO3. No SD card; original console disconnected.
constexpr int SENSOR_PIN = D2;
constexpr uint32_t DEBOUNCE_US = 5000;
constexpr size_t MAX_LOG_BYTES = 1024 * 1024;
const char *LOG_PATH = "/pedals.csv";
File logFile;
bool storageOK = false, logFull = false;
int candidate, stableState;
uint32_t candidateSince, lastClosure = 0, lastFlush = 0;
uint32_t closures = 0, transitions = 0;
bool haveClosure = false;
bool pulseIndicator = false;
uint32_t pulseIndicatorSince = 0;
uint32_t lastHeartbeat = 0;
String command;
struct SensorEvent {uint32_t timestamp; uint32_t interval; int level;};
QueueHandle_t sensorQueue;
volatile uint32_t queueDrops=0;
bool sensorTaskOK=false;
void record(const char *event,uint32_t timestamp,int level,uint32_t interval);
#include "DashboardUI.h"

void sensorTask(void*) {
  int sampled=digitalRead(SENSOR_PIN), accepted=sampled;
  uint32_t changed=micros(), last=0;bool haveLast=false;
  for(;;) {
    uint32_t now=micros();int level=digitalRead(SENSOR_PIN);
    if(level!=sampled){sampled=level;changed=now;}
    if(sampled!=accepted && uint32_t(now-changed)>=DEBOUNCE_US) {
      accepted=sampled;uint32_t interval=0;
      if(accepted==LOW){if(haveLast)interval=changed-last;last=changed;haveLast=true;}
      SensorEvent e{changed,interval,accepted};
      if(xQueueSend(sensorQueue,&e,0)!=pdTRUE)++queueDrops;
    }
    vTaskDelay(1);
  }
}

void updateIndicator() {
  uint32_t now = millis();
  if (pulseIndicator && uint32_t(now - pulseIndicatorSince) >= 350) pulseIndicator = false;
  // GPIO21 LED is active LOW. Heartbeat only means storage is recording-ready.
  bool on = (!storageOK || logFull || queueDrops || !sensorTaskOK) ? (now % 250 < 125)
                                  : (now % 2000 < 80 || pulseIndicator);
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

void record(const char *event, uint32_t timestamp, int level, uint32_t interval) {
  char row[128];
  snprintf(row, sizeof(row), "%s,%lu,%d,%lu,%lu\n", event,
    (unsigned long)timestamp, level, (unsigned long)closures, (unsigned long)interval);
  if (Serial && Serial.availableForWrite() >= strlen(row)) Serial.print(row);
  if (storageOK && !logFull) {
    if (logFile.size() + strlen(row) > MAX_LOG_BYTES) {
      logFull = true;
      logFile.flush();
    } else if (logFile.print(row) != strlen(row)) {
      storageOK = false;
    }
  }
}

void status() {
  hostLink.printf("STATUS firmware=bike-dashboard-r16 chip=%012llx pin=D2/GPIO3 level=%d closures=%lu transitions=%lu storage=%s full=%d bytes=%u debounce_us=%lu led_on=%d led_mode=%s wifi_supported=1 ble_supported=1 ble_ready=%d watch_csc=1 watch_power=1 gym_title=1 free_heap=%u max_alloc=%u\n",
    ESP.getEfuseMac(), digitalRead(SENSOR_PIN), (unsigned long)closures,
    (unsigned long)transitions, storageOK ? "OK" : "ERROR", logFull,
    storageOK ? (unsigned)logFile.size() : 0, (unsigned long)DEBOUNCE_US,
    digitalRead(LED_BUILTIN) == LOW, (!storageOK || logFull || queueDrops || !sensorTaskOK) ? "ERROR_FAST" : "RECORD_HEARTBEAT",
    hostLink.bluetoothReady(),ESP.getFreeHeap(),ESP.getMaxAllocHeap());
  hostLink.bluetoothStatus();hostLink.cyclingStatus();hostLink.powerStatus();hostLink.wifiStatus();
}

void handleCommand() {
  command.trim();
  command.toUpperCase();
  if (command == "DEMO ON") {setDemo(true);hostLink.println("DEMO ON: simulated display data only");}
  else if(command == "DEMO OFF") {setDemo(false);hostLink.println("DEMO OFF: live sensor display");}
  else if(command.startsWith("TITLE ")) {
    if(setDisplayTitleHex(command.c_str()+6))titleStatus();else hostLink.println("ERROR invalid title; use 1..24 printable ASCII characters as hex");
  }
  else if(command.startsWith("PPR ")) {
    int n;if(sscanf(command.c_str(),"PPR %d",&n)==1&&n>=1&&n<=16){
      hostLink.setPulsesPerRevolution(n);Preferences p;
      if(p.begin("bike-ui",false)){p.putUChar("ppr",n);p.end();}
      hostLink.cyclingStatus();
    }else hostLink.println("ERROR PPR must be 1..16");
  }
  else if(command.startsWith("BRIGHTNESS ")) {
    int value;if(sscanf(command.c_str(),"BRIGHTNESS %d",&value)==1){setScreenBrightness(value);hostLink.printf("BRIGHTNESS level=%d\n",screenBrightness);}
  }
  else if(command.startsWith("GLOW ")) {
    int b,c,f;if(sscanf(command.c_str(),"GLOW %d %d %d",&b,&c,&f)==3){setGlow(b,c,f);hostLink.printf("GLOW peak=%d centre=%d high_floor=%d\n",glowBrightness,glowCentre,glowHighFloor);}
    else hostLink.println("ERROR use GLOW peak_percent centre_percent high_floor_percent");
  }
  else if (command == "STATUS") {status();
    hostLink.printf("DEMO active=%d pulses=%lu rate=%.1f\n",demoMode,(unsigned long)demoPulses,pulseRate());
    titleStatus();hostLink.printf("METRIC rpm=%d\n",showRpm);
    hostLink.printf("EDGE green_gradient strength=%u\n",edgeStrength);
    hostLink.printf("GLOW peak=%d centre=%d high_floor=%d\n",glowBrightness,glowCentre,glowHighFloor);
    hostLink.printf("BRIGHTNESS level=%d\n",screenBrightness);
    hostLink.printf("UI ready=%d fps=%.1f resistance=%d touch_ok=%d touch_reads=%lu touch_errors=%lu touch_x=%d touch_y=%d sensor_task=%d drops=%lu\n",
      displayReady,uiFps,resistance,touchOK,(unsigned long)touchReads,(unsigned long)touchFailures,touchX,touchY,sensorTaskOK,(unsigned long)queueDrops);
  }
  else if(command == "SCREEN") {
    if(!hostLink.isUSBResponse()){hostLink.println("ERROR SCREEN requires USB");command="";return;}
    if(!displayReady){hostLink.println("ERROR display unavailable");command="";return;}
    Serial.setTxTimeoutMs(2000);
    Serial.println("BEGIN_RGB565 240 240");
    Serial.write((uint8_t*)canvas.getBuffer(),240*240*2);
    Serial.println("END_RGB565");
    Serial.setTxTimeoutMs(0);
  }
  else if (command == "DUMP") {
    if(!hostLink.isUSBResponse()){hostLink.println("ERROR DUMP requires USB");command="";return;}
    Serial.setTxTimeoutMs(2000);
    // Use only with pedals stopped: dumping deliberately blocks acquisition.
    if (logFile) logFile.flush();
    Serial.println("BEGIN_PEDAL_LOG");
    File input = LittleFS.open(LOG_PATH, "r");
    if (input) {
      uint8_t buffer[512];
      while (input.available()) {
        size_t n = input.read(buffer, sizeof(buffer));
        Serial.write(buffer, n);
      }
      input.close();
    } else hostLink.println("ERROR no log");
    Serial.println("END_PEDAL_LOG");
    Serial.setTxTimeoutMs(0);
  } else if (command == "MARK") record("MARK", micros(), digitalRead(SENSOR_PIN), 0);
  else hostLink.println("Commands: STATUS, DUMP (pedals stopped), MARK. Logs append across power cycles; no automatic erase.");
  command = "";
}

void setup() {
  hostLink.begin(115200);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);
  command.reserve(40);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  candidate = stableState = digitalRead(SENSOR_PIN);
  candidateSince = micros();
  // Never format an existing filesystem automatically.
  if (mountPedalStorage()) {
    logFile = LittleFS.open(LOG_PATH, "a");
    storageOK = (bool)logFile;
    if (storageOK && !logFile.size())
      logFile.println("event,boot_us,level,boot_closures,interval_us");
  }
  record("BOOT", micros(), stableState, 0);
  if (logFile) logFile.flush();
  sensorQueue=xQueueCreate(128,sizeof(SensorEvent));
  if(sensorQueue) sensorTaskOK=xTaskCreatePinnedToCore(sensorTask,"bike-sensor",3072,nullptr,3,nullptr,0)==pdPASS;
  uiSetup();hostLink.setResistance(resistance);
  Preferences sensorPrefs;if(sensorPrefs.begin("bike-ui",true)){hostLink.setPulsesPerRevolution(sensorPrefs.getUChar("ppr",1));sensorPrefs.end();}
  uint64_t mac=ESP.getEfuseMac();
  snprintf(Board::BluetoothName,sizeof(Board::BluetoothName),"Gym Xiao-%02X%02X%02X",int((mac>>24)&255),int((mac>>32)&255),int((mac>>40)&255));
  hostLink.beginWifi();hostLink.beginBluetooth();
}

void wirelessCommands() {
  // Bound work per display frame. Epoch changes discard incomplete commands
  // when a different authenticated transport/session takes over the line.
  for(int n=0;n<128&&hostLink.available();++n) {
    int raw=hostLink.read();if(raw<0)break;char c=char(raw);
    if(hostEpoch!=hostLink.epoch()) {
      hostEpoch=hostLink.epoch();command="";commandOverflow=false;
      receivingProvision=false;provisionLength=0;memset(provisionPacket,0,sizeof(provisionPacket));
    }
    if(c=='\r')continue;
    if(command.isEmpty()&&!receivingProvision&&!commandOverflow&&c=='?') {
      command="STATUS";handleCommand();hostLink.endResponse();continue;
    }
    if(command.isEmpty()&&!receivingProvision&&!commandOverflow&&c=='@') {
      receivingProvision=true;provisionLength=0;continue;
    }
    if(c=='\n') {
      if(receivingProvision) {
        bool ok=!commandOverflow&&hostLink.provisionWifi(provisionPacket);
        memset(provisionPacket,0,sizeof(provisionPacket));provisionLength=0;receivingProvision=false;
        hostLink.println(ok?"ACK N saved; restarting":"ERR WiFi setup rejected");
        if(ok){if(logFile)logFile.flush();delay(200);ESP.restart();}
      }else if(commandOverflow){hostLink.println("ERROR command too long");command="";}
      else if(!command.isEmpty())handleCommand();
      commandOverflow=false;hostLink.endResponse();
    }else if(receivingProvision) {
      if(provisionLength<sizeof(provisionPacket)-1){provisionPacket[provisionLength++]=c;provisionPacket[provisionLength]=0;}
      else commandOverflow=true;
    }else if(command.length()<64&&!commandOverflow)command+=c;
    else commandOverflow=true;
  }
  for(int n=0;n<4;++n)hostLink.serviceBluetooth();
}

void loop() {
  SensorEvent event;
  while(sensorQueue && xQueueReceive(sensorQueue,&event,0)==pdTRUE) {
    stableState=event.level;++transitions;
    if(stableState==LOW) {
      ++closures;haveClosure=true;lastClosure=event.timestamp;
      pulseIndicator=true;pulseIndicatorSince=millis();uiPulse(event.interval);hostLink.crankPulse(event.timestamp);
    }
    record(stableState==LOW?"CLOSE":"OPEN",event.timestamp,stableState,event.interval);
  }
  if (millis() - lastHeartbeat >= 10000) {
    record("HEARTBEAT", micros(), digitalRead(SENSOR_PIN), 0);
    lastHeartbeat = millis();
  }
  if (millis() - lastFlush >= 1000) {
    if (storageOK) logFile.flush();
    lastFlush = millis();
  }
  wirelessCommands();
  touchService();
  demoService();
  uiDraw();
  serviceScreenBrightness();
  updateIndicator();
  delay(1);
}
