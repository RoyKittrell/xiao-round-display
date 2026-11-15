#pragma once
#include <math.h>
#include <stdint.h>
#include "Parameters.h"
namespace StarSystems {
constexpr float Pi=3.14159265358979323846f,Tau=2*Pi;
enum class System : uint8_t {TYC,TIC,HR,Count};
constexpr int Count=int(System::Count);
struct Vec {float x=0,y=0;};
inline Vec add(Vec a,Vec b){return {a.x+b.x,a.y+b.y};}
inline Vec mul(Vec a,float s){return {a.x*s,a.y*s};}
struct Matrix {float xx,xy,yx,yy,b;};
struct Geometry {
 Matrix matrix[Data::OrbitCount];
 Geometry(){for(int k=0;k<Data::OrbitCount;k++){
  auto e=Data::Orbits[k];float w=e.arg*Pi/180,i=e.i*Pi/180,o=e.node*Pi/180;
  float cw=cosf(w),sw=sinf(w),ci=cosf(i),co=cosf(o),so=sinf(o);
  matrix[k]={cw*co-sw*ci*so,-sw*co-cw*ci*so,cw*so+sw*ci*co,-sw*so+cw*ci*co,sqrtf(1-e.e*e.e)};
 }}
};
inline const Geometry& geometry(){static const Geometry cache;return cache;}
inline Vec unitAtE(int id,float E){
 const auto& e=Data::Orbits[id];const auto& m=geometry().matrix[id];
 float x=cosf(E)-e.e,y=m.b*sinf(E);return {m.xx*x+m.xy*y,m.yx*x+m.yy*y};
}
// Match alpha.js's projected Kepler equation. Double precision is confined to
// phase reduction so days of playback retain smooth sub-second orbital phase.
inline Vec unitOrbit(int id,double days){
 const auto& e=Data::Orbits[id];float M=float(fmod((days-e.peri)/e.period,1.0)*Tau),E=M;
 for(int i=0;i<8;i++){float d=(E-e.e*sinf(E)-M)/(1-e.e*cosf(E));E-=d;if(fabsf(d)<1e-6f)break;}
 return unitAtE(id,E);
}
inline Vec orbit(int id,double days){return mul(unitOrbit(id,days),Data::Orbits[id].a);}
inline void split(Vec relative,float m1,float m2,Vec centre,Vec& a,Vec& b){
 a=add(centre,mul(relative,-m2/(m1+m2)));b=add(centre,mul(relative,m1/(m1+m2)));
}
struct State {Vec stars[6],centres[3],ac;int count=0;};
inline float scale(System system){return system==System::HR?105.0f:110.0f;}
inline State state(System system,double seconds){
 using namespace Data;State out;const float* m=Masses[int(system)];
 if(system==System::TYC){
  float ma=m[0]+m[1],mc=m[2]+m[3],mb=m[4]+m[5];
  split(unitOrbit(Wide,seconds*Orbits[Wide].period/WideSeconds),ma+mc,mb,{},out.ac,out.centres[2]);
  split(mul(unitOrbit(Quad,seconds*Orbits[Quad].period/QuadSeconds),.55f),ma,mc,out.ac,out.centres[0],out.centres[1]);
  for(int k=0;k<3;k++)split(mul(unitOrbit(PairA+k,seconds*PairDaysPerSecond),.22f),m[2*k],m[2*k+1],out.centres[k],out.stars[2*k],out.stars[2*k+1]);
  out.count=6;
 }else if(system==System::TIC){
  split(unitOrbit(TicOuter,seconds*TicDaysPerSecond),m[0]+m[1],m[2],{},out.centres[0],out.stars[2]);
  split(mul(unitOrbit(TicInner,seconds*TicDaysPerSecond),.36f),m[0],m[1],out.centres[0],out.stars[0],out.stars[1]);out.count=3;
 }else{
  split(unitOrbit(Hr,seconds*HrDaysPerSecond),m[0],m[1],{},out.stars[0],out.stars[1]);out.count=2;
 }
 return out;
}
} // namespace StarSystems
