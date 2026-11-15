#include "../XiaoPlayground/FrameTiles.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <random>
int main(){
 std::array<uint16_t,240*240> frame{},screen{};FrameTiles tiles;int calls=0;
 auto send=[&](int l,int t,int r,int b){assert(l>=0&&t>=0&&r<240&&b<240);calls++;for(int y=t;y<=b;y++)for(int x=l;x<=r;x++)screen[y*240+x]=frame[y*240+x];return true;};
 assert(tiles.flush(frame.data(),false,send));assert(calls==30&&frame==screen);calls=0;
 assert(tiles.flush(frame.data(),false,send)&&calls==0);
 std::mt19937 random(431);
 for(int i=0;i<200;i++){for(int j=0;j<30;j++)frame[random()%frame.size()]=random();assert(tiles.flush(frame.data(),false,send));assert(screen==frame);}
 // An interrupted send must not mark its unsent tiles as delivered.
 frame.fill(0xabcd);bool first=true;
 assert(!tiles.flush(frame.data(),false,[&](int l,int t,int r,int b){if(first){first=false;return send(l,t,r,b);}return false;}));
 assert(tiles.flush(frame.data(),false,send));assert(screen==frame);
 calls=0;assert(tiles.flush(frame.data(),true,send)&&calls==30);
 calls=0;tiles.invalidate();assert(tiles.flush(frame.data(),false,send)&&calls==30);
 puts("PASS exact framebuffer reconstruction, stationary frames, partial-transfer retry and forced repaint; tile tracking uses4500 bytes");
}
