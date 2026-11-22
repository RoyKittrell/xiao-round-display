#include <cassert>
#include <cmath>
#include "../device_profiles/gym/BikeDashboard/RideStats.h"
#include "../device_profiles/gym/BikeDashboard/CyclingCadence.h"
#include "../device_profiles/gym/BikeDashboard/VirtualWheel.h"
bool close(double a,double b){return fabs(a-b)<0.0001;}
int main(){
 RideStats r;for(uint64_t i=1;i<=100;i++)r.pulse(i*1000000,60,28,5);
 assert(r.data().revolutions==100&&close(r.data().metres,500));
 assert(r.data().movingUs==99000000&&r.data().elapsedUs==99000000);
 assert(close(r.data().joules,28*99));assert(close(r.averageWatts(),28));assert(close(r.averageRpm(),60));assert(close(r.averageKph(),18));assert(close(r.overallKph(),18));
 r.tick(110000000);assert(r.data().elapsedUs==109000000&&r.data().movingUs==99000000&&close(r.data().joules,2772));
 auto saved=r.data();RideStats resumed;assert(resumed.restore(saved));resumed.tick(1000000000);assert(resumed.data().elapsedUs==saved.elapsedUs);
 resumed.tick(1001000000);assert(resumed.data().elapsedUs==saved.elapsedUs+1000000);
 r.pulse(111000000,0,0,5);assert(r.data().movingUs==99000000);r.pulse(112000000,60,46,6);assert(close(r.data().metres,511)&&close(r.data().joules,2818));
 auto bad=saved;bad.metres=NAN;assert(!resumed.restore(bad));r.reset();assert(r.data().revolutions==0&&r.data().joules==0);
 CyclingCadence c;c.pulse(1000000);c.pulse(2000000);assert(c.rpm(2000000)==60);c.pulse(10000000);assert(c.rpm(10000000)==0);c.pulse(11000000);assert(c.rpm(11000000)==60);
 VirtualWheel w;w.pulse(1000000,0,5);w.pulse(2000000,60,5);w.pulse(3000000,60,5);
 assert(w.count()==7);assert(w.time1024()==uint16_t(2940000ULL*1024/1000000));assert(w.time2048()==uint16_t(2940000ULL*2048/1000000));
 uint8_t csc[11],power[16];w.csc(csc,3,3072);w.power(power,28,3,3072,27);
 assert(csc[0]==3&&csc[1]==7&&csc[7]==3&&csc[9]==0&&csc[10]==12);
 assert(power[0]==0x30&&power[1]==8&&power[14]==27&&power[15]==0&&power[2]==28&&power[4]==7&&power[10]==3&&power[12]==0&&power[13]==12);
 // Calibrating future travel or resetting ride totals cannot rewind wheel events.
 auto before=w.count();w.pulse(4000000,60,6);assert(w.count()>before);
 VirtualWheel wrapped;wrapped.pulse(64000000,0,2.1);assert(wrapped.time1024()==0&&wrapped.time2048()==0);
}
