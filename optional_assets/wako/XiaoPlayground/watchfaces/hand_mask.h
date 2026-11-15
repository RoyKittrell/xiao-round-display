#pragma once
#include "../WatchGraphics.h"
#include "../assets/fonts/WakoHands.h"
#include <initializer_list>
namespace WatchFace { namespace Detail {
inline int coverage(const WakoAssets::Mask& mask,int x,int y){
 if(x<0||y<0||x>=mask.width||y>=mask.height)return 0;
 int bit=y*mask.width+x;uint8_t byte=mask.pixels[bit/2];return bit&1?byte&15:byte>>4;
}
// Rotate a small immutable coverage mask with bilinear sampling. Fixed-point
// increments keep all per-pixel work integer on the Nordic's software FPU.
inline void handMask(uint16_t* out,const WakoAssets::Mask& mask,float turns,uint16_t colour){
 if(activeColourTransform())colour=activeColourTransform()(colour);
 float s=sinf(turns*2*Pi),c=cosf(turns*2*Pi),minX=240,maxX=0,minY=240,maxY=0;
 for(int yy:{0,mask.height})for(int xx:{0,mask.width}){
  float u=xx-mask.pivotX,v=yy-mask.pivotY,x=120+c*u-s*v,y=120+s*u+c*v;
  minX=fminf(minX,x);maxX=fmaxf(maxX,x);minY=fminf(minY,y);maxY=fmaxf(maxY,y);
 }
 int l=int(clamp(floorf(minX)-1,0,239)),r=int(clamp(ceilf(maxX)+1,0,239));
 int top=int(clamp(floorf(minY)-1,0,239)),bottom=int(clamp(ceilf(maxY)+1,0,239));
 int32_t stepX=lroundf(c*65536),stepY=lroundf(s*65536);
 int32_t rowX=lroundf((c*(l+.5f-120)+s*(top+.5f-120)+mask.pivotX)*65536);
 int32_t rowY=lroundf((-s*(l+.5f-120)+c*(top+.5f-120)+mask.pivotY)*65536);
 for(int y=top;y<=bottom;y++,rowX+=stepY,rowY+=stepX){
  int32_t sx=rowX,sy=rowY;
  for(int x=l;x<=r;x++,sx+=stepX,sy-=stepY){
   int ix=sx>>16,iy=sy>>16,fx=(sx>>8)&255,fy=(sy>>8)&255;
   int a=coverage(mask,ix,iy)*(256-fx)*(256-fy)+coverage(mask,ix+1,iy)*fx*(256-fy)+coverage(mask,ix,iy+1)*(256-fx)*fy+coverage(mask,ix+1,iy+1)*fx*fy;
   int opacity=((a+32768)>>16)*17;
   if(opacity)out[y*Size+x]=blend(out[y*Size+x],colour,opacity);
  }
 }
}
} }
