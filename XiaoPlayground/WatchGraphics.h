#pragma once
// Portable RGB565 vector renderer. The desktop preview uses this exact code.
// No heap allocations: callers lend the existing dial and frame buffers.
#include <stdint.h>
#include <math.h>
#include <string.h>
#if defined(__GNUC__) && !defined(__clang__)
// The core defaults to -Os. Optimise this renderer's integer pixel loops for
// speed without changing the compiler settings of the hardware libraries.
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif

namespace WatchFace {
constexpr int Size=240;
constexpr float Pi=3.14159265358979323846f;
struct Point {float x,y;};
inline uint16_t rgb(int r,int g,int b){return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
inline float clamp(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
inline uint16_t blend(uint16_t a,uint16_t b,int opacity){
 int inv=255-opacity;
 int r=(((a>>11)&31)*inv+((b>>11)&31)*opacity+127)/255;
 int g=(((a>>5)&63)*inv+((b>>5)&63)*opacity+127)/255;
 int bl=((a&31)*inv+(b&31)*opacity+127)/255;
 return (r<<11)|(g<<5)|bl;
}
using ColourTransform=uint16_t (*)(uint16_t);
inline ColourTransform& activeColourTransform(){static ColourTransform transform=nullptr;return transform;}
class ScopedColourTransform {
 ColourTransform previous;
 public:
 explicit ScopedColourTransform(ColourTransform next):previous(activeColourTransform()){activeColourTransform()=next;}
 ~ScopedColourTransform(){activeColourTransform()=previous;}
 ScopedColourTransform(const ScopedColourTransform&)=delete;
 ScopedColourTransform& operator=(const ScopedColourTransform&)=delete;
};
class Painter {
 uint16_t* pixels;
 ColourTransform transform;
 uint16_t colour(uint16_t value) const{return transform?transform(value):value;}
 void pixel(int x,int y,uint16_t colour,int coverage){
  if(coverage<=0)return;
  int i=y*Size+x;
  pixels[i]=coverage>=256?colour:blend(pixels[i],colour,(coverage*255+128)>>8);
 }
 static int low(float x){return int(clamp(floorf(x-1),0,Size-1));}
 static int high(float x){return int(clamp(ceilf(x+1),0,Size-1));}
 public:
 explicit Painter(uint16_t* target):pixels(target),transform(activeColourTransform()){}
 void ring(float cx,float cy,float radius,float width,uint16_t colour){
  colour=this->colour(colour);
  int centreX=lroundf(cx*256),centreY=lroundf(cy*256);
  int outer=lroundf((radius+width*.5f)*256),inner=lroundf((radius-width*.5f)*256);
  if(inner<=0||outer<=0)return;
  int l=low(cx-radius-width),r=high(cx+radius+width),t=low(cy-radius-width),b=high(cy+radius+width);
  for(int y=t;y<=b;y++)for(int x=l;x<=r;x++){
   int dx=x*256+128-centreX,dy=y*256+128-centreY,square=dx*dx+dy*dy;
   int coverage=128+(outer*outer-square)/(2*outer);
   int inside=128+(square-inner*inner)/(2*inner);
   if(inside<coverage)coverage=inside;
   if(coverage>0)pixel(x,y,colour,coverage);
  }
 }
 // Metal has a bright left facet and a darker right facet. Its highlight stays
 // in screen coordinates while hands turn, as if lit from above-left.
 uint16_t metal(int x,int y,int centreX,int halfWidth){
  int t=((x*256+128-centreX)*256)/(halfWidth+3);if(t<-256)t=-256;if(t>256)t=256;
  int light=179-(66*t)/256-(78*(y-120))/256;if(light<64)light=64;if(light>255)light=255;
  return colour(rgb((220*light)/255,(230*light)/255,(232*light)/255));
 }
 void disk(float cx,float cy,float radius,uint16_t colour,bool polished=false){
  colour=this->colour(colour);
  int centreX=lroundf(cx*256),centreY=lroundf(cy*256),r=lroundf(radius*256);
  const int left=low(cx-radius),right=high(cx+radius),top=low(cy-radius),bottom=high(cy+radius);
  for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++){
   int dx=x*256+128-centreX,dy=y*256+128-centreY;
   // The squared-distance ramp is tangent to the circle at its edge. All
   // per-pixel work is integer: this core is built with software floats.
   int coverage=128+(r*r-dx*dx-dy*dy)/(2*r);
   if(coverage>0)pixel(x,y,polished?metal(x,y,centreX,r):colour,coverage);
  }
 }
 void polygon(const Point* points,int count,uint16_t colour,bool polished=false,const uint16_t* underlay=nullptr){
  colour=this->colour(colour);
  // All polygons passed here are convex; signed edge distances give a
  // one-pixel coverage ramp without a supersized framebuffer.
  float area=0,minX=Size,maxX=0,minY=Size,maxY=0;
  int nx[8],ny[8],constants[8];
  for(int i=0;i<count;i++){
   Point a=points[i],b=points[(i+1)%count];area+=a.x*b.y-b.x*a.y;
   minX=fminf(minX,a.x);maxX=fmaxf(maxX,a.x);minY=fminf(minY,a.y);maxY=fmaxf(maxY,a.y);
  }
  float orientation=area>=0?1:-1;
  for(int i=0;i<count;i++){
   Point a=points[i],b=points[(i+1)%count];float dx=b.x-a.x,dy=b.y-a.y;
   float scale=orientation/sqrtf(dx*dx+dy*dy);
   float normalX=-dy*scale,normalY=dx*scale;
   nx[i]=lroundf(normalX*4096);ny[i]=lroundf(normalY*4096);
   constants[i]=lroundf(-(normalX*a.x+normalY*a.y)*4096)+(nx[i]+ny[i])/2+2048;
  }
  int centreX=lroundf((minX+maxX)*128),halfWidth=lroundf((maxX-minX)*128);
  const int left=low(minX),right=high(maxX),top=low(minY),bottom=high(maxY);
  for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++){
   int distance=4096;
   for(int e=0;e<count;e++){int d=nx[e]*x+ny[e]*y+constants[e];if(d<distance)distance=d;if(distance<=0)break;}
   if(distance>0)pixel(x,y,underlay?underlay[y*Size+x]:polished?metal(x,y,centreX,halfWidth):colour,(distance+8)>>4);
  }
 }
 void line(Point a,Point b,float width,uint16_t colour){
  float dx=b.x-a.x,dy=b.y-a.y,scale=width*.5f/sqrtf(dx*dx+dy*dy);
  Point p[4]={{a.x-dy*scale,a.y+dx*scale},{b.x-dy*scale,b.y+dx*scale},
              {b.x+dy*scale,b.y-dx*scale},{a.x+dy*scale,a.y-dx*scale}};
  polygon(p,4,colour);
 }
};
inline Point radial(float angle,float r){return {120+sinf(angle)*r,120-cosf(angle)*r};}
class Hand {
 Painter& p;float sine,cosine;
 public:
 Point origin={120,120};
 Hand(Painter& painter,float turns):p(painter),sine(sinf(turns*2*Pi)),cosine(cosf(turns*2*Pi)){}
 Point at(float across,float along){return {origin.x+cosine*across+sine*along,origin.y+sine*across-cosine*along};}
 void poly(const Point* local,int n,uint16_t colour,bool polished=false,const uint16_t* underlay=nullptr){
  Point world[8];for(int i=0;i<n;i++)world[i]=at(local[i].x,local[i].y);p.polygon(world,n,colour,polished,underlay);
 }
 void disk(float across,float along,float radius,uint16_t colour,bool polished=false){Point c=at(across,along);p.disk(c.x,c.y,radius,colour,polished);}
 void line(float x0,float y0,float x1,float y1,float width,uint16_t colour){p.line(at(x0,y0),at(x1,y1),width,colour);}
};
} // namespace WatchFace
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
