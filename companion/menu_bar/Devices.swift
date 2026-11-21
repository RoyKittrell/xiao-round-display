import AppKit

extension AppDelegate {
    var selectedDeviceId:String {
        if let id=connectionSettings["device_id"],!id.isEmpty{return id}
        if let serial=connectionSettings["usb_serial"] ?? connectionSettings["bluetooth_usb_serial"],!serial.isEmpty{return "usb:"+serial.filter{$0.isLetter || $0.isNumber}.lowercased()}
        if let host=connectionSettings["wifi_host"]{return "wifi:"+host}
        if let id=connectionSettings["bluetooth_id"]{return "ble:"+id.lowercased()}
        return ""
    }
    var selectionEpoch:String{connectionSettings["selection_epoch"] ?? "legacy"}
    var selectedRole:String{
        if let role=hardware["role"] as? String{return role}
        if deviceCatalog[selectedDeviceId]?["usb_present"] as? Bool == false{return deviceCatalog[selectedDeviceId]?["role"] as? String ?? "unknown"}
        return "unknown"
    }
    var deviceCatalog:[String:[String:Any]]{catalogState["devices"] as? [String:[String:Any]] ?? [:]}
    var deviceNames:[String:String]{namesState["names"] as? [String:String] ?? [:]}
    func nameForDevice(_ id:String)->String {
        if let name=deviceNames[id],!name.isEmpty{return name}
        let role=deviceCatalog[id]?["role"] as? String ?? "unknown"
        let base=role=="gym" ? "Gym Xiao":role=="desktop" ? "Desktop Xiao":"XIAO"
        return base+" · "+String(id.suffix(6)).uppercased()
    }
    var catalogSignature:String {
        deviceCatalog.keys.sorted().map{id in
            id+":"+nameForDevice(id)+":"+String(deviceCatalog[id]?["usb_present"] as? Bool ?? false)+":"+String(deviceCatalog[id]?["online"] as? Bool ?? false)+":"+(deviceCatalog[id]?["role"] as? String ?? "unknown")
        }.joined(separator:"|")
    }
    func loadDeviceState(){catalogState=object(dataRoot.appendingPathComponent("device-catalog.json"));namesState=object(dataRoot.appendingPathComponent("device-names.json"))}
    func writePrivate(_ value:[String:Any],to url:URL)throws{
        try FileManager.default.createDirectory(at:url.deletingLastPathComponent(),withIntermediateDirectories:true)
        try JSONSerialization.data(withJSONObject:value,options:[.prettyPrinted,.sortedKeys]).write(to:url,options:.atomic)
        try FileManager.default.setAttributes([.posixPermissions:0o600],ofItemAtPath:url.path)
    }
    func rememberConnection()throws {
        let id=selectedDeviceId;guard !id.isEmpty else{return}
        let url=dataRoot.appendingPathComponent("device-connections.json")
        var saved=object(url);saved[id]=connectionSettings;try writePrivate(saved,to:url)
    }
    func addDevicePicker(){
        let picker=submenu("Device: "+(selectedDeviceId.isEmpty ? "Choose XIAO…":nameForDevice(selectedDeviceId)))
        for id in deviceCatalog.keys.sorted(by:{nameForDevice($0).localizedStandardCompare(nameForDevice($1)) == .orderedAscending}) {
            let present=deviceCatalog[id]?["usb_present"] as? Bool ?? false
            let online=(deviceCatalog[id]?["online"] as? Bool ?? false) || (id==selectedDeviceId && connected)
            let entry=add(nameForDevice(id)+(present || online ? "":" · disconnected"),to:picker,action:#selector(chooseDevice(_:)),checked:id==selectedDeviceId,enabled: !wifiSetupRunning)
            entry.representedObject=id;entry.toolTip=deviceCatalog[id]?["model"] as? String
        }
        if deviceCatalog.isEmpty{add("Discovering connected XIAOs…",to:picker,enabled:false)}
        picker.addItem(.separator())
        add("Device Settings…",to:picker,action:#selector(deviceSettings(_:)),enabled: !selectedDeviceId.isEmpty && !wifiSetupRunning)
        add("Choose USB Device…",to:picker,action:#selector(findUSBDisplay(_:)),enabled: !wifiSetupRunning)
    }
    @objc func chooseDevice(_ sender:NSMenuItem){
        guard !wifiSetupRunning,let id=sender.representedObject as? String,let record=deviceCatalog[id] else{return}
        selectDevice(id,record:record)
    }
    func selectDevice(_ id:String,record:[String:Any],forceUSB:Bool=false){
        guard !wifiSetupRunning else{return}
        do{
            try rememberConnection()
            let saved=object(dataRoot.appendingPathComponent("device-connections.json"))
            var chosen=saved[id] as? [String:String] ?? ["mode":"usb"]
            for key in ["usb_serial","usb_device","bluetooth_id","bluetooth_usb_serial","wifi_host"] {
                if chosen[key]==nil,let v=record[key] as? String{chosen[key]=v}
            }
            if forceUSB{chosen["mode"]="usb"}
            if record["role"] as? String=="gym"{chosen["bluetooth_for_workout"]="1"}
            chosen["device_id"]=id;chosen["selection_epoch"]=UUID().uuidString
            connectionSettings=chosen;health=[:];device=[:];gymState=[:];gymPendingToken=nil;gymSliders=[];gymSendWork?.cancel();gymSendWork=nil;gymDesired=[:];gymPendingValues=[:]
            prefs=Preferences();pendingBrightnessRevision=nil;pendingBrightnessValue=nil;pendingBrightnessProfile=nil
            saveConnection();load();prepareMenuIfNeeded()
        }catch{showError("Could not save device selection: \(error.localizedDescription)")}
    }
    var deviceProfileCatalog:[[String:Any]] {
        if let resource=Bundle.main.resourceURL {
            let data=object(resource.appendingPathComponent("companion/device_profiles/catalog.json"))
            if let profiles=data["profiles"] as? [[String:Any]]{return profiles}
        }
        return [["id":"desktop","name":"Desktop"],["id":"gym","name":"Gym Display"]]
    }
    func deviceSettingsView()->(NSView,NSTextField,NSPopUpButton){
        let view=NSView(frame:NSRect(x:0,y:0,width:370,height:133))
        let nameLabel=NSTextField(labelWithString:"Device Name")
        nameLabel.frame=NSRect(x:0,y:111,width:370,height:18);view.addSubview(nameLabel)
        let name=NSTextField(frame:NSRect(x:0,y:80,width:370,height:25))
        name.stringValue=nameForDevice(selectedDeviceId);name.setAccessibilityLabel("Device Name");view.addSubview(name)
        let profileLabel=NSTextField(labelWithString:"Profile")
        profileLabel.frame=NSRect(x:0,y:49,width:370,height:18);view.addSubview(profileLabel)
        let profile=NSPopUpButton(frame:NSRect(x:0,y:16,width:370,height:28),pullsDown:false)
        for item in deviceProfileCatalog {
            guard let id=item["id"] as? String,let label=item["name"] as? String else{continue}
            profile.addItem(withTitle:label);profile.lastItem?.representedObject=id
            if id==selectedRole{profile.selectItem(at:profile.numberOfItems-1)}
        }
        if !["desktop","gym"].contains(selectedRole){profile.select(nil)}
        profile.setAccessibilityLabel("Device Profile");view.addSubview(profile)
        if selectedRole=="gym" {
            view.frame.size.height=205
            for child in view.subviews{child.frame.origin.y+=72}
            let titleLabel=NSTextField(labelWithString:"Gym Display Title · 1–24 plain text characters")
            titleLabel.frame=NSRect(x:0,y:49,width:370,height:18);view.addSubview(titleLabel)
            let title=NSTextField(frame:NSRect(x:0,y:16,width:370,height:25));title.tag=501
            title.stringValue=gymCurrent["title"] as? String ?? "BIKE LAB"
            title.setAccessibilityLabel("Gym Display Title");view.addSubview(title)
            title.isEnabled=gymCurrent["title"] is String
            if !title.isEnabled{title.placeholderString="Requires the Gym firmware update"}
        }
        return (view,name,profile)
    }
    @objc func deviceSettings(_ sender:NSMenuItem){
        let id=selectedDeviceId;guard !id.isEmpty,!wifiSetupRunning else{return}
        NSApp.activate(ignoringOtherApps:true)
        let alert=NSAlert();alert.messageText="Device Settings"
        alert.informativeText="Names are saved on this Mac. Changing the profile installs its firmware over USB. Each profile retains its settings; other XIAOs keep running. Disconnect the bike sensor before using Desktop, which can use D2 for its SD card."
        let (view,name,profile)=deviceSettingsView()
        alert.accessoryView=view;alert.addButton(withTitle:"Save / Apply Profile");alert.addButton(withTitle:"Cancel");alert.window.initialFirstResponder=name
        guard alert.runModal() == .alertFirstButtonReturn else{return}
        guard let wanted=profile.selectedItem?.representedObject as? String else{showError("Select a device profile.");return}
        do{
            try saveDeviceName(name.stringValue,id:id)
            if wanted != selectedRole{try beginDeviceProfileInstall(wanted,id:id)}
            else if wanted=="gym",let title=view.viewWithTag(501) as? NSTextField,title.isEnabled,title.stringValue != gymCurrent["title"] as? String {
                let value=title.stringValue
                guard !value.trimmingCharacters(in:.whitespaces).isEmpty,value.count<=24,value.unicodeScalars.allSatisfy({$0.value>=32 && $0.value<=126}) else{throw NSError(domain:"XIAOTitle",code:1,userInfo:[NSLocalizedDescriptionKey:"Use 1–24 plain text characters for the display title."])}
                gymDesired["title"]=value;scheduleGymAction()
            }
            prepareMenuIfNeeded()
        }catch{showError(error.localizedDescription)}
    }
    func beginDeviceProfileInstall(_ profile:String,id:String)throws{
        guard ["desktop","gym"].contains(profile),id==selectedDeviceId else{throw NSError(domain:"XIAOProfile",code:1,userInfo:[NSLocalizedDescriptionKey:"Select a valid device profile."])}
        let record=deviceCatalog[id] ?? [:]
        let family=hardware["family"] as? String ?? record["family"] as? String ?? "unknown"
        guard family=="esp32s3" else{throw NSError(domain:"XIAOProfile",code:2,userInfo:[NSLocalizedDescriptionKey:"Profile switching currently supports positively identified ESP32-S3 boards. Existing Desktop builds on other controllers remain supported."])}
        guard record["usb_present"] as? Bool == true else{throw NSError(domain:"XIAOProfile",code:3,userInfo:[NSLocalizedDescriptionKey:"Connect this XIAO with a USB data cable to apply its profile. Its saved name has been updated."])}
        let requestedEpoch=selectionEpoch
        let python=Bundle.main.object(forInfoDictionaryKey:"XIAOPythonPath") as? String ?? ""
        let script=Bundle.main.resourceURL!.appendingPathComponent("companion/device_profiles.py")
        let panel=NSPanel(contentRect:NSRect(x:0,y:0,width:400,height:110),styleMask:[.titled],backing:.buffered,defer:false)
        panel.title="Applying Device Profile"
        let label=NSTextField(wrappingLabelWithString:"Installing \(profile=="gym" ? "Gym Display":"Desktop")…\nKeep this XIAO connected by USB. Other displays continue updating.")
        label.frame=NSRect(x:20,y:25,width:360,height:65);panel.contentView?.addSubview(label);panel.center();panel.makeKeyAndOrderFront(nil)
        wifiSetupRunning=true;prepareMenuIfNeeded()
        DispatchQueue.global(qos:.userInitiated).async{[weak self] in
            let process=Process(),output=Pipe();process.executableURL=URL(fileURLWithPath:python)
            process.arguments=["-B",script.path,"--device",id,"--profile",profile]
            var env=ProcessInfo.processInfo.environment;env["XIAO_DISPLAY_DATA_DIR"]=dataRoot.path;process.environment=env
            process.standardOutput=output;process.standardError=FileHandle.nullDevice
            var result:[String:Any]=[:]
            do{
                try process.run();let data=output.fileHandleForReading.readDataToEndOfFile();process.waitUntilExit()
                result=(try? JSONSerialization.jsonObject(with:data)) as? [String:Any] ?? ["error":"Profile installation did not return a result."]
            }catch{result=["error":error.localizedDescription]}
            let snapshot=result
            DispatchQueue.main.async{
                panel.close();guard let self=self else{return};self.wifiSetupRunning=false
                defer{self.prepareMenuIfNeeded()}
                if let error=snapshot["error"] as? String{self.showError(error);return}
                guard snapshot["verified"] as? Bool == true else{self.showError("The display has not confirmed its new profile.");return}
                guard self.selectedDeviceId==id,self.selectionEpoch==requestedEpoch else{return}
                if profile=="gym"{self.connectionSettings["bluetooth_for_workout"]="1"}
                else{self.connectionSettings.removeValue(forKey:"bluetooth_for_workout")}
                self.connectionSettings["selection_epoch"]=UUID().uuidString
                self.health=[:];self.device=[:];self.gymState=[:];self.saveConnection()
                let done=NSAlert();done.messageText="\(profile=="gym" ? "Gym Display":"Desktop") profile ready"
                done.informativeText="The XIAO confirmed its new firmware. Its menus will refresh as the helper reconnects. Saved profile settings and other devices are preserved."
                done.addButton(withTitle:"Done");done.runModal()
            }
        }
    }
    func saveDeviceName(_ name:String,id:String)throws{
        let name=name.trimmingCharacters(in:.whitespacesAndNewlines)
        guard !id.isEmpty,!name.isEmpty,name.count<=48,!name.unicodeScalars.contains(where:CharacterSet.controlCharacters.contains) else{throw NSError(domain:"XIAODeviceName",code:1)}
        var names=deviceNames;names[id]=name;let value:[String:Any]=["schema_version":1,"names":names]
        try writePrivate(value,to:dataRoot.appendingPathComponent("device-names.json"));namesState=value
    }

}
