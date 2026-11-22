"""Production native Gym/device menus, aliases and queued-edit isolation; no worker."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'companion/menu_bar/MenuBar.swift').read_text().split('@main struct HelperMain {')[0]
harness=r'''
@main struct Check {
 static func main()throws{
  _=NSApplication.shared
  let gym="usb:001122334455",desk="usb:aabbccddeeff"
  let delegate=AppDelegate()
  delegate.connectionSettings=["device_id":gym,"usb_serial":"001122334455","mode":"usb","selection_epoch":"one"]
  delegate.catalogState=["devices":[gym:["id":gym,"role":"gym","usb_present":true,"usb_serial":"001122334455","usb_device":"/dev/cu.synthetic-gym"],desk:["id":desk,"role":"desktop","usb_present":false,"usb_serial":"aabbccddeeff"]]]
  delegate.health=["selected_id":gym,"selection":"one","display_connected":true,"updated_at":Date().timeIntervalSince1970,"hardware":["role":"gym","model":"Gym Xiao"]]
  delegate.gymState=["device_id":gym,"selection":"one","ready":true,"glow":["peak":32,"centre":29,"high_floor":68],"brightness":77,"demo":true,"title":"Bike Lab"]
  assert(object(delegate.deviceDataRoot.appendingPathComponent("namespace-fixture.json"))["id"] as? String==gym)
  delegate.rebuild()
  assert(delegate.menu.items[0].title.hasPrefix("Device: "))
  let firmwareMenu=delegate.menu.items.first{$0.title=="Firmware"}!.submenu!
  assert(!firmwareMenu.items.first{$0.title=="Update Firmware over Wi-Fi…"}!.isEnabled)

  let deviceMenu=delegate.menu.items[0].submenu!
  assert(deviceMenu.items.contains{$0.title=="Device Settings…"})
  assert(!deviceMenu.items.contains{$0.title.contains("Rename")})
  var capable=delegate.hardware;capable["ota_supported"]=true;delegate.health["hardware"]=capable
  delegate.connectionSettings["wifi_host"]="xiao-display-334455.local";delegate.rebuild()
  assert(delegate.menu.items.first{$0.title=="Firmware"}!.submenu!.items.first{$0.title=="Update Firmware over Wi-Fi…"}!.isEnabled)
  delegate.gymState["units"]="metric";delegate.gymState["distance_mm"]=5000;delegate.gymState["ride"]=["distance_m":500.0,"work_kj":2.5];delegate.rebuild()
  let rideMenu=delegate.menu.items.first{$0.title=="Ride Data"}!.submenu!
  assert(rideMenu.items.first{$0.title=="Kilometres · km/h"}!.state == .on)
  assert(rideMenu.items.first{$0.title=="Start New Ride…"}!.isEnabled)
  assert(rideMenu.items[0].title.contains("0.500"))
  let originalHealth=delegate.health
  for family in ["esp32c6","nrf54"] {
    var manual=delegate.hardware;manual["family"]=family;delegate.health["hardware"]=manual
    delegate.rebuild()
    let items=delegate.menu.items.first{$0.title=="Firmware"}!.submenu!.items
    assert(items.allSatisfy{!$0.isEnabled})
    assert(!items.contains{$0.title=="Update Firmware over Wi-Fi…"})
  }
  delegate.health=originalHealth;delegate.rebuild()
  let (settingsView,nameField,profilePicker)=delegate.deviceSettingsView()
  let titleField=settingsView.viewWithTag(501) as! NSTextField
  assert(titleField.isEnabled && titleField.stringValue=="Bike Lab")
  assert(nameField.stringValue==delegate.nameForDevice(gym))
  assert(profilePicker.itemTitles==["Desktop","Gym Display"])
  assert(profilePicker.selectedItem?.representedObject as? String=="gym")
  profilePicker.selectItem(at:0)
  assert(delegate.selectedRole=="gym") // A dropdown edit alone cannot change firmware/role.

  assert(delegate.menu.items[0].submenu!.items.contains{$0.title.contains("disconnected")})
  assert(delegate.menu.items.contains{$0.title=="Pulses"});assert(delegate.menu.items.contains{$0.title=="Demo Mode"})
  assert(!delegate.menu.items.contains{$0.title=="Scenes" || $0.title=="Profiles" || $0.title=="Data Sources"})
  let connections=delegate.menu.items.first{$0.title=="Connections"}!.submenu!
  assert(connections.items.filter{$0.title.contains("needs Gym firmware")}.count==2)
  assert(connections.items.filter{$0.title.contains("needs Gym firmware")}.allSatisfy{!$0.isEnabled})
  // r11 enables wireless controls; r10 above retains the explicit upgrade labels.
  delegate.health["hardware"]=["role":"gym","model":"Gym Xiao","bluetooth":true,"wifi_hardware":true,"wifi_firmware":true]
  delegate.rebuild()
  let wireless=delegate.menu.items.first{$0.title=="Connections"}!.submenu!
  assert(wireless.items.first{$0.title=="Set Up Wi-Fi via USB…"}!.isEnabled)
  assert(wireless.items.first{$0.title=="Find Bluetooth Display…"}!.isEnabled)
  assert(!wireless.items.first{$0.title=="Wi-Fi"}!.isEnabled)
  assert(!wireless.items.first{$0.title=="Bluetooth"}!.isEnabled)
  // Provisioning uses the verified USB cable even while controls use Bluetooth.
  delegate.health["transport"]="bluetooth";delegate.rebuild()
  assert(delegate.menu.items.first{$0.title=="Connections"}!.submenu!.items.first{$0.title=="Set Up Wi-Fi via USB…"}!.isEnabled)
  var withoutUSB=delegate.deviceCatalog
  withoutUSB[gym]?["usb_present"]=false;delegate.catalogState=["devices":withoutUSB];delegate.rebuild()
  assert(!delegate.menu.items.first{$0.title=="Connections"}!.submenu!.items.first{$0.title=="Set Up Wi-Fi via USB…"}!.isEnabled)
  withoutUSB[gym]?["usb_present"]=true;delegate.catalogState=["devices":withoutUSB]
  delegate.health["transport"]="usb";delegate.rebuild()
  // Watch firmware removes the Mac Bluetooth connector and explains pairing.
  delegate.health["hardware"]=["role":"gym","model":"Gym Xiao","bluetooth":true,"wifi_hardware":true,"wifi_firmware":true,"watch_csc":true]
  delegate.rebuild()
  let watchConnections=delegate.menu.items.first{$0.title=="Connections"}!.submenu!
  assert(watchConnections.items.first{$0.title=="Bluetooth · Apple Watch"}!.submenu!.items.contains{$0.title=="Connect Apple Watch…"})
  assert(!watchConnections.items.contains{$0.title=="Find Bluetooth Display…" || $0.title=="Bluetooth"})
  assert(watchConnections.items.contains{$0.title=="Automatic · USB, then Wi-Fi"})
  assert(delegate.gymSliders.map{Int($0.slider.minValue)}==[1,5,0,0])
  assert(delegate.gymSliders.map{Int($0.slider.doubleValue)}==[32,29,68,77])
  try delegate.saveDeviceName("My Gym Bike",id:gym)
  let fresh=AppDelegate();fresh.loadDeviceState();assert(fresh.nameForDevice(gym)=="My Gym Bike")
  do{try delegate.saveDeviceName("\n",id:gym);fatalError("Bad alias accepted")}catch{}
  // An ACK for the previous packet must not clear a newer unsent edit.
  delegate.gymPendingToken="old";delegate.gymPendingValues=["brightness":20]
  delegate.gymState["last_token"]="old";delegate.gymDesired=["brightness":44]
  delegate.scheduleGymAction();delegate.updateGymControls()
  assert(delegate.gymDesired["brightness"] as? Int==44)
  RunLoop.current.run(until:Date().addingTimeInterval(0.3))
  let queued=object(dataRoot.appendingPathComponent("gym-actions.json"))[gym] as! [String:Any]
  assert((queued["values"] as! [String:Int])["brightness"]==44)
  assert(queued["target"] as? String==gym && queued["selection"] as? String=="one")
  // Switching cancels unsent/pending intents and preserves each connection.
  delegate.connectionSettings["wifi_host"]="xiao-display-334455.local"
  delegate.gymDesired=["demo":false];delegate.scheduleGymAction()
  delegate.selectDevice(desk,record:delegate.deviceCatalog[desk]!)
  assert(delegate.gymDesired.isEmpty && delegate.gymPendingValues.isEmpty && delegate.gymSendWork==nil)
  RunLoop.current.run(until:Date().addingTimeInterval(0.3))
  let unchanged=object(dataRoot.appendingPathComponent("gym-actions.json"))[gym] as! [String:Any]
  assert(unchanged["token"] as? String==queued["token"] as? String)
  assert(delegate.connectionSettings["wifi_host"]==nil)
  delegate.selectDevice(gym,record:delegate.deviceCatalog[gym]!)
  assert(delegate.connectionSettings["wifi_host"]=="xiao-display-334455.local")
  assert(delegate.selectionEpoch != "one")
  // Menu selection loads independent saved settings, without touching peers.
  delegate.prefs.enabledMask=128;assert(delegate.save());let gymRoot=delegate.deviceDataRoot
  delegate.selectDevice(desk,record:delegate.deviceCatalog[desk]!)
  assert(delegate.deviceDataRoot != gymRoot);delegate.prefs.enabledMask=256;assert(delegate.save())
  delegate.selectDevice(gym,record:delegate.deviceCatalog[gym]!)
  assert(delegate.prefs.enabledMask==128)
  // A stale prior-device snapshot cannot restore wrong connected menus.
  delegate.health=["selected_id":desk,"selection":"old","display_connected":true,"updated_at":Date().timeIntervalSince1970,"hardware":["role":"desktop"]]
  assert(!delegate.connected && delegate.hardware.isEmpty && delegate.selectedRole=="unknown")
  print("PASS native Gym menus: roles, limits, aliases, offline devices, ACK/edit race, cancelled selection, pairing restoration and stale-health isolation")
 }
}
'''
with tempfile.TemporaryDirectory(prefix='xiao-gym-menu-',dir='/tmp') as tmp:
 root=Path(tmp);src=root/'Check.swift';src.write_text(source+harness);exe=root/'check'
 modules=['BluetoothSearch','Profiles','WeatherLocation','WifiSetup','Devices','GymMenu']
 subprocess.run(['swiftc','-swift-version','5','-module-cache-path','/tmp/xiao-swift-cache','-O',str(src),*[str(ROOT/'companion/menu_bar'/f'{x}.swift') for x in modules],'-o',str(exe),'-framework','AppKit','-framework','CoreLocation','-framework','CoreWLAN'],check=True)
 import sys,json
 sys.path.insert(0,str(ROOT/'companion'))
 from multi_device import device_root
 fixture=device_root(root/'data','usb:001122334455');fixture.mkdir(parents=True)
 (fixture/'namespace-fixture.json').write_text(json.dumps({'id':'usb:001122334455'}))
 env={**os.environ,'XIAO_DISPLAY_DATA_DIR':str(root/'data')}
 subprocess.run([str(exe)],env=env,check=True)
