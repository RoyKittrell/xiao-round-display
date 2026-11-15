#pragma once
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif
#include "WatchAppearance.h"
#include "watchfaces/big_ben.h"
#include "watchfaces/st_marks.h"
#include "watchfaces/prague_astronomical.h"
#include "watchfaces/conciergerie.h"
#include "watchfaces/zytglogge.h"
#include "watchfaces/spasskaya.h"
#include "watchfaces/grand_central.h"
#include "watchfaces/aberdeen.h"
#include "watchfaces/sultan_abdul_samad.h"
#include "watchfaces/royal_holloway.h"
#include "watchfaces/face_palettes.h"

namespace WatchFace {
enum class Style : uint8_t {BigBen,StMarks,PragueAstronomical,Conciergerie,Zytglogge,Spasskaya,GrandCentral,Aberdeen,SultanAbdulSamad,RoyalHolloway,Count};
struct Theme {uint16_t background,ink,accent;};
struct Definition {
 const char* id;
 const char* name;
 const char* title1;
 const char* title2;
 const char* location;
 Theme theme;
 void (*dial)(uint16_t*);
 void (*hands)(uint16_t*,const uint16_t*,double);
 ColourTransform light,dark;
};
constexpr int Count=int(Style::Count);
inline const Definition& definition(Style style){
 static const Definition faces[]={
  {"big_ben","Big Ben","Big Ben","","London / United Kingdom",{BigBen::Glass,BigBen::Ink,BigBen::Gold},BigBen::dial,BigBen::hands,nullptr,Palettes::bigBenDark},
  {"st_marks","St Mark's Clock","St Mark's","Clock","Venice / Italy",{StMarks::Blue,StMarks::Stone,StMarks::Gold},StMarks::dial,StMarks::hands,nullptr,Palettes::stMarksDark},
  {"prague_astronomical","Prague Astronomical Clock","Prague","Astronomical Clock","Prague / Czechia",{Prague::Blue,Prague::Ink,Prague::Gold},Prague::dial,Prague::hands,Palettes::pragueLight,Palettes::pragueDark},
  {"conciergerie","Conciergerie Clock","Conciergerie","Clock","Paris / France",{Conciergerie::Royal,Conciergerie::Ivory,Conciergerie::Gold},Conciergerie::dial,Conciergerie::hands,nullptr,Palettes::conciergerieDark},
  {"zytglogge","Zytglogge","Zytglogge","","Bern / Switzerland",{Zytglogge::Red,Zytglogge::Black,Zytglogge::Gold},Zytglogge::dial,Zytglogge::hands,Palettes::zytgloggeLight,Palettes::zytgloggeDark},
  {"spasskaya","Spasskaya Tower Clock","Spasskaya","Tower Clock","Moscow / Russia",{Spasskaya::Dark,Spasskaya::Gold,Spasskaya::Gold},Spasskaya::dial,Spasskaya::hands,Palettes::spasskayaLight,Palettes::spasskayaDark},
  {"grand_central","Grand Central Terminal Clock","Grand Central","Terminal Clock","New York / United States",{0xef36,GrandCentral::Ink,GrandCentral::Brass},GrandCentral::dial,GrandCentral::hands,nullptr,Palettes::grandCentralDark},
  {"aberdeen","Aberdeen Town House Clock","Aberdeen","Town House Clock","Aberdeen / Scotland",{Aberdeen::Glass,Aberdeen::Ink,Aberdeen::Gold},Aberdeen::dial,Aberdeen::hands,nullptr,Palettes::aberdeenDark},
  {"sultan_abdul_samad","Sultan Abdul Samad Building Clock Tower","Sultan Abdul Samad","Building Clock Tower","Kuala Lumpur / Malaysia",{SultanAbdulSamad::Glass,SultanAbdulSamad::Ink,SultanAbdulSamad::Copper},SultanAbdulSamad::dial,SultanAbdulSamad::hands,nullptr,Palettes::sultanDark},
  {"royal_holloway","Founder's Building Clock","Founder's Building","Royal Holloway","Egham / United Kingdom",{RoyalHolloway::Glass,RoyalHolloway::Ink,RoyalHolloway::Gold},RoyalHolloway::dial,RoyalHolloway::hands,nullptr,Palettes::royalHollowayDark}
 };
 static_assert(sizeof(faces)/sizeof(faces[0])==Count,"Register every clock face");
 int index=int(style);return faces[index>=0&&index<Count?index:0];
}
inline const char* name(Style style){return definition(style).name;}
inline bool centreTap(int x,int y){int dx=x-120,dy=y-120;return dx*dx+dy*dy<=38*38;}
inline ColourTransform transform(Style style,WatchAppearance appearance){const auto& f=definition(style);return appearance==WatchAppearance::Dark?f.dark:f.light;}
inline uint16_t themedColour(Style style,WatchAppearance appearance,uint16_t c){auto map=transform(style,appearance);return map?map(c):c;}
inline void dial(uint16_t* out,Style style,WatchAppearance appearance=WatchAppearance::Light){ScopedColourTransform palette(transform(style,appearance));definition(style).dial(out);}
inline void hands(uint16_t* out,const uint16_t* bg,double seconds,Style style,WatchAppearance appearance=WatchAppearance::Light){ScopedColourTransform palette(transform(style,appearance));definition(style).hands(out,bg,seconds);}
} // namespace WatchFace
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
