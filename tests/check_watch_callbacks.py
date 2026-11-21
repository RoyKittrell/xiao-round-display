"""Exercise the actual radio callbacks that previously caused watch reconnects."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]/'device_profiles/gym'
source=(root/'BikeDashboard/GymLink.h').read_text()
callbacks=source[source.index('  class ServerCallbacks:'):source.index('  class MeasurementCallbacks:')]
code=r'''
#include <cassert>
#include <cstdint>
int changed=0,advertised=0;
void ble_svc_gatt_changed(int,int){++changed;}
struct ble_gap_conn_desc {uint16_t conn_handle;struct {bool encrypted;bool authenticated;} sec_state;};
struct BLEServer {int disconnected=-1;void disconnect(uint16_t id){disconnected=id;}};
struct BLEDevice {static void startAdvertising(){++advertised;}};
struct BLEServerCallbacks {
 virtual void onConnect(BLEServer*,ble_gap_conn_desc*){}
 virtual void onDisconnect(BLEServer*,ble_gap_conn_desc*){}
};
struct BLESecurityCallbacks {
 virtual bool onSecurityRequest(){return false;}
 virtual void onAuthenticationComplete(ble_gap_conn_desc*,int){}
};
class HostLink {
public:
 static HostLink* instance;
 volatile bool connected=false,subscribed=false,powerSubscribed=false,encrypted=false;
 volatile uint16_t connection=0xffff;
 volatile int securityStatus=-1;
 volatile uint32_t connects=0,disconnects=0;
'''+callbacks+r'''
};
HostLink* HostLink::instance=nullptr;
int main(){
 HostLink h;HostLink::instance=&h;BLEServer server;
 BLEServerCallbacks* link=&h.serverCallbacks;BLESecurityCallbacks* security=&h.securityCallbacks;
 ble_gap_conn_desc watch={7,{true,false}}; // encrypted Just Works, no MITM passkey
 link->onConnect(&server,&watch);assert(h.connected&&h.connection==7);
 security->onAuthenticationComplete(&watch,0);assert(h.encrypted&&h.securityStatus==0);
 assert(server.disconnected==-1&&changed==0);
 ble_gap_conn_desc other={8,{false,false}};link->onConnect(&server,&other);
 assert(server.disconnected==8&&h.connection==7);
 h.subscribed=true;h.powerSubscribed=true;link->onDisconnect(&server,&watch);
 assert(!h.connected&&!h.subscribed&&!h.powerSubscribed&&h.connection==0xffff&&advertised==1);
 link->onConnect(&server,&watch);security->onAuthenticationComplete(&watch,0);
 assert(h.connects==2&&h.disconnects==1&&changed==0);
}
'''
with tempfile.TemporaryDirectory(prefix='gym-watch-callbacks-') as tmp:
    path=Path(tmp);(path/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS actual Watch callbacks: Just Works accepted; reconnects do not invalidate GATT')
