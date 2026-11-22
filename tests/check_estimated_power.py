"""Test firmware model and actual CPS packet encoder with real crank events."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]/'device_profiles/gym'
s=(root/'BikeDashboard/GymLink.h').read_text()
methods=s[s.index('  void setResistance('):s.index('  RideStats& ride()')]
code=r'''#include <cassert>
#include <cstdint>
#include <cmath>
#include "EstimatedPower.h"
#include "CyclingCadence.h"
#include "VirtualWheel.h"
uint64_t clockUs=0;uint64_t esp_timer_get_time(){return clockUs;}
class HostLink {public:CyclingCadence cadence;VirtualWheel wheel;double bluetoothJoules=0;int resistanceLevel=1;
'''+methods+r'''};
int main(){
 assert(estimatedWatts(0,8)==0&&estimatedWatts(NAN,1)==0);
 assert(estimatedWatts(60,1)==28&&estimatedWatts(60,8)==46);
 assert(estimatedWatts(80,1)==50&&estimatedWatts(90,8)==104);
 assert(estimatedWatts(1e20f,8)==32767);
 for(int r=1;r<8;r++)assert(estimatedWatts(90,r)<estimatedWatts(90,r+1));
 HostLink h;uint8_t p[16];h.powerPacket(p);assert(p[0]==0x30&&p[1]==8&&p[2]==0&&p[3]==0);
 h.cadence.pulse(1000000);h.cadence.pulse(2000000);clockUs=2000000;
 h.setResistance(8);h.powerPacket(p);
 assert(p[2]==46&&p[3]==0&&p[10]==2&&p[11]==0&&p[12]==0&&p[13]==8);
 // Stopping zeros power without fabricating crank events.
 clockUs=6000001;h.powerPacket(p);assert(p[2]==0&&p[10]==2&&p[13]==8);
 // PPR calibration controls both power and cadence; counter wrap remains legal.
 HostLink two;two.cadence.calibrate(2);
 for(uint64_t n=1;n<=4;n++)two.cadence.pulse(n*500000);
 clockUs=2000000;assert(two.realCadence()==60&&two.realPower()==28);
}
'''
with tempfile.TemporaryDirectory(prefix='gym-power-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'BikeDashboard'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS model, stop timeout, calibration and actual CPS packet encoder')
