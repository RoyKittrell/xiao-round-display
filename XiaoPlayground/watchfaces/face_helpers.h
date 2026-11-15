#pragma once
#include "../WatchGraphics.h"
#include "../assets/fonts/ClockFonts.h"
#include <initializer_list>

namespace WatchFace { namespace Detail {
// The glyphs contain 4-bit coverage, so small serif text remains antialiased.
inline int textWidth(const ClockFonts::Font& f,const char* s){
 int width=0;for(;*s;s++)if(uint8_t(*s)>=f.first&&uint8_t(*s)<=f.last)width+=f.glyphs[uint8_t(*s)-f.first].advance;return width;
}
inline void text(uint16_t* out,const ClockFonts::Font& f,const char* s,int x,int baseline,uint16_t colour){
 if(activeColourTransform())colour=activeColourTransform()(colour);
 for(;*s;s++){
  uint8_t ch=uint8_t(*s);if(ch<f.first||ch>f.last)continue;
  const auto& g=f.glyphs[ch-f.first];
  for(int y=0;y<g.height;y++)for(int i=0;i<g.width;i++){
   int px=x+g.left+i,py=baseline+g.top+y;if(px<0||px>=Size||py<0||py>=Size)continue;
   int bit=y*g.width+i;uint8_t v=f.pixels[g.offset+bit/2];int alpha=(bit&1?v&15:v>>4)*17;
   if(alpha)out[py*Size+px]=blend(out[py*Size+px],colour,alpha);
  }
  x+=g.advance;
 }
}
inline void centred(uint16_t* out,const ClockFonts::Font& f,const char* s,int cx,int baseline,uint16_t colour){text(out,f,s,cx-textWidth(f,s)/2,baseline,colour);}
inline void ring(Painter& p,float cx,float cy,float r,float width,uint16_t colour){
 p.ring(cx,cy,r,width,colour);
}
inline void ring(Painter& p,float r,float w,uint16_t c){ring(p,120,120,r,w,c);}
inline void track(Painter& p,float inner,float outer,uint16_t c,float thin=.65f,float thick=1.6f){
 for(int i=0;i<60;i++){float a=i*Pi/30;p.line(radial(a,i%5?inner:inner-3),radial(a,outer),i%5?thin:thick,c);}
}
inline void star(Painter& p,Point c,float r,uint16_t colour,int rays=8){
 for(int i=0;i<rays;i++){
  float a=i*2*Pi/rays;Point tip={c.x+sinf(a)*r,c.y-cosf(a)*r};
  Point left={c.x+sinf(a-.55f)*r*.32f,c.y-cosf(a-.55f)*r*.32f};
  Point right={c.x+sinf(a+.55f)*r*.32f,c.y-cosf(a+.55f)*r*.32f};
  Point triangle[3]={left,tip,right};p.polygon(triangle,3,colour);
 }
 p.disk(c.x,c.y,r*.28f,colour);
}
inline void sun(Painter& p,Point c,float r,uint16_t gold){
 star(p,c,r,gold,12);p.disk(c.x,c.y,r*.58f,rgb(134,86,29));p.disk(c.x,c.y,r*.48f,rgb(245,210,111));
}
inline float romanWidth(char c){return c=='I'?.36f:.76f;}
inline void roman(Hand& h,const char* s,float height,uint16_t c,float horizontalScale=1){
 // Condensed Roman lettering keeps long VIII/XXIII labels inside their sector.
 // Preserve radial height and stroke weight while scaling the whole word's
 // tangential coordinates, including its serifs and inter-letter spacing.
 auto line=[&](float x0,float y0,float x1,float y1,float width,uint16_t colour){
  h.line(x0*horizontalScale,y0,x1*horizontalScale,y1,width,colour);
 };
 float width=-.18f;for(const char* q=s;*q;q++)width+=romanWidth(*q)+.18f;
 float x=-width*height*.5f,H=height*.5f;
 for(;*s;s++){
  float w=romanWidth(*s)*height,mid=x+w*.5f,stroke=height*.115f;
  if(*s=='I'){
   line(mid,-H,mid,H,stroke,c);line(mid-height*.20f,-H,mid+height*.20f,-H,stroke*.7f,c);line(mid-height*.20f,H,mid+height*.20f,H,stroke*.7f,c);
  }else if(*s=='V'){
   line(x,H,mid,-H,stroke,c);line(mid,-H,x+w,H,stroke*.65f,c);
   line(x-height*.12f,H,x+height*.2f,H,stroke*.65f,c);line(x+w-height*.18f,H,x+w+height*.12f,H,stroke*.65f,c);
  }else{
   line(x,-H,x+w,H,stroke,c);line(x,H,x+w,-H,stroke*.65f,c);
   for(int v=0;v<2;v++){float yy=v?H:-H;line(x-height*.12f,yy,x+height*.18f,yy,stroke*.65f,c);line(x+w-height*.18f,yy,x+w+height*.12f,yy,stroke*.65f,c);}
  }
  x+=w+height*.18f;
 }
}
inline void romans(Painter& p,float radius,float height,uint16_t c,bool radialText=true,float horizontalScale=1){
 const char* values[]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};
 for(int i=0;i<12;i++){Hand letter(p,radialText?i/12.0f:0);letter.origin=radial(i*Pi/6,radius);roman(letter,values[i],height,c,horizontalScale);}
}
inline void arabics(uint16_t* out,const ClockFonts::Font& font,float radius,uint16_t colour){
 for(int i=0;i<12;i++){Point at=radial(i*Pi/6,radius);char s[3];int n=i?i:12;s[0]=n>=10?'1':'0'+n;s[1]=n>=10?'0'+n%10:0;s[2]=0;centred(out,font,s,lroundf(at.x),lroundf(at.y)+8,colour);}
}
inline void prepare(uint16_t* out,const uint16_t* bg){memcpy(out,bg,Size*Size*2);}
inline float hour(double t){return float(fmod(t,43200.0)/43200.0);}
inline float minute(double t){return float(fmod(t,3600.0)/3600.0);}
inline void lance(Hand& h,float length,float width,uint16_t colour,float tail=0){
 const Point shaft[]={{-width*.32f,-tail},{-width*.5f,length*.68f},{0,length},{width*.5f,length*.68f},{width*.32f,-tail}};
 h.poly(shaft,5,colour);
}
inline void lozenge(Hand& h,float length,float width,uint16_t c,const uint16_t* bg=nullptr){
 h.line(0,-12,0,length-8,width*.25f,c);
 Point outer[]={{0,length},{-width,length-12},{0,length-25},{width,length-12}};h.poly(outer,4,c);
 if(bg){Point inner[]={{0,length-5},{-width*.46f,length-12},{0,length-19},{width*.46f,length-12}};h.poly(inner,4,0,false,bg);}
}
inline void hub(Painter& p,uint16_t c,uint16_t centre,float r=4.2f){p.disk(120,120,r,c);p.disk(119.5f,119.5f,r*.42f,centre);}
inline void fleur(Painter& p,Point c,float r,uint16_t colour){
 Hand h(p,0);h.origin=c;
 Point bud[]={{0,r},{-r*.3f,r*.45f},{0,-r*.5f},{r*.3f,r*.45f}};h.poly(bud,4,colour);
 for(int side : {-1,1}){h.disk(side*r*.45f,r*.15f,r*.3f,colour);h.line(side*r*.6f,r*.1f,0,-r*.5f,r*.18f,colour);}
 h.line(-r*.5f,-r*.5f,r*.5f,-r*.5f,r*.22f,colour);h.line(0,-r*.5f,0,-r*.9f,r*.25f,colour);
}
// Tiny geometric zodiac emblems. Each is deliberately distinct at 9–12 px.
inline void zodiac(Painter& p,int sign,Point c,float r,uint16_t colour){
 Hand h(p,0);h.origin=c;float w=.8f;
 auto line=[&](float a,float b,float x,float y){h.line(a*r,b*r,x*r,y*r,w,colour);};
 switch(sign%12){
 case 0:line(0,-1,0,.6f);line(0,.6f,-.6f,1);line(0,.6f,.6f,1);h.disk(-.6f*r,.65f*r,.25f*r,colour);h.disk(.6f*r,.65f*r,.25f*r,colour);break;
 case 1:ring(p,c.x,c.y+r*.25f,r*.55f,w,colour);line(-.5f,.3f,-.8f,1);line(.5f,.3f,.8f,1);break;
 case 2:line(-.6f,-1,-.6f,1);line(.6f,-1,.6f,1);line(-1,-1,1,-1);line(-1,1,1,1);break;
 case 3:line(-1,.4f,.5f,.4f);line(-.5f,-.4f,1,-.4f);h.disk(-.65f*r,-.4f*r,.3f*r,colour);h.disk(.65f*r,.4f*r,.3f*r,colour);break;
 case 4:ring(p,c.x-r*.35f,c.y+r*.35f,r*.35f,w,colour);line(0,-.3f,.35f,.9f);line(.35f,.9f,.9f,.5f);line(.9f,.5f,.6f,-.7f);break;
 case 5:case 7:line(-.85f,-1,-.85f,1);line(-.3f,-1,-.3f,1);line(.3f,-1,.3f,1);line(-.85f,1,-.3f,.4f);line(-.3f,1,.3f,.4f);line(.3f,-.8f,.9f,-.3f);if(sign==7)line(.9f,-.3f,.9f,.1f);break;
 case 6:line(-1,-.7f,1,-.7f);line(-1,-.1f,1,-.1f);line(-.65f,-.1f,0,.8f);line(0,.8f,.65f,-.1f);break;
 case 8:line(-.8f,-.8f,.8f,.8f);line(.8f,.8f,.1f,.8f);line(.8f,.8f,.8f,.1f);line(-.7f,.4f,.4f,-.7f);break;
 case 9:line(-1,.8f,-.5f,-.6f);line(-.5f,-.6f,0,.8f);line(0,.8f,.5f,-.7f);ring(p,c.x+r*.6f,c.y+r*.4f,r*.4f,w,colour);break;
 case 10:for(int i=0;i<2;i++){float y=i?-.4f:.5f;line(-1,y,-.5f,y+.35f);line(-.5f,y+.35f,0,y);line(0,y,.5f,y+.35f);line(.5f,y+.35f,1,y);}break;
 case 11:line(-.65f,-1,-.35f,0);line(-.35f,0,-.65f,1);line(.65f,-1,.35f,0);line(.35f,0,.65f,1);line(-1,0,1,0);break;
 }
}
} } // namespace WatchFace::Detail
