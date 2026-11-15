#pragma once
#include "../WatchFace.h"
namespace WatchUI {
enum class Tap {None,NextFace};
inline Tap tap(int x,int y){
 if(WatchFace::centreTap(x,y))return Tap::NextFace;
 return Tap::None;
}
}
