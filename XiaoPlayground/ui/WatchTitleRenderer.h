#pragma once
#include "../WatchFace.h"

namespace WatchUI {
inline void title(uint16_t* out,const uint16_t* dial,WatchFace::Style style,bool demo,WatchAppearance appearance=WatchAppearance::Light){
 using namespace WatchFace;
 const auto& face=definition(style);
 // Preserve the dial around the perimeter, under a quiet translucent veil.
 bool dark=appearance==WatchAppearance::Dark;
 const uint16_t veil=dark?rgb(8,13,19):rgb(242,235,218),paper=dark?rgb(245,235,213):rgb(35,39,43);
 for(int i=0;i<Size*Size;i++)out[i]=blend(dial[i],veil,211);
 Painter p(out);
 uint16_t accent=blend(themedColour(style,appearance,face.theme.accent),paper,80);
 p.line({86,71},{111,71},.65f,accent);p.line({129,71},{154,71},.65f,accent);
 Detail::star(p,{120,71},3,accent,4);
 bool two=face.title2[0];
 Detail::centred(out,ClockFonts::Title,face.title1,120,two?105:117,paper);
 if(two)Detail::centred(out,ClockFonts::Title,face.title2,120,130,paper);
 Detail::centred(out,ClockFonts::Small,face.location,120,two?153:147,dark?rgb(187,189,183):rgb(99,101,96));
 p.line({101,168},{139,168},.6f,accent);
 // Small collection-position dots; filled when this is the selected clock.
 float firstDot=120-(Count-1)*4;
 for(int i=0;i<Count;i++)p.disk(firstDot+i*8,187,i==int(style)?1.8f:1.05f,i==int(style)?paper:rgb(84,89,91));
 if(demo)Detail::centred(out,ClockFonts::Small,"DEMO",120,206,accent);
}
} // namespace WatchUI
