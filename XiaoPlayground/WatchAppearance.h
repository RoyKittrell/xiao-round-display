#pragma once
#include <stdint.h>

enum class WatchAppearance : uint8_t {Light,Dark};
enum class AppearanceOverride : uint8_t {Automatic,Light,Dark};

// The existing RTC/host clock is configured civil time. Half-open intervals make
// 17:00 dark and 07:30 light, including midnight without any date arithmetic.
inline WatchAppearance scheduledAppearance(uint32_t localSeconds){
 uint32_t minute=(localSeconds%86400)/60;
 return minute>=17*60||minute<7*60+30?WatchAppearance::Dark:WatchAppearance::Light;
}
class DisplayLighting {
 public:
 AppearanceOverride overrideMode=AppearanceOverride::Automatic;
 WatchAppearance appearance=WatchAppearance::Light;
 uint8_t dayBrightness=0,nightBrightness=0;
 bool update(uint32_t localSeconds){
  auto next=overrideMode==AppearanceOverride::Automatic?scheduledAppearance(localSeconds):
   overrideMode==AppearanceOverride::Dark?WatchAppearance::Dark:WatchAppearance::Light;
  bool changed=next!=appearance;appearance=next;return changed;
 }
 uint8_t brightness() const{return appearance==WatchAppearance::Dark?nightBrightness:dayBrightness;}
 void adjust(int delta){
  int next=int(brightness())+delta;if(next<0)next=0;if(next>100)next=100;
  if(appearance==WatchAppearance::Dark)nightBrightness=next;else dayBrightness=next;
 }
};
