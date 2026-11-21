"""Measure the actual menu callbacks off-screen, without starting a worker.

This excludes macOS popup animation and physical pointer movement. It measures
our own callback work and verifies opening reuses the existing checkbox view.
"""
from pathlib import Path
import subprocess,tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'companion/menu_bar/MenuBar.swift').read_text().split('@main struct HelperMain {')[0]
harness=r'''
@main struct Benchmark {
 static func main() throws {
  _ = NSApplication.shared
  let delegate=AppDelegate()
  delegate.scenes=try decoder.decode([Scene].self,from:Data(contentsOf:URL(fileURLWithPath:CommandLine.arguments[1])))
  delegate.profileStore=try ProfileStore(url:URL(fileURLWithPath:"/tmp/xiao-benchmark-unused-\(UUID().uuidString).json"))
  delegate.connectionSettings=["device_id":"usb:001122334455","selection_epoch":"one"]
  delegate.health=["selected_id":"usb:001122334455","selection":"one","hardware":["role":"desktop"],"usb_connected":true,"updated_at":Date().timeIntervalSince1970,
   "sources":["codex":["label":"Codex","source_observed_at":Date().timeIntervalSince1970],
   "bambu":["label":"Bambu Printer","source_observed_at":Date().timeIntervalSince1970],
   "sun":["label":"Sun","source_observed_at":Date().timeIntervalSince1970]]]
  delegate.device=["scene":2,"revision":UInt32(1),"day_brightness":0,"night_brightness":0,"sleeping":0]
  var t=CFAbsoluteTimeGetCurrent();delegate.rebuild()
  let buildMS=(CFAbsoluteTimeGetCurrent()-t)*1000
  let view=delegate.carouselChecklist!
  var samples:[Double]=[]
  for _ in 0..<500 {
   t=CFAbsoluteTimeGetCurrent();delegate.menuWillOpen(delegate.menu)
   samples.append((CFAbsoluteTimeGetCurrent()-t)*1000)
   assert(delegate.carouselChecklist === view)
  }
  assert(delegate.menuActivity != nil)
  let activity=delegate.menuActivity!
  delegate.menuWillOpen(delegate.menu)
  assert(delegate.menuActivity === activity)
  let other=NSMenu();delegate.menuDidClose(other)
  assert(delegate.menuIsOpen && delegate.menuActivity != nil)
  delegate.menuDidClose(delegate.menu)
  assert(!delegate.menuIsOpen && delegate.menuActivity == nil)
  delegate.menuDidClose(delegate.menu) // Idempotent activity cleanup.
  view.update(mask:384);t=CFAbsoluteTimeGetCurrent()
  for _ in 0..<10000{view.update(mask:384)}
  let unchangedMS=(CFAbsoluteTimeGetCurrent()-t)*1000/10000
  assert(view.buttons.filter{$0.state == .on}.map{$0.tag} == [9,10])
  assert(view.allButton.state == .mixed)
  view.update(mask:delegate.scenes.reduce(0){$0 | (1 << ($1.id-2))});assert(view.allButton.state == .on)
  view.update(mask:0);assert(view.allButton.state == .off && view.buttons.allSatisfy{$0.state == .off && $0.isEnabled})
  view.update(mask:128);assert(view.buttons.first{$0.tag==9}!.isEnabled)
  samples.sort()
  let output:[String:Any]=["initial_build_ms":buildMS,"cached_open_median_ms":samples[samples.count/2],
   "cached_open_max_ms":samples.last!,"unchanged_checklist_update_ms":unchangedMS,
   "menu_opens_measured":samples.count,"view_reused":true,"all_none_mixed_states":true,"menu_activity_balanced":true]
  print(String(data:try JSONSerialization.data(withJSONObject:output,options:[.prettyPrinted,.sortedKeys]),encoding:.utf8)!)
 }
}
'''
with tempfile.TemporaryDirectory(prefix='xiao-menu-benchmark-',dir='/tmp') as tmp:
    src=Path(tmp)/'Benchmark.swift';src.write_text(source+harness)
    exe=Path(tmp)/'benchmark'
    subprocess.run(['swiftc','-swift-version','5','-module-cache-path','/tmp/xiao-swift-cache','-O',str(src),str(root/'companion/menu_bar/Profiles.swift'),str(root/'companion/menu_bar/BluetoothSearch.swift'),str(root/'companion/menu_bar/WeatherLocation.swift'),str(root/'companion/menu_bar/WifiSetup.swift'),str(root/'companion/menu_bar/Devices.swift'),str(root/'companion/menu_bar/GymMenu.swift'),'-o',str(exe),'-framework','AppKit','-framework','CoreLocation','-framework','CoreWLAN'],check=True)
    subprocess.run([str(exe),str(root/'companion/scenes.json')],check=True)
