"""Exercise real title decoder/preferences and metric toggle, plus host protocol."""
from pathlib import Path
import subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
from gym_control import commands,parse_status
source=(ROOT/'device_profiles/gym/BikeDashboard/DashboardUI.h').read_text()
code=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
uint8_t mainMetric=0;char displayTitle[25]="BIKE LAB";
std::string savedTitle;uint8_t savedMetric=0;
struct Preferences {
 bool begin(const char*,bool){return true;}void end(){}
 void putUChar(const char*,uint8_t value){savedMetric=value;}
 size_t putString(const char*,const char* value){savedTitle=value;return strlen(value);}
};
'''+source[source.index('void toggleMetric()'):source.index('void titleStatus()')]+r'''
int main(){
 toggleMetric();assert(mainMetric==1&&savedMetric==1);
 toggleMetric();assert(mainMetric==2&&savedMetric==2);
 toggleMetric();assert(mainMetric==0&&savedMetric==0);
 assert(setDisplayTitleHex("42696b65204c6162"));assert(savedTitle=="Bike Lab"&&!strcmp(displayTitle,"Bike Lab"));
 assert(!setDisplayTitleHex("0A"));assert(!setDisplayTitleHex("GG"));assert(!setDisplayTitleHex("2020"));
 assert(!setDisplayTitleHex("4"));assert(!setDisplayTitleHex(""));assert(savedTitle=="Bike Lab");
 std::string longest(48,'4');assert(setDisplayTitleHex(longest.c_str()));
 longest+="44";assert(!setDisplayTitleHex(longest.c_str()));
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
for title in ["Bike Lab","My Bike Lab",'x'*24]:
 packet=commands({'title':title})[0]
 assert len(packet)<=64
 assert parse_status(['TITLE hex='+packet.decode().split()[1]])['title']==title
for title in ['',' ','x'*25,'bad\ninput','emoji😈']:
 try:commands({'title':title})
 except ValueError:pass
 else:raise AssertionError('invalid title accepted')
print('PASS real firmware title decoder, persistence/toggle, host roundtrip and bounds')
