#include <cassert>
#include <cstdint>
#include "../device_profiles/gym/BikeDashboard/CyclingCadence.h"
int main(){
 CyclingCadence live;assert(live.rpm(0)==0);
 live.pulse(1000000);assert(live.rpm(1000000)==0);
 live.pulse(2000000);assert(live.rpm(2000000)==60);
 assert(live.rpm(6000001)==0);
 live.calibrate(2);assert(live.rpm(2000000)==0);
 live.pulse(3000000);live.pulse(4000000);assert(live.rpm(4000000)==0);
 live.pulse(5000000);live.pulse(6000000);assert(live.rpm(6000000)==30);
 CyclingCadence c;uint8_t data[5];c.measurement(data);
 assert(data[0]==2&&data[1]==0&&data[3]==0);
 c.pulse(1000000);c.measurement(data);
 assert(data[1]==1&&data[2]==0&&data[3]==0&&data[4]==4);
 c.pulse(64000000);assert(c.time()==0&&c.count()==2);
 uint64_t afterMicrosWrap=4294967296ULL+500000;
 c.pulse(afterMicrosWrap);assert(c.time()==uint16_t(afterMicrosWrap*1024ULL/1000000ULL));
 CyclingCadence two;two.calibrate(2);two.pulse(1000000);assert(two.count()==0);
 two.pulse(2000000);assert(two.count()==1&&two.time()==2048);
 two.calibrate(0);assert(two.calibration()==2);two.calibrate(17);assert(two.calibration()==2);
 // Recalibration discards a partial turn, retaining cumulative revolutions.
 two.pulse(3000000);two.calibrate(1);two.pulse(4000000);assert(two.count()==2);
 CyclingCadence wrapping;for(uint32_t n=1;n<=65536;++n)wrapping.pulse(uint64_t(n)*1000000ULL);
 assert(wrapping.count()==0);wrapping.measurement(data);assert(data[1]==0&&data[2]==0);
 // No physical pulses means an unchanging measurement, including time.
 c.measurement(data);uint8_t before[5];for(int n=0;n<5;++n)before[n]=data[n];
 c.measurement(data);for(int n=0;n<5;++n)assert(data[n]==before[n]);
}
