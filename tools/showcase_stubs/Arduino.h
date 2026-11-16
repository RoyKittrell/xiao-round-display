#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <algorithm>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
using String=std::string;
class __FlashStringHelper;

inline float radians(float degrees){return degrees*3.14159265358979323846f/180.0f;}
