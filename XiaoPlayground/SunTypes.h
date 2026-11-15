#pragma once
#include <stdint.h>
struct SunMeta {uint32_t imageEpoch,fetchedEpoch,kpEpoch,scalesEpoch,flareEpoch,xrayEpoch;float kp;int g,r,s;char xray[12],flare[12];};
