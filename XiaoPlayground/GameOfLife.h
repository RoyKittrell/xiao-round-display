#pragma once
#include <stdint.h>
#include <string.h>

// Conway B3/S23: eight neighbours, simultaneous updates, dead outside a finite
// 128x128 world, beyond the visible viewport. The raster is shared with firmware and desktop previews.
class GameOfLife {
 public:
 static constexpr int Size=240,CellPixels=4,Columns=128,Rows=Columns;
 static constexpr int ViewColumns=Size/CellPixels,ViewRows=ViewColumns;
 static constexpr int ViewX=(Columns-ViewColumns)/2,ViewY=(Rows-ViewRows)/2;
 static constexpr int Cells=Columns*Rows,Bytes=(Cells+7)/8;
 static constexpr uint32_t StepMs=150,SettledMs=10000;
 private:
 uint8_t cells[Bytes]={},next[Bytes]={},previous[Bytes]={};
 uint32_t rng=1,lastStep=0,settledAt=0,generationCount=0,seedCount=0;
 uint16_t population=0;
 bool initialized=false,active=false,havePrevious=false,settling=false;
 static bool readBit(const uint8_t* data,int i){return (data[i>>3]>>(i&7))&1;}
 static void put(uint8_t* data,int i){data[i>>3]|=uint8_t(1u<<(i&7));}
 uint32_t randomWord(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
 public:
 bool valid(int x,int y) const{return x>=0&&x<Columns&&y>=0&&y<Rows;}
 bool cell(int x,int y) const{return x>=0&&x<Columns&&y>=0&&y<Rows&&readBit(cells,y*Columns+x);}
 void clear(uint32_t now){
  memset(cells,0,sizeof(cells));memset(previous,0,sizeof(previous));memset(next,0,sizeof(next));
  population=0;generationCount=0;lastStep=now;havePrevious=settling=false;initialized=true;
 }
 void setCell(int x,int y,bool alive=true){
  if(!valid(x,y))return;
  int i=y*Columns+x;bool was=readBit(cells,i);if(was==alive)return;
  cells[i>>3]^=uint8_t(1u<<(i&7));population=uint16_t(int(population)+(alive?1:-1));
 }
 void reseed(uint32_t now,uint32_t seed){
  clear(now);rng=seed?seed:0x6d2b79f5u;seedCount++;
  for(int i=0;i<Cells;i++)if(randomWord()%100<25){put(cells,i);population++;}
 }
 void enter(uint32_t now,uint32_t seed){
  if(!initialized)reseed(now,seed);
  active=true;lastStep=now;settling=false;
 }
 void leave(){active=false;}
 bool running() const{return active;}
 uint32_t generation() const{return generationCount;}
 uint32_t seeds() const{return seedCount;}
 uint16_t alive() const{return population;}
 uint16_t available() const{return Cells;}
 void step(uint32_t now){
  lastStep=now;memset(next,0,sizeof(next));uint16_t count=0;
  for(int y=0;y<Rows;y++)for(int x=0;x<Columns;x++){
   const int i=y*Columns+x;
   int neighbours=0;
   for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy)neighbours+=cell(x+dx,y+dy);
   if(neighbours==3||(neighbours==2&&readBit(cells,i))){put(next,i);count++;}
  }
  bool repeated=!memcmp(next,cells,Bytes)||(havePrevious&&!memcmp(next,previous,Bytes));
  if(repeated){if(!settling){settling=true;settledAt=now;}}
  else settling=false;
  memcpy(previous,cells,Bytes);memcpy(cells,next,Bytes);havePrevious=true;
  population=count;generationCount++;
 }
 bool advance(uint32_t now){
  if(!active||uint32_t(now-lastStep)<StepMs)return false;
  step(now);
  if(!population||(settling&&uint32_t(now-settledAt)>=SettledMs))reseed(now,randomWord());
  return true;
 }
 void render(uint16_t* out) const{
  memset(out,0,Size*Size*sizeof(uint16_t));
  for(int y=0;y<ViewRows;y++)for(int x=0;x<ViewColumns;x++)if(cell(x+ViewX,y+ViewY)){
   // Edge-to-edge crop: no circle mask, border, gutters or hidden frame.
   for(int dy=0;dy<CellPixels;dy++)for(int dx=0;dx<CellPixels;dx++)
    out[(y*CellPixels+dy)*Size+x*CellPixels+dx]=0xffff;
  }
 }
};
