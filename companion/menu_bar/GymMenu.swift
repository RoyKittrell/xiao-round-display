import AppKit

final class GymSliderBox:NSView {
    let slider:NSSlider,label=NSTextField(labelWithString:"")
    let key:String,deviceId:String,epoch:String,title:String
    init(key:String,title:String,value:Int,minimum:Int,deviceId:String,epoch:String,target:AnyObject){
        self.key=key;self.title=title;self.deviceId=deviceId;self.epoch=epoch
        slider=NSSlider(value:Double(value),minValue:Double(minimum),maxValue:100,target:target,action:#selector(AppDelegate.gymSlider(_:)))
        super.init(frame:NSRect(x:0,y:0,width:285,height:58))
        label.frame=NSRect(x:15,y:32,width:260,height:20);label.font=NSFont.menuFont(ofSize:12)
        slider.frame=NSRect(x:15,y:7,width:250,height:24);slider.isContinuous=true;slider.setAccessibilityLabel(title)
        slider.identifier=NSUserInterfaceItemIdentifier(key);addSubview(label);addSubview(slider);update(value)
    }
    required init?(coder:NSCoder){fatalError()}
    func update(_ value:Int){slider.doubleValue=Double(value);label.show("\(title) · \(value)%")}
}

extension AppDelegate {
    var gymCurrent:[String:Any]{
        guard gymState["device_id"] as? String==selectedDeviceId,gymState["selection"] as? String==selectionEpoch else{return [:]}
        return gymState
    }
    func addGymMenus(){
        let transport=health["transport"] as? String ?? "usb"
        add(connected ? "Gym Xiao · \(transport=="wifi" ? "Wi-Fi":transport=="bluetooth" ? "Bluetooth":"USB")":"Gym Xiao · disconnected",enabled:false)
        let pulses=submenu("Pulses"),current=gymCurrent
        gymSliders=[]
        if connected,current["ready"] as? Bool == true,let glow=current["glow"] as? [String:Int],let brightness=current["brightness"] as? Int {
            for (key,title,minimum,value) in [("peak","Pulse brightness",1,glow["peak"] ?? 1),("centre","Centre glow",5,glow["centre"] ?? 5),("high_floor","High-cadence minimum",0,glow["high_floor"] ?? 0),("brightness","Screen brightness",0,brightness)] {
                let view=GymSliderBox(key:key,title:title,value:value,minimum:minimum,deviceId:selectedDeviceId,epoch:selectionEpoch,target:self)
                let item=NSMenuItem();item.view=view;pulses.addItem(item);gymSliders.append(view)
            }
            let feedback=NSTextField(labelWithString:"Device confirmed")
            feedback.frame=NSRect(x:15,y:0,width:265,height:22);feedback.font=NSFont.menuFont(ofSize:11);feedback.textColor = .secondaryLabelColor
            let row=NSMenuItem();row.view=feedback;pulses.addItem(row);gymFeedback=feedback
            add("Pulse appearance and backlight are separate",to:pulses,enabled:false)
        }else{add(connected ? "Reading settings from Gym Xiao…":"Reconnect to read device settings",to:pulses,enabled:false)}
        let demo=submenu("Demo Mode"),on=current["demo"] as? Bool
        for (index,label) in ["On · desk demonstration","Off · live pedal sensor"].enumerated(){
            let entry=add(label,to:demo,action:#selector(gymDemo(_:)),tag:index,checked:on==(index==0),enabled:connected && current["ready"] as? Bool == true)
            entry.representedObject=["target":selectedDeviceId,"selection":selectionEpoch]
        }
        let rideMenu=submenu("Ride Data")
        let ready=connected && current["ride"] is [String:Any] && !wifiSetupRunning
        if let ride=current["ride"] as? [String:Any] {
            let imperial=current["units"] as? String=="imperial"
            let distance=(ride["distance_m"] as? Double ?? 0)/(imperial ? 1609.344:1000)
            add(String(format:"Distance · %.3f %@",distance,imperial ? "mi":"km"),to:rideMenu,enabled:false)
            add(String(format:"Estimated work · %.2f kJ",ride["work_kj"] as? Double ?? 0),to:rideMenu,enabled:false)
        }
        for (tag,label) in ["Kilometres · km/h","Miles · mph"].enumerated(){add(label,to:rideMenu,action:#selector(gymUnits(_:)),tag:tag,checked:current["units"] as? String==(tag==0 ? "metric":"imperial"),enabled:ready)}
        add("Distance per Pedal Turn…",to:rideMenu,action:#selector(gymDistance(_:)),enabled:ready)
        add("Start New Ride…",to:rideMenu,action:#selector(gymNewRide(_:)),enabled:ready)
        add("Provisional distance · default 5 m/turn",to:rideMenu,enabled:false)
        add("Watch virtual wheel · set 2100 mm",to:rideMenu,enabled:false)
        let connections=submenu("Connections")
        let mode=connectionSettings["mode"] ?? "usb"
        let ble=hardware["bluetooth"] as? Bool == true
        let usbAvailable=(deviceCatalog[selectedDeviceId]?["usb_present"] as? Bool ?? false) || (connected && transport=="usb")
        let pairedBLE=ble && connectionSettings["bluetooth_id"] != nil && connectionSettings["bluetooth_usb_serial"]==connectionSettings["usb_serial"]
        add("USB",to:connections,action:#selector(changeConnection(_:)),tag:0,checked:mode=="usb",enabled: !wifiSetupRunning)
        if wifiFirmware {
            add("Wi-Fi",to:connections,action:#selector(changeConnection(_:)),tag:3,checked:mode=="wifi",enabled:connectionSettings["wifi_host"] != nil && !wifiSetupRunning)
            add(wifiSetupRunning ? "Setting up Wi-Fi…":"Set Up Wi-Fi via USB…",to:connections,action:#selector(setupWifi(_:)),enabled: !wifiSetupRunning && usbAvailable)
        }else{add("Wi-Fi · needs Gym firmware support",to:connections,enabled:false)}
        if hardware["watch_csc"] as? Bool == true {
            let watch=NSMenu();watch.autoenablesItems=false
            add("Bluetooth · Apple Watch",to:connections).submenu=watch
            add(current["watch_streaming"] as? Bool == true ? "Workout sensor streaming":"Ready for Apple Watch",to:watch,enabled:false)
            add("Connect Apple Watch…",to:watch,action:#selector(showWatchPairing(_:)))
            let ppr=current["pulses_per_revolution"] as? Int ?? 1
            add("Sensor calibration: \(ppr) pulse\(ppr==1 ? "":"s") per turn",to:watch,enabled:false)
            add("Demo data stays on the display",to:watch,enabled:false)
        }else if ble {
            add("Bluetooth",to:connections,action:#selector(changeConnection(_:)),tag:1,checked:mode=="bluetooth",enabled:pairedBLE && !wifiSetupRunning)
            add(bluetoothScanRunning ? "Searching for display…":"Find Bluetooth Display…",to:connections,action:#selector(findBluetooth(_:)),enabled: !bluetoothScanRunning && !wifiSetupRunning)
        }else{add("Bluetooth · needs Gym firmware support",to:connections,enabled:false)}
        add("Automatic · USB, then Wi-Fi",to:connections,action:#selector(changeConnection(_:)),tag:2,checked:mode=="auto",enabled: !wifiSetupRunning && wifiFirmware)
        add("Choose USB Device…",to:connections,action:#selector(findUSBDisplay(_:)),enabled: !wifiSetupRunning)
        menu.addItem(.separator());add("Quit Helper",action:#selector(quitHelper(_:)))
    }
    @objc func showWatchPairing(_ sender:NSMenuItem){
        NSApp.activate(ignoringOtherApps:true)
        let alert=NSAlert();alert.messageText="Connect Gym Xiao to Apple Watch"
        let suffix=String((connectionSettings["usb_serial"] ?? "").filter{$0.isLetter || $0.isNumber}.suffix(6)).uppercased()
        alert.informativeText="On your watch, open Settings → Bluetooth. Under Health Devices, choose Gym Xiao-\(suffix), then start an Indoor Cycle workout. The watch receives real pedal cadence, estimated power and virtual wheel data. In the sensor settings, set Wheel Size to 2100 mm if offered. Indoor speed/distance display depends on the Watch workout view. Desk Demo Mode does not send simulated workouts. The Mac uses USB first, then Wi-Fi."
        alert.addButton(withTitle:"OK");alert.runModal()
    }
    @objc func gymSlider(_ sender:NSSlider){
        guard connected,selectedRole=="gym",let view=gymSliders.first(where:{$0.slider===sender}),view.deviceId==selectedDeviceId,view.epoch==selectionEpoch else{return}
        view.update(Int(sender.doubleValue.rounded()))
        if view.key=="brightness"{gymDesired["brightness"]=Int(sender.doubleValue.rounded())}
        else{
            var glow=(gymDesired["glow"] as? [String:Int]) ?? [:]
            glow[view.key]=Int(sender.doubleValue.rounded());gymDesired["glow"]=glow
        }
        scheduleGymAction()
    }
    @objc func gymDemo(_ sender:NSMenuItem){
        guard connected,selectedRole=="gym",let context=sender.representedObject as? [String:String],context["target"]==selectedDeviceId,context["selection"]==selectionEpoch else{return}
        gymDesired["demo"]=sender.tag==0;scheduleGymAction()
    }
    func scheduleGymAction(){
        gymSendWork?.cancel();let id=selectedDeviceId,epoch=selectionEpoch
        let work=DispatchWorkItem{[weak self] in
            guard let self=self,self.selectedRole=="gym",self.selectedDeviceId==id,self.selectionEpoch==epoch else{return}
            let token=UUID().uuidString
            var values=self.gymPendingValues
            for (key,value) in self.gymDesired {
                if key=="glow",let changed=value as? [String:Int]{values[key]=(values[key] as? [String:Int] ?? [:]).merging(changed){_,new in new}}
                else{values[key]=value}
            }
            let action:[String:Any]=["target":id,"selection":epoch,"token":token,"values":values]
            do{let url=dataRoot.appendingPathComponent("gym-actions.json");var value=object(url);value[id]=action;try self.writePrivate(value,to:url);self.gymPendingToken=token;self.gymPendingValues=values;self.gymDesired=[:];self.gymPendingAt=Date();self.gymFeedback?.show("Applying…")}
            catch{self.showError("Could not queue Gym settings: \(error.localizedDescription)")}
        }
        gymSendWork=work;DispatchQueue.main.asyncAfter(deadline:.now()+0.22,execute:work)
    }
    func refreshRideItems(){
        guard let items=menu.items.first(where:{$0.title=="Ride Data"})?.submenu?.items,items.count>=2,let ride=gymCurrent["ride"] as? [String:Any] else{return}
        let imperial=gymCurrent["units"] as? String=="imperial"
        items[0].title=String(format:"Distance · %.3f %@",(ride["distance_m"] as? Double ?? 0)/(imperial ? 1609.344:1000),imperial ? "mi":"km")
        items[1].title=String(format:"Estimated work · %.2f kJ",ride["work_kj"] as? Double ?? 0)
    }
    @objc func gymUnits(_ sender:NSMenuItem){
        guard selectedRole=="gym",connected else{return}
        gymDesired["units"]=sender.tag==0 ? "metric":"imperial";scheduleGymAction()
    }
    @objc func gymDistance(_ sender:NSMenuItem){
        guard selectedRole=="gym",connected else{return}
        let id=selectedDeviceId,epoch=selectionEpoch
        let alert=NSAlert();alert.messageText="Distance per Pedal Turn"
        alert.informativeText="The provisional value is 5 metres per full pedal revolution. Later, divide the original console's distance increase by the number of turns. Changes affect future distance only."
        let field=NSTextField(frame:NSRect(x:0,y:0,width:320,height:26));field.stringValue=String(format:"%.3f",Double(gymCurrent["distance_mm"] as? Int ?? 5000)/1000)
        alert.accessoryView=field;alert.addButton(withTitle:"Save Metres per Turn");alert.addButton(withTitle:"Cancel")
        guard alert.runModal() == .alertFirstButtonReturn,selectedDeviceId==id,selectionEpoch==epoch else{return}
        guard let metres=Double(field.stringValue),metres.isFinite,metres>=0.1,metres<=100 else{showError("Enter 0.1–100 metres per full turn.");return}
        gymDesired["distance_mm"]=Int((metres*1000).rounded());scheduleGymAction()
    }
    @objc func gymNewRide(_ sender:NSMenuItem){
        guard selectedRole=="gym",connected else{return}
        let id=selectedDeviceId,epoch=selectionEpoch
        let alert=NSAlert();alert.messageText="Start a new ride?";alert.informativeText="Reset the real ride totals and peaks. Raw pedal logs and Watch sensor counters are retained. Demo data is separate."
        alert.addButton(withTitle:"Start New Ride");alert.addButton(withTitle:"Cancel")
        guard alert.runModal() == .alertFirstButtonReturn,selectedDeviceId==id,selectionEpoch==epoch else{return}
        gymDesired["new_ride"]=true;scheduleGymAction()
    }
    func updateGymControls(){
        guard selectedRole=="gym" else{return}
        if let token=gymPendingToken {
            if gymCurrent["last_token"] as? String==token{gymPendingToken=nil;gymPendingValues=[:];gymFeedback?.show("Device confirmed")}
            else if Date().timeIntervalSince(gymPendingAt)<4{gymFeedback?.show("Applying…");return}
            else{gymPendingToken=nil;gymPendingValues=[:]}
        }
        if gymSendWork?.isCancelled==false,!gymDesired.isEmpty{return}
        let g=gymCurrent["glow"] as? [String:Int] ?? [:]
        for view in gymSliders {
            guard view.deviceId==selectedDeviceId,view.epoch==selectionEpoch else{continue}
            if let n=view.key=="brightness" ? gymCurrent["brightness"] as? Int:g[view.key]{view.update(n)}
        }
    }
}
