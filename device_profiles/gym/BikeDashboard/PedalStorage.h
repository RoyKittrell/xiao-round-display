#pragma once
#include <esp_partition.h>
// A Desktop-only device may never have had a filesystem. Initialize only a
// completely erased partition. Never format existing or damaged user data.
inline bool mountPedalStorage(){
  if(LittleFS.begin(false))return true;
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
  if(!part)return false;
  uint8_t data[256];
  for(size_t offset=0;offset<part->size;offset+=sizeof(data)){
    size_t length=part->size-offset<sizeof(data)?part->size-offset:sizeof(data);
    if(esp_partition_read(part,offset,data,length)!=ESP_OK)return false;
    for(size_t i=0;i<length;i++)if(data[i]!=0xff)return false;
  }
  return LittleFS.format()&&LittleFS.begin(false);
}
