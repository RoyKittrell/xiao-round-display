#pragma once
#include <stdint.h>
namespace WatchDirty {
// Four-row tiles keep SPI command overhead small, while separate runs avoid
// transmitting the empty space between hands. Compare the last *sent* frame.
template<int Tile=4,class Send> void flush(const uint16_t* frame,const uint16_t* sent,Send send){
 static_assert(Tile==4||Tile==8,"Watch dirty tiles must divide the panel and fit a 64-bit row mask");
 constexpr int Columns=240/Tile;
 for(int ty=0;ty<Columns;ty++){
  uint64_t dirty=0;
  for(int y=ty*Tile;y<ty*Tile+Tile;y++)for(int x=0;x<240;x++)
   if(frame[y*240+x]!=sent[y*240+x])dirty|=uint64_t(1)<<(x/Tile);
  for(int x=0;x<Columns;){
   if(!(dirty&(uint64_t(1)<<x))){x++;continue;}
   int begin=x;while(x<Columns&&(dirty&(uint64_t(1)<<x)))x++;
   if(!send(begin*Tile,ty*Tile,x*Tile-1,ty*Tile+Tile-1))return;
  }
 }
}
}
