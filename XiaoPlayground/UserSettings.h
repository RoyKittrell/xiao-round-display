#pragma once
#include <stdint.h>
#if __has_include("UserSettings.local.h")
#include "UserSettings.local.h"
#else
// Blank public defaults. configure.py can create an ignored local override.
namespace UserSettings {
constexpr int UtcOffsetMinutes=0;
constexpr const char* TimeZoneLabel="UTC";
constexpr const char* LocationLabel="Location not set";
constexpr bool HasObserver=false;
constexpr double Latitude=0,Longitude=0;
constexpr const char* Branding="";
}
#endif
namespace UserSettings {
constexpr int UtcOffsetSeconds=UtcOffsetMinutes*60;
constexpr uint32_t UnixAtCivilEpoch=uint32_t(946684800LL-UtcOffsetSeconds);
}
