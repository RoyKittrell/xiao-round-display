#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <deque>
#include <vector>
struct Print{virtual size_t write(uint8_t)=0;};
struct Stream:Print{virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;virtual size_t write(uint8_t)=0;};
inline constexpr int HEX=16;inline uint32_t getFreeHeap(){return 10000;}
struct USB {template<class T>void print(T){}template<class T>void print(T,int){}template<class T>void println(T){}std::deque<uint8_t> in;std::vector<uint8_t> out;void begin(unsigned long){}int available(){return in.size();}int read(){if(in.empty())return -1;int c=in.front();in.pop_front();return c;}int peek(){return in.empty()?-1:in.front();}void flush(){}size_t write(uint8_t c){out.push_back(c);return 1;}};
inline USB Serial;inline uint32_t now=0;inline uint32_t millis(){return now;}inline void yield(){}
