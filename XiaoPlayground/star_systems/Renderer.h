#pragma once
#include "../UserSettings.h"
#include "Playback.h"
#include "../watchfaces/face_helpers.h"
namespace StarSystems {
using WatchFace::rgb;using WatchFace::blend;
struct Pixel {int32_t x=0,y=0;};
struct CurvePoint {int16_t x,y;};
class Renderer {
 static constexpr int Samples=48,TrailLength=20;
 CurvePoint curves[Data::OrbitCount][Samples+1];
 Pixel trails[6][TrailLength];int head=0,used=0;uint64_t sampleAt=0;
 int discCos=0,discSin=0,discUnflatten=0;
 bool initialised=false;
 static uint16_t backgroundColour(){return rgb(3,5,10);}
 static Pixel pixel(Vec v,System s){float r=scale(s)*256;return {30720+int32_t(lroundf(v.x*r)),30720-int32_t(lroundf(v.y*r))};}
 static void dot(uint16_t* out,int x,int y,uint16_t colour,int alpha){
  int dx=2*x+1-240,dy=2*y+1-240;
  if(x<0||x>=240||y<0||y>=240||dx*dx+dy*dy>=236*236||alpha<=0)return;
  out[y*240+x]=blend(out[y*240+x],colour,alpha>255?255:alpha);
 }
 // Fixed-point antialiased one-pixel segments. No float/sqrt work per pixel.
 static void line(uint16_t* out,Pixel a,Pixel b,uint16_t colour,int opacity=255){
  bool steep=abs(b.y-a.y)>abs(b.x-a.x);
  if(steep){int32_t t=a.x;a.x=a.y;a.y=t;t=b.x;b.x=b.y;b.y=t;}
  if(a.x>b.x){Pixel t=a;a=b;b=t;}int32_t dx=b.x-a.x,dy=b.y-a.y;
  if(!dx){dot(out,steep?a.y/256:a.x/256,steep?a.x/256:a.y/256,colour,opacity);return;}
  int first=a.x/256,last=b.x/256;int32_t step=int32_t(int64_t(dy)*256/dx);
  int32_t y=a.y+int32_t(int64_t(dy)*(first*256+128-a.x)/dx);
  for(int x=first;x<=last;x++,y+=step){
   int covered=256;if(x==first)covered=256-(a.x&255);if(x==last){int tail=(b.x&255)+1;if(tail<covered)covered=tail;}
   int iy=y>>8,f=y&255,alpha=opacity*covered/256;
   dot(out,steep?iy:x,steep?x:iy,colour,alpha*(255-f)/255);
   dot(out,steep?iy+1:x,steep?x:iy+1,colour,alpha*f/255);
  }
 }
 void initialise(){if(initialised)return;
  const float expanded[]={.22f,.22f,.22f,.55f,1,.36f,1,1};
  for(int id=0;id<Data::OrbitCount;id++)for(int j=0;j<=Samples;j++){
   // Q7 preserves the entire relative ellipse before mass splitting. The
   // eccentric TIC orbit exceeds 128 px, even though both stars fit on glass.
   Vec p=unitAtE(id,Tau*j/Samples);float r=(id==Data::Hr?105:110)*expanded[id]*128;
   curves[id][j]={int16_t(lroundf(p.x*r)),int16_t(lroundf(-p.y*r))};
  }
  float node=Data::Orbits[Data::Hr].node*Pi/180;
  discCos=lroundf(cosf(node)*1024);discSin=lroundf(sinf(node)*1024);
  discUnflatten=lroundf(1024/cosf(Data::Orbits[Data::Hr].i*Pi/180));
  initialised=true;
 }
 void trace(uint16_t* out,int id,float massFactor,Pixel centre,uint16_t colour){
  int weight=lroundf(massFactor*4096);Pixel previous;
  for(int j=0;j<=Samples;j++){
   auto p=curves[id][j];Pixel next={centre.x+p.x*weight/2048,centre.y+p.y*weight/2048};
   if(j)line(out,previous,next,colour);previous=next;
  }
 }
 void pairTracks(uint16_t* out,int id,float m1,float m2,Pixel centre,uint16_t colour){trace(out,id,-m2/(m1+m2),centre,colour);trace(out,id,m1/(m1+m2),centre,colour);}
 static uint16_t colour(int index){unsigned c=Data::Colours[index];return rgb(c>>16,(c>>8)&255,c&255);}
 static float radius(System s,int i){return s==System::HR?(i?3.0f:4.7f):s==System::TIC?3.5f:(i%2?2.5f:3.2f);}
 static void glow(uint16_t* out,Pixel centre,float core,uint16_t colour,int strength=170){
  int cx=(centre.x+8)/16,cy=(centre.y+8)/16,R=int(ceilf(core*3.6f)),square=R*R*256;
  for(int y=cy/16-R;y<=cy/16+R;y++)for(int x=cx/16-R;x<=cx/16+R;x++){
   int dx=x*16+8-cx,dy=y*16+8-cy,t=255-(dx*dx+dy*dy)*255/square;
   if(t>0)dot(out,x,y,colour,(((t*t)>>8)*t>>8)*strength/255);
  }
 }
 static void glyphText(uint16_t* out,const ClockFonts::Font& font,const char* text,int x,int baseline,uint16_t colour,int opacity){
  for(;*text;text++){
   uint8_t c=*text;if(c<font.first||c>font.last)continue;const auto& g=font.glyphs[c-font.first];
   for(int y=0;y<g.height;y++)for(int i=0;i<g.width;i++){
    int bit=y*g.width+i,v=font.pixels[g.offset+bit/2],alpha=(bit&1?v&15:v>>4)*17*opacity/255;
    if(alpha)dot(out,x+g.left+i,baseline+g.top+y,colour,alpha);
   }x+=g.advance;
  }
 }
 static void text(uint16_t* out,const ClockFonts::Font& f,const char* value,int baseline,uint16_t colour,int alpha){glyphText(out,f,value,120-WatchFace::Detail::textWidth(f,value)/2,baseline,colour,alpha);}
 void disc(uint16_t* out,Pixel be){
  // The source's illustrative 0.13 AU disc (~36 px), projected with its i/node.
  // A soft continuous density field reads as gas, rather than planet rings.
  int cx=(be.x+8)/16,cy=(be.y+8)/16;
  for(int y=cy/16-36;y<=cy/16+36;y++)for(int x=cx/16-36;x<=cx/16+36;x++){
   int dx=x*16+8-cx,dy=cy-y*16-8;
   int u=(dx*discCos+dy*discSin)/1024;
   int v=((-dx*discSin+dy*discCos)/1024)*discUnflatten/1024;
   int t=(u*u+v*v)*255/(36*36*256);
   if(t<255)dot(out,x,y,rgb(77,145,168),(255-t)*48/255);
  }
 }
 static void stream(uint16_t* out,const State& s,double seconds){
  Vec be=s.stars[0],donor=s.stars[1],v=add(donor,mul(be,-1));float len=sqrtf(v.x*v.x+v.y*v.y);Vec u=mul(v,1/len),side={-u.y,u.x};
  Vec a=add(donor,mul(u,-.045f)),d=add(be,mul(side,.103f));
  Vec b=add(add(a,mul(u,-len*.42f)),mul(side,len*.18f)),c=add(d,mul(u,.15f));
  float phase=float(fmod(seconds*.12,1.0));
  for(int k=0;k<36;k++){
   float q=fmodf(phase+k/36.0f,1.0f),t=q/.72f;Vec p;
   if(q<.72f){float v=1-t;p=add(add(mul(a,v*v*v),mul(b,3*v*v*t)),add(mul(c,3*v*t*t),mul(d,t*t*t)));}
   else{t=(q-.72f)/.28f;float angle=Pi*.5f+t*Tau*.8f,r=.172f*(1-t*.65f);p=add(be,add(mul(u,r*cosf(angle)),mul(side,r*.6f*sinf(angle))));}
   int alpha=int(85*fminf(1,q/.12f)*fminf(1,(1-q)/.18f));Pixel px=pixel(p,System::HR);
   dot(out,(px.x+128)/256,(px.y+128)/256,rgb(207,159,126),alpha);
   dot(out,(px.x+128)/256+1,(px.y+128)/256,rgb(187,132,110),alpha/3);
  }
 }
 struct Box {int x,y,w,h;};
 static bool overlaps(Box a,Box b){return a.x<b.x+b.w+3&&a.x+a.w+3>b.x&&a.y<b.y+b.h+3&&a.y+a.h+3>b.y;}
 static bool inside(Box b){for(int x : {b.x,b.x+b.w})for(int y : {b.y,b.y+b.h})if((x-120)*(x-120)+(y-120)*(y-120)>110*110)return false;return true;}
 static void labels(uint16_t* out,const State& state,System system,int alpha){
  if(!alpha)return;Box placed[6];int count=0;
  for(int i=0;i<state.count;i++){
   const char* name=Data::Names[int(system)][i];bool two=system==System::HR&&i==1;
   const char* first=two?"Stripped":name;int width=WatchFace::Detail::textWidth(ClockFonts::Small,two?"companion":name),height=two?23:11;
   Pixel at=pixel(state.stars[i],system);int x=(at.x+128)/256,y=(at.y+128)/256;
   Box options[]={{x+11,y-17,width,height},{x-width-11,y-17,width,height},{x+11,y+9,width,height},{x-width-11,y+9,width,height},{x-width/2,y-height-13,width,height},{x-width/2,y+13,width,height},{120-width/2,48+count*24,width,height}};
   Box best=options[6];bool found=false;
   for(auto box:options){if(!inside(box))continue;bool clear=true;
    if(overlaps(box,{55,24,130,15})||overlaps(box,{65,206,110,15}))clear=false;
    for(int j=0;j<count;j++)if(overlaps(box,placed[j]))clear=false;
    for(int j=0;j<state.count;j++){Pixel p=pixel(state.stars[j],system);if(overlaps(box,{int(p.x/256)-8,int(p.y/256)-8,16,16}))clear=false;}
    if(clear){best=box;found=true;break;}
   }
   if(!found&&!inside(best))continue;
   placed[count++]=best;uint16_t ink=blend(colour(i),rgb(233,235,230),145);
   glyphText(out,ClockFonts::Small,first,best.x,best.y+9,ink,alpha);
   if(two)glyphText(out,ClockFonts::Small,"companion",best.x,best.y+21,ink,alpha);
  }
 }
 static void introduction(uint16_t* out,const Playback& play,uint32_t now){
  int alpha=play.titleOpacity(now);uint16_t ivory=rgb(234,233,220),gold=rgb(160,139,96);
  if(alpha){
   int veil=alpha*224/255;
   for(int i=0;i<240*240;i++)if(out[i])out[i]=blend(out[i],backgroundColour(),veil);
   text(out,ClockFonts::Small,UserSettings::Branding,65,gold,alpha);
   WatchFace::Painter p(out);p.line({94,78},{146,78},.6f,blend(backgroundColour(),gold,alpha));
   text(out,ClockFonts::Title,Data::Titles[int(play.system)],109,ivory,alpha);
   const char* subtitle=play.system==System::TYC?"Six stars / three binaries":play.system==System::TIC?"The compact triple":"The vampire binary";
   text(out,ClockFonts::Small,subtitle,132,rgb(161,185,202),alpha);
   const char* caveat=play.system==System::TYC?"Independent orbital clocks":play.system==System::TIC?"Expanded binary separation":"After major mass transfer";
   text(out,ClockFonts::Small,caveat,155,rgb(132,150,168),alpha);
   text(out,ClockFonts::Small,"Schematic / not to scale",172,gold,alpha);
   if(!play.noticeOpacity(now)){
    text(out,ClockFonts::Small,"Centre tap: next system",194,rgb(137,150,159),alpha);
    if(play.system==System::HR)text(out,ClockFonts::Small,"Lower tap: historical flow",210,rgb(137,150,159),alpha);
   }
  }else if(int labels=play.labelOpacity(now)){
   text(out,ClockFonts::Small,Data::Titles[int(play.system)],34,rgb(135,157,175),labels);
   text(out,ClockFonts::Small,UserSettings::Branding,217,rgb(106,121,139),labels);
  }
  if(play.system==System::HR){
   int notice=play.noticeOpacity(now);
   if(notice){
    text(out,ClockFonts::Small,play.historical?"Historical transfer":"Present-day view",184,rgb(224,178,123),notice);
    text(out,ClockFonts::Small,play.historical?"Artistic / not observed today":"Be-star decretion disc",200,rgb(163,154,143),notice);
   }else if(play.historical&&!alpha)text(out,ClockFonts::Small,"Historical illustration",216,rgb(112,91,69),190);
  }
 }
 public:
 void resetTrails(){head=used=0;sampleAt=0;}
 void background(uint16_t* out,System system){
  initialise();for(int y=0;y<240;y++)for(int x=0;x<240;x++){int dx=2*x+1-240,dy=2*y+1-240;out[y*240+x]=dx*dx+dy*dy<236*236?backgroundColour():0;}
  // Sparse, very dim backdrop; the coloured moving stars remain dominant.
  uint32_t seed=12347;for(int i=0;i<20;i++){seed=seed*1664525+1013904223;int x=25+seed%190;seed=seed*1664525+1013904223;int y=25+seed%190;dot(out,x,y,rgb(30,43,59),100);}
  auto m=Data::Masses[int(system)];Pixel centre={30720,30720};
  if(system==System::TYC)pairTracks(out,Data::Wide,m[0]+m[1]+m[2]+m[3],m[4]+m[5],centre,rgb(23,35,49));
  else if(system==System::TIC)pairTracks(out,Data::TicOuter,m[0]+m[1],m[2],centre,rgb(30,43,60));
  else pairTracks(out,Data::Hr,m[0],m[1],centre,rgb(30,43,60));
 }
 void render(uint16_t* out,const uint16_t* bg,const Playback& play,uint32_t now){
  memcpy(out,bg,240*240*sizeof(uint16_t));System system=play.system;State s=state(system,play.seconds());const float* m=Data::Masses[int(system)];
  if(system==System::TYC){
   pairTracks(out,Data::Quad,m[0]+m[1],m[2]+m[3],pixel(s.ac,system),rgb(29,41,56));
   for(int k=0;k<3;k++)pairTracks(out,Data::PairA+k,m[k*2],m[k*2+1],pixel(s.centres[k],system),rgb(41,53,66));
  }else if(system==System::TIC)pairTracks(out,Data::TicInner,m[0],m[1],pixel(s.centres[0],system),rgb(44,55,73));
  uint64_t ms=uint64_t(play.seconds()*1000+.5);
  if(!used||ms-sampleAt>=120){
   if(used&&ms-sampleAt>500)resetTrails();
   for(int i=0;i<s.count;i++)trails[i][head]=pixel(s.stars[i],system);
   head=(head+1)%TrailLength;if(used<TrailLength)used++;sampleAt=ms;
  }
  for(int i=0;i<s.count;i++)for(int age=used-1;age>0;age--){
   int at=(head-1-age+TrailLength)%TrailLength,next=(at+1)%TrailLength;
   line(out,trails[i][at],trails[i][next],colour(i),(used-age)*49/TrailLength);
  }
  if(system==System::HR){disc(out,pixel(s.stars[0],system));if(play.historical)stream(out,s,play.seconds());}
  WatchFace::Painter painter(out);
  for(int i=0;i<s.count;i++){
   Pixel at=pixel(s.stars[i],system);float r=radius(system,i),x=at.x/256.0f,y=at.y/256.0f;
   glow(out,at,r,colour(i));painter.disk(x,y,r,colour(i));painter.disk(x-.25f,y-.25f,r*.49f,blend(colour(i),rgb(255,253,240),205));
  }
  labels(out,s,system,play.labelOpacity(now)*(255-play.titleOpacity(now))/255);introduction(out,play,now);
 }
};
} // namespace StarSystems
