import AppKit
import CoreWLAN

extension AppDelegate {
    @objc func setupWifi(_ sender:NSMenuItem){
        guard !wifiSetupRunning else{return}
        guard wifiFirmware else{showError(wifiHardware ? "This ESP32 needs Wi-Fi firmware before setup.":"The connected controller does not support Wi-Fi. Use USB or Bluetooth.");return}
        guard let serial=connectionSettings["usb_serial"],serial.filter({$0.isHexDigit}).count==12 else{
            showError("Choose the ESP32 display under Connection → Choose USB Display first. Wi-Fi setup needs USB once; the original nRF54 display has no Wi-Fi.");return
        }
        NSApp.activate(ignoringOtherApps:true)
        let alert=NSAlert();alert.messageText="Connect display to Wi-Fi"
        alert.informativeText="Keep your S3 or C6 connected by USB, with its antenna attached. Use a 2.4 GHz WPA2/WPA3 Personal network. The password goes directly to the display and is not saved on this Mac."
        let view=NSView(frame:NSRect(x:0,y:0,width:370,height:112))
        let nameLabel=NSTextField(labelWithString:"Wi-Fi network name")
        nameLabel.frame=NSRect(x:0,y:89,width:360,height:18);view.addSubview(nameLabel)
        let name=NSTextField(frame:NSRect(x:0,y:61,width:370,height:24))
        name.stringValue=CWWiFiClient.shared().interface()?.ssid() ?? "";view.addSubview(name)
        let passwordLabel=NSTextField(labelWithString:"Wi-Fi password")
        passwordLabel.frame=NSRect(x:0,y:34,width:360,height:18);view.addSubview(passwordLabel)
        let password=NSSecureTextField(frame:NSRect(x:0,y:4,width:370,height:24));view.addSubview(password)
        alert.accessoryView=view;alert.addButton(withTitle:"Connect");alert.addButton(withTitle:"Cancel")
        alert.window.initialFirstResponder=name.stringValue.isEmpty ? name:password
        guard alert.runModal() == .alertFirstButtonReturn else{return}
        let ssid=name.stringValue,key=password.stringValue;password.stringValue=""
        guard !ssid.isEmpty,!key.isEmpty else{showError("Enter the network name and password.");return}
        wifiSetupRunning=true;prepareMenuIfNeeded()
        let python=Bundle.main.object(forInfoDictionaryKey:"XIAOPythonPath") as? String ?? ""
        let script=Bundle.main.resourceURL!.appendingPathComponent("companion/wifi_setup.py")
        DispatchQueue.global(qos:.userInitiated).async{[weak self] in
            let process=Process(),input=Pipe(),output=Pipe()
            process.executableURL=URL(fileURLWithPath:python);process.arguments=["-B",script.path]
            var env=ProcessInfo.processInfo.environment;env["XIAO_DISPLAY_DATA_DIR"]=dataRoot.path;process.environment=env
            process.standardInput=input;process.standardOutput=output;process.standardError=FileHandle.nullDevice
            var result:[String:Any]=[:]
            do{
                let data=try JSONSerialization.data(withJSONObject:["serial":serial,"ssid":ssid,"password":key])
                try process.run();input.fileHandleForWriting.write(data);try input.fileHandleForWriting.close()
                let timeout=DispatchWorkItem{if process.isRunning{process.terminate()}}
                DispatchQueue.global().asyncAfter(deadline:.now()+100,execute:timeout)
                let reply=output.fileHandleForReading.readDataToEndOfFile();process.waitUntilExit();timeout.cancel()
                result=(try? JSONSerialization.jsonObject(with:reply)) as? [String:Any] ?? ["error":"Wi-Fi setup did not return a result."]
            }catch{result=["error":"Wi-Fi setup could not run: \(error.localizedDescription)"]}
            let snapshot=result
            DispatchQueue.main.async{
                guard let self=self,!self.quitting else{return}
                self.wifiSetupRunning=false
                defer{self.prepareMenuIfNeeded()}
                if let error=snapshot["error"] as? String{self.showError(error);return}
                if let host=snapshot["host"] as? String{self.connectionSettings["wifi_host"]=host}
                if snapshot["connected"] as? Bool == true{
                    self.connectionSettings["mode"]="wifi";self.saveConnection()
                    let done=NSAlert();done.messageText="Display connected to Wi-Fi"
                    done.informativeText="The helper is now using your local network. You can power the display from a USB charger. USB and Bluetooth remain available in Connection."
                    done.addButton(withTitle:"Great");done.runModal()
                }else{
                    self.saveConnection();self.showError(snapshot["message"] as? String ?? "Settings saved. The display is still trying to connect.")
                }
            }
        }
    }
}
