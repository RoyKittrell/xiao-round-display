#pragma once
#include "Arduino.h"
class Print {
public:
 virtual size_t write(uint8_t)=0;
 size_t write(const uint8_t* p,size_t n){for(size_t i=0;i<n;i++)write(p[i]);return n;}
 size_t print(const char* p){return write((const uint8_t*)p,strlen(p));}
};
