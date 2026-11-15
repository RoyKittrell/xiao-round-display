#pragma once
#include <stdint.h>
// Keep only the fingerprint of each last-sent 8x8 tile, saving110,700 bytes
// against a second RGB565 canvas. The periodic panel recovery also repaints
// unchanged content. Only completed transfers advance the sent fingerprints.
class FrameTiles {
 static constexpr int Tile=8,Columns=30;
 uint32_t sent[Columns*Columns]={};
 bool valid[Columns*Columns]={};
 static uint32_t hash(const uint16_t* frame,int tx,int ty){
  uint32_t value=2166136261u;
  for(int y=ty*Tile;y<(ty+1)*Tile;y++)for(int x=tx*Tile;x<(tx+1)*Tile;x++){
   value^=frame[y*240+x];value*=16777619u;
  }
  return value;
 }
 public:
 void invalidate(){for(auto& v:valid)v=false;}
 template<class Send> bool flush(const uint16_t* frame,bool force,Send send){
  uint32_t row[Columns];
  for(int y=0;y<Columns;y++){
   for(int x=0;x<Columns;x++)row[x]=hash(frame,x,y);
   for(int x=0;x<Columns;){
    int index=y*Columns+x;
    if(!force&&valid[index]&&sent[index]==row[x]){x++;continue;}
    int begin=x++;
    while(x<Columns&&(force||!valid[y*Columns+x]||sent[y*Columns+x]!=row[x]))x++;
    if(!send(begin*Tile,y*Tile,x*Tile-1,y*Tile+Tile-1))return false;
    for(int i=begin;i<x;i++){sent[y*Columns+i]=row[i];valid[y*Columns+i]=true;}
   }
  }
  return true;
 }
};
