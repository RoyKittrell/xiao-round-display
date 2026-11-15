"""Check the real Swift discovery subprocess with fake scripts; no radio access."""
from pathlib import Path
import subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[1]
harness=r'''
import Foundation
@main struct Check {
 static func main(){
  let python=CommandLine.arguments[1],folder=URL(fileURLWithPath:CommandLine.arguments[2])
  let good=BluetoothSearch.run(python:python,script:folder.appendingPathComponent("good.py"),timeout:5)
  assert((good["devices"] as? [[String:String]])?.count==1)
  let bad=BluetoothSearch.run(python:python,script:folder.appendingPathComponent("bad.py"),timeout:5)
  assert(bad["error"] != nil)
  let started=ProcessInfo.processInfo.systemUptime
  let blocked=BluetoothSearch.run(python:python,script:folder.appendingPathComponent("blocked.py"),timeout:0.2)
  assert((blocked["error"] as? String)?.contains("timed out")==true)
  assert(ProcessInfo.processInfo.systemUptime-started<4)
  print("PASS native Bluetooth discovery: result decoding, failed child and bounded timeout/cleanup; no radio used")
 }
}
'''
with tempfile.TemporaryDirectory(prefix='xiao-search-check-',dir='/tmp') as tmp:
    folder=Path(tmp);(folder/'good.py').write_text('print(\'{"devices":[{"name":"Test display","id":"example"}]}\')\n')
    (folder/'bad.py').write_text('raise SystemExit(2)\n')
    (folder/'blocked.py').write_text('import signal,time\nsignal.signal(signal.SIGTERM,signal.SIG_IGN)\ntime.sleep(20)\n')
    src=folder/'Check.swift';src.write_text(harness);exe=folder/'check'
    subprocess.run(['swiftc','-swift-version','5','-module-cache-path','/tmp/xiao-swift-cache',str(ROOT/'companion/menu_bar/BluetoothSearch.swift'),str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe),sys.executable,tmp],check=True,timeout=15)
