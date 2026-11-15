#pragma once
#include <stdint.h>

// Timing is independent of rendering. The three seconds start only after the
// title has reached the LCD, not while a new static dial is still being drawn.
class WatchTitle {
 bool visible=false,painted=false,presented=false;
 uint32_t shownAt=0;
 public:
 static constexpr uint32_t Duration=3000;
 void begin(){visible=true;painted=presented=false;}
 void dismiss(){visible=false;}
 bool active() const{return visible;}
 bool needsPaint() const{return visible&&!painted;}
 void didPaint(){painted=true;}
 void repaint(){painted=false;}
 void didPresent(uint32_t now){if(visible&&painted&&!presented){presented=true;shownAt=now;}}
 bool tick(uint32_t now){if(visible&&presented&&uint32_t(now-shownAt)>=Duration){visible=false;return true;}return false;}
};
