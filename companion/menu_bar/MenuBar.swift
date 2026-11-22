import AppKit
import CoreWLAN
import Foundation
import Darwin
import Dispatch

struct Scene: Codable { let id: Int; let name: String }
let helperName="XIAO Display Helper"
let dataRoot=ProcessInfo.processInfo.environment["XIAO_DISPLAY_DATA_DIR"].map{URL(fileURLWithPath:$0)} ?? FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/\(helperName)")
let displayTimezone:TimeZone={
    let settings=object(dataRoot.appendingPathComponent("settings.json"))
    let minutes=settings["utc_offset_minutes"] as? Int ?? 0
    return TimeZone(secondsFromGMT:((-720...840).contains(minutes) ? minutes:0)*60)!
}()
let decoder:JSONDecoder={let d=JSONDecoder();d.keyDecodingStrategy = .convertFromSnakeCase;return d}()
let encoder:JSONEncoder={let e=JSONEncoder();e.keyEncodingStrategy = .convertToSnakeCase;e.outputFormatting=[.prettyPrinted,.sortedKeys];return e}()
func object(_ url:URL)->[String:Any] { guard let data=try? Data(contentsOf:url),let value=try? JSONSerialization.jsonObject(with:data) as? [String:Any] else{return [:]};return value }
extension NSTextField {
    func show(_ value:String){if stringValue != value{stringValue=value}}
}

final class SliderBox:NSView {
    let slider:NSSlider
    let label=NSTextField(labelWithString:"")
    let feedback=NSTextField(labelWithString:"")
    init(value:Int,target:AnyObject,action:Selector,enabled:Bool){
        slider=NSSlider(value:Double(value),minValue:0,maxValue:100,target:target,action:action)
        super.init(frame:NSRect(x:0,y:0,width:250,height:78))
        label.frame=NSRect(x:16,y:53,width:220,height:18);label.font=NSFont.menuFont(ofSize:12);label.stringValue="Brightness · \(value)%"
        slider.frame=NSRect(x:16,y:28,width:218,height:24);slider.isContinuous=true;slider.isEnabled=enabled
        feedback.frame=NSRect(x:16,y:6,width:220,height:17);feedback.font=NSFont.menuFont(ofSize:11);feedback.textColor = .secondaryLabelColor
        slider.setAccessibilityLabel("Display brightness");addSubview(label);addSubview(slider);addSubview(feedback)
    }
    required init?(coder:NSCoder){fatalError()}
}

// Native menu-item actions dismiss their menu. Controls embedded in a view
// handle clicks themselves, allowing several checkbox changes in one visit.
final class CarouselChecklist:NSView {
    var buttons:[NSButton]=[]
    let allButton=NSButton(),note=NSTextField(labelWithString:"")
    let allMask:Int
    let bitBase:Int
    private var lastMask:Int?
    let onToggle:(Int)->Void, onDone:()->Void
    override var isFlipped:Bool {true}
    init(scenes:[Scene],mask:Int,bitBase:Int=2,onToggle:@escaping(Int)->Void,onDone:@escaping()->Void){
        self.onToggle=onToggle;self.onDone=onDone
        self.bitBase=bitBase;allMask=scenes.reduce(0){$0 | (1 << ($1.id-bitBase))}
        super.init(frame:NSRect(x:0,y:0,width:270,height:CGFloat(scenes.count*26+92)))
        allButton.setButtonType(.switch);allButton.title="All";allButton.allowsMixedState=true
        allButton.target=self;allButton.action=#selector(toggle(_:));allButton.tag = -1
        allButton.font=NSFont.menuFont(ofSize:13);allButton.frame=NSRect(x:14,y:8,width:242,height:24);addSubview(allButton)
        let separator=NSBox();separator.boxType = .separator;separator.frame=NSRect(x:14,y:38,width:242,height:1);addSubview(separator)
        for (index,scene) in scenes.enumerated(){
            let button=NSButton(checkboxWithTitle:scene.name,target:self,action:#selector(toggle(_:)))
            button.tag=scene.id;button.font=NSFont.menuFont(ofSize:13)
            button.frame=NSRect(x:14,y:CGFloat(48+index*26),width:242,height:24)
            buttons.append(button);addSubview(button)
        }
        note.font=NSFont.menuFont(ofSize:11);note.textColor = .secondaryLabelColor
        note.frame=NSRect(x:14,y:CGFloat(scenes.count*26+60),width:165,height:17);addSubview(note)
        let done=NSButton(title:"Done",target:self,action:#selector(finish(_:)));done.bezelStyle = .rounded
        done.frame=NSRect(x:187,y:CGFloat(scenes.count*26+52),width:70,height:28);addSubview(done)
        update(mask:mask)
    }
    required init?(coder:NSCoder){fatalError()}
    func update(mask:Int){
        guard mask != lastMask else{return};lastMask=mask
        let allState:NSControl.StateValue=mask==allMask ? .on:mask==0 ? .off:.mixed
        if allButton.state != allState{allButton.state=allState}
        note.show(mask==0 ? "None selected · paused":"Changes save immediately")
        for button in buttons {
            let selected=mask & (1 << (button.tag-bitBase)) != 0
            let state:NSControl.StateValue=selected ? .on:.off
            if button.state != state{button.state=state}
        }
    }
    @objc func toggle(_ sender:NSButton){onToggle(sender.tag)}
    @objc func finish(_ sender:NSButton){onDone()}
}

final class AppDelegate:NSObject,NSApplicationDelegate,NSMenuDelegate {
    var deviceDataRoot:URL {
        guard !selectedDeviceId.isEmpty else{return dataRoot}
        let encoded=selectedDeviceId.utf8.map{String(format:"%02x",$0)}.joined()
        return dataRoot.appendingPathComponent("devices").appendingPathComponent(encoded)
    }
    var prefsURL:URL{deviceDataRoot.appendingPathComponent("display-preferences.json")}
    var statusURL:URL{deviceDataRoot.appendingPathComponent("control-status.json")}
    var healthURL:URL{deviceDataRoot.appendingPathComponent("helper-status.json")}
    var item:NSStatusItem!,menu=NSMenu(),child:Process?,timer:Timer?
    var prefs=Preferences(),scenes:[Scene]=[],device:[String:Any]=[:],health:[String:Any]=[:]
    var quitting=false,sliderBox:SliderBox?
    var signalSources:[DispatchSourceSignal]=[]
    var pendingBrightnessRevision:UInt32?,pendingBrightnessValue:Int?,pendingBrightnessProfile:String?
    var profileStore:ProfileStore?
    var carouselChecklist:CarouselChecklist?
    var clockChecklist:CarouselChecklist?,clockFaces:[Scene]=[]
    var weatherPlace:WeatherPlace?,weatherLocationRequest:WeatherLocationRequest?
    var weatherLocationBusy=false
    var weatherLocationPanel:NSPanel?
    let weatherLocationURL=dataRoot.appendingPathComponent("weather-location.json")
    let stateQueue=DispatchQueue(label:"org.xiaodisplay.menu-state",qos:.utility)
    var stateReadPending=false,menuIsOpen=false,lastMenuSignature="",profileMenuGeneration=0
    var sourceEntries:[String:NSMenuItem]=[:]
    var menuActivity:NSObjectProtocol?
    var connectionSettings:[String:String]=[:],bluetoothScanRunning=false,wifiSetupRunning=false
    let connectionURL=dataRoot.appendingPathComponent("display-connection.json")
    var catalogState:[String:Any]=[:],namesState:[String:Any]=[:],gymState:[String:Any]=[:]
    var gymSliders:[GymSliderBox]=[],gymDesired:[String:Any]=[:],gymPendingValues:[String:Any]=[:]
    var gymSendWork:DispatchWorkItem?,gymPendingToken:String?,gymPendingAt=Date.distantPast
    var gymFeedback:NSTextField?
    var connected:Bool {
        guard health["selected_id"] as? String==selectedDeviceId,health["selection"] as? String==selectionEpoch else{return false}
        if deviceCatalog[selectedDeviceId]?["online"] as? Bool == false{return false}
        let timestamp=health["updated_at"] as? Double ?? 0
        return (health["display_connected"] as? Bool ?? health["usb_connected"] as? Bool ?? false) && Date().timeIntervalSince1970-timestamp<20
    }
    var hardware:[String:Any]{guard health["selected_id"] as? String==selectedDeviceId,health["selection"] as? String==selectionEpoch else{return [:]};return health["hardware"] as? [String:Any] ?? [:]}
    var wifiHardware:Bool{hardware["wifi_hardware"] as? Bool ?? false}
    var wifiFirmware:Bool{hardware["wifi_firmware"] as? Bool ?? false}
    var currentScene:Int { device["scene"] as? Int ?? prefs.selectedScene }
    var actualNight:Bool {
        if prefs.appearance==1{return false};if prefs.appearance==2{return true}
        var c=Calendar(identifier:.gregorian);c.timeZone=displayTimezone
        let parts=c.dateComponents([.hour,.minute],from:Date());let minute=(parts.hour ?? 0)*60+(parts.minute ?? 0)
        return minute>=1020 || minute<450
    }
    var actualBrightness:Int {
        if connected,device["error"]==nil,let v=device[actualNight ? "night_brightness":"day_brightness"] as? Int{return v}
        return 60 // Show the firmware startup default until a device reading arrives.
    }
    func applicationDidFinishLaunching(_ notification:Notification){
        let catalog=Bundle.main.resourceURL!.appendingPathComponent("companion/scenes.json")
        scenes=(try? decoder.decode([Scene].self,from:Data(contentsOf:catalog))) ?? []
        let faces=Bundle.main.resourceURL!.appendingPathComponent("companion/clock-faces.json")
        clockFaces=(try? decoder.decode([Scene].self,from:Data(contentsOf:faces))) ?? []
        weatherPlace=WeatherPlace.load(weatherLocationURL)
        if CommandLine.arguments.contains("--menu-dump"){
            let tree:[String:Any]=["name":helperName,"scenes":scenes.map{$0.name},"scene_options":["Clocks":["Show Clocks","Select/deselect all"]+clockFaces.map{$0.name},"Weather":["Show Weather","Select Location…"]],"profiles":["Saved profiles","Save Current Settings as Profile…","Delete Profile"],"playback":["Carousel","Stay on selected scene"],"intervals":[1,5,10,15,30,60],"brightness":"0–100% slider; startup60%","power":["Automatic schedule","Always on","Screen off"]]
            if let d=try? JSONSerialization.data(withJSONObject:tree,options:.prettyPrinted){print(String(data:d,encoding:.utf8)!)}
            NSApp.terminate(nil);return
        }
        installStopHandlers()
        NSApp.setActivationPolicy(.accessory)
        do{profileStore=try ProfileStore(url:dataRoot.appendingPathComponent("display-profiles.json"))}
        catch{showError("Could not load profiles. Your saved file has been left untouched. \(error.localizedDescription)")}
        connectionSettings=object(connectionURL) as? [String:String] ?? ["mode":"usb"];loadDeviceState()
        load();item=NSStatusBar.system.statusItem(withLength:NSStatusItem.squareLength)
        if let button=item.button {
            button.image=NSImage(systemSymbolName:"circle.dotted.circle",accessibilityDescription:"XIAO display controls")
            button.image?.isTemplate=true;button.toolTip="XIAO Round Display"
        }
        menu.autoenablesItems=false;menu.delegate=self;item.menu=menu
        startWorker()
        timer=Timer(timeInterval:0.5,repeats:true){[weak self] _ in self?.pollState()}
        RunLoop.main.add(timer!,forMode:.common)
        rebuild()
    }
    func startWorker(){
        let python=Bundle.main.object(forInfoDictionaryKey:"XIAOPythonPath") as? String ?? ""
        let script=Bundle.main.resourceURL!.appendingPathComponent("companion/display_bridge.py")
        let process=Process();process.qualityOfService = .utility;process.executableURL=URL(fileURLWithPath:python);process.arguments=["-B","-u",script.path]
        var env=ProcessInfo.processInfo.environment;env["XIAO_DISPLAY_DATA_DIR"]=dataRoot.path;env["PYTHONDONTWRITEBYTECODE"]="1";env["XIAO_MENU_OWNER_PID"]=String(getpid());process.environment=env
        process.currentDirectoryURL=dataRoot;process.standardOutput=FileHandle.standardOutput;process.standardError=FileHandle.standardError
        process.terminationHandler={[weak self] _ in DispatchQueue.main.async{if self?.quitting==false{NSApp.terminate(nil)}}}
        do{try process.run();child=process}catch{showError("Could not start the data helper: \(error.localizedDescription)")}
    }
    func installStopHandlers(){
        for number in [SIGTERM,SIGINT] {
            signal(number,SIG_IGN)
            let source=DispatchSource.makeSignalSource(signal:number,queue:.main)
            source.setEventHandler{[weak self] in self?.shutdown(unload:false)}
            source.resume();signalSources.append(source)
        }
    }
    func signalWorker(_ number:Int32){
        guard let process=child,process.isRunning else{return}
        let pid=process.processIdentifier
        if getpgid(pid)==pid && pid != getpgrp(){kill(-pid,number)}else{kill(pid,number)}
    }
    func shutdown(unload:Bool){
        if quitting{return};quitting=true;endMenuActivity();timer?.invalidate();signalWorker(SIGTERM)
        let worker=child
        DispatchQueue.global().async{[weak self] in
            let deadline=Date().addingTimeInterval(2)
            while worker?.isRunning==true && Date()<deadline{Thread.sleep(forTimeInterval:0.05)}
            if worker?.isRunning==true{self?.signalWorker(SIGKILL)}
            if worker != nil{worker?.waitUntilExit()}
            DispatchQueue.main.async{
                if unload {
                    let p=Process();p.executableURL=URL(fileURLWithPath:"/bin/launchctl");p.arguments=["bootout","gui/\(getuid())/org.xiaodisplay.helper"];try? p.run()
                    DispatchQueue.main.asyncAfter(deadline:.now()+0.3){NSApp.terminate(nil)}
                }else{NSApp.terminate(nil)}
            }
        }
    }
    func updateBrightnessFeedback(){
        guard let box=sliderBox else{return}
        if !connected{box.feedback.show("Display disconnected");return}
        if device["error"] != nil{box.feedback.show("Waiting for display confirmation");return}
        let off=(device["sleeping"] as? Int ?? 0)==1
        if let revision=pendingBrightnessRevision,let target=pendingBrightnessValue {
            if (device["revision"] as? UInt32 ?? 0)>=revision {
                let actual=device[pendingBrightnessProfile ?? "day_brightness"] as? Int ?? 0
                box.feedback.show(actual==target ? (off ? "Screen off · level \(actual)%":"Display confirmed \(actual)%") : "Display: \(actual)% · adjust slider again")
                pendingBrightnessRevision=nil;pendingBrightnessValue=nil;pendingBrightnessProfile=nil
            }else{box.feedback.show("Applying \(target)%…")}
        }else{
            let actual=actualBrightness
            if box.slider.doubleValue != Double(actual){box.slider.doubleValue=Double(actual)}
            box.label.show("Brightness · \(actual)% · \(actualNight ? "night":"day")")
            box.feedback.show(off ? "Screen off · level \(actual)%":"Display: \(actual)%")
        }
    }
    func pollState(){
        guard !stateReadPending,!quitting else{return};stateReadPending=true
        let revisionWhenRequested=prefs.revision,deviceWhenRequested=selectedDeviceId,epochWhenRequested=selectionEpoch
        let prefsPath=prefsURL,statusPath=statusURL,healthPath=healthURL,gymPath=deviceDataRoot.appendingPathComponent("gym-status.json")
        stateQueue.async{[weak self] in
            let reader=JSONDecoder();reader.keyDecodingStrategy = .convertFromSnakeCase
            let saved=(try? Data(contentsOf:prefsPath)).flatMap{try? reader.decode(Preferences.self,from:$0)}
            let deviceState=object(statusPath),workerState=object(healthPath)
            let catalog=object(dataRoot.appendingPathComponent("device-catalog.json")),names=object(dataRoot.appendingPathComponent("device-names.json")),gym=object(gymPath)
            DispatchQueue.main.async{
                guard let self=self else{return};self.stateReadPending=false
                guard self.selectedDeviceId==deviceWhenRequested,self.selectionEpoch==epochWhenRequested else{return}
                // A slow read must never overwrite a more recent checkbox,
                // profile or slider action performed while it was in flight.
                if self.prefs.revision==revisionWhenRequested,let saved=saved{self.prefs=saved}
                self.device=deviceState;self.health=workerState;self.catalogState=catalog;self.namesState=names;self.gymState=gym;self.updateGymControls()
                self.carouselChecklist?.update(mask:self.prefs.enabledMask);self.clockChecklist?.update(mask:self.prefs.includedClocks);self.updateTip()
                // Keep navigation stable: source-age labels are snapshots on opening.
                if self.menuIsOpen{self.updateBrightnessFeedback()}
                self.prepareMenuIfNeeded()
            }
        }
    }
    func load(){
        if let data=try? Data(contentsOf:prefsURL),let value=try? decoder.decode(Preferences.self,from:data){prefs=value}
        else if selectedRole=="desktop",let data=try? Data(contentsOf:dataRoot.appendingPathComponent("display-preferences.json")),let value=try? decoder.decode(Preferences.self,from:data){prefs=value}
        device=object(statusURL);health=object(healthURL)
        carouselChecklist?.update(mask:prefs.enabledMask)
        clockChecklist?.update(mask:prefs.includedClocks)
    }
    @discardableResult func save()->Bool {
        prefs.revision = prefs.revision == UInt32.max ? 1:prefs.revision+1
        do{try FileManager.default.createDirectory(at:deviceDataRoot,withIntermediateDirectories:true);try encoder.encode(prefs).write(to:prefsURL,options:.atomic);return true}catch{showError("Could not save display settings: \(error.localizedDescription)");return false}
    }
    func updateTip(){
        let name=scenes.first{$0.id==currentScene}?.name ?? "Display"
        let tip=connected ? "XIAO Display · \(name)":"XIAO Display · USB disconnected"
        if item?.button?.toolTip != tip{item?.button?.toolTip=tip}
    }
    var menuSignature:String {
        [String(prefs.revision),String(currentScene),String(connected),String(actualNight),
         String(device["sleeping"] as? Int ?? -1),String(device["revision"] as? UInt32 ?? 0),
         String(device["day_brightness"] as? Int ?? -1),String(device["night_brightness"] as? Int ?? -1),
         device["error"] as? String ?? "",String(profileMenuGeneration),selectedDeviceId,selectionEpoch,catalogSignature,selectedRole,String(hardware["ota_supported"] as? Bool ?? false),String(hardware["watch_csc"] as? Bool ?? false),String(gymCurrent["watch_streaming"] as? Bool ?? false),String(gymCurrent["pulses_per_revolution"] as? Int ?? 1),String(gymCurrent["ready"] as? Bool ?? false),String(gymCurrent["demo"] as? Bool ?? false),gymCurrent["units"] as? String ?? "",String(gymCurrent["distance_mm"] as? Int ?? 5000),connectionSettings["mode"] ?? "usb",connectionSettings["bluetooth_id"] ?? "",hardware["model"] as? String ?? "",String(wifiFirmware),String(hardware["control_version"] as? Int ?? 2),String(bluetoothScanRunning),String(wifiSetupRunning),connectionSettings["wifi_host"] ?? "",String(weatherLocationBusy),weatherPlace?.city ?? ""].joined(separator:"|")
    }
    func prepareMenuIfNeeded(){if !menuIsOpen && menuSignature != lastMenuSignature{rebuild()}}
    func menuWillOpen(_ menu:NSMenu){
        guard menu === self.menu else{return}
        menuIsOpen=true
        if menuActivity==nil {
            // Menu-bar apps have no ordinary foreground window. Keep this
            // user interaction awake without preventing the Mac from sleeping.
            menuActivity=ProcessInfo.processInfo.beginActivity(options:.userInitiatedAllowingIdleSystemSleep,reason:"Using display controls")
        }
        updateBrightnessFeedback();refreshSourceItems()
    }
    func endMenuActivity(){
        if let activity=menuActivity{ProcessInfo.processInfo.endActivity(activity);menuActivity=nil}
    }
    func menuDidClose(_ menu:NSMenu){
        guard menu === self.menu else{return}
        menuIsOpen=false;endMenuActivity()
        DispatchQueue.main.async{[weak self] in self?.prepareMenuIfNeeded()}
    }
    @discardableResult func add(_ title:String,to target:NSMenu?=nil,action:Selector?=nil,tag:Int=0,checked:Bool=false,enabled:Bool=true)->NSMenuItem{
        let entry=NSMenuItem(title:title,action:action,keyEquivalent:"");entry.target=self;entry.tag=tag;entry.state=checked ? .on:.off;entry.isEnabled=enabled;(target ?? menu).addItem(entry);return entry
    }
    func submenu(_ title:String)->NSMenu{let m=NSMenu();m.autoenablesItems=false;add(title).submenu=m;return m}
    func rebuild(){
        menu.removeAllItems();addDevicePicker();addFirmwareMenu()
        if selectedRole=="gym"{addGymMenus();lastMenuSignature=menuSignature;return}
        if selectedRole != "desktop" {
            add(connected ? "Identifying device…":"Selected device disconnected",enabled:false)
            add("Choose USB Device…",action:#selector(findUSBDisplay(_:)))
            menu.addItem(.separator());add("Quit Helper",action:#selector(quitHelper(_:)));lastMenuSignature=menuSignature;return
        }
        add(hardware["model"] as? String ?? "Identifying XIAO…",enabled:false)
        let current=scenes.first{$0.id==currentScene}?.name ?? "Ready"
        add(connected ? "Connected via \(health["transport"] as? String == "wifi" ? "Wi-Fi":health["transport"] as? String == "bluetooth" ? "Bluetooth":"USB") · \(current)":"Display disconnected · changes will queue",enabled:false)
        if hardware["firmware_update_recommended"] as? Bool == true{add("Older firmware · update for Weather & face filters",enabled:false)}
        if connected,(device["revision"] as? UInt32 ?? 0) != prefs.revision{add("Applying settings…",enabled:false)}
        if device["error"] is String{add("Waiting for display controls",enabled:false)}
        menu.addItem(.separator())
        let picker=submenu("Scenes")
        for scene in scenes{
            if scene.id==11 && (hardware["scene_count"] as? Int ?? 10)<10 {add("Weather · firmware update needed",to:picker,enabled:false);continue}
            if scene.id==2 || scene.id==11 {
                let entry=add(scene.name,to:picker,checked:currentScene==scene.id)
                let options=NSMenu();options.autoenablesItems=false;entry.submenu=options
                add("Show \(scene.name)",to:options,action:#selector(selectScene(_:)),tag:scene.id)
                options.addItem(.separator())
                if scene.id==2 {
                    if hardware["clock_filters"] as? Bool == false{add("Update firmware for individual face selection",to:options,enabled:false);continue}
                    let view=CarouselChecklist(scenes:clockFaces,mask:prefs.includedClocks,bitBase:0,
                        onToggle:{[weak self] id in self?.toggleClockFace(id)},onDone:{[weak self] in self?.menu.cancelTracking()})
                    view.allButton.title="Select / deselect all"
                    let item=NSMenuItem();item.view=view;options.addItem(item);clockChecklist=view
                }else{
                    add(weatherPlace.map{"Saved: \($0.city)"} ?? "No location selected",to:options,enabled:false)
                    add(weatherLocationBusy ? "Finding location…":"Select Location…",to:options,action:#selector(selectWeatherLocation(_:)),enabled: !weatherLocationBusy)
                }
            }else{add(scene.name,to:picker,action:#selector(selectScene(_:)),tag:scene.id,checked:currentScene==scene.id)}
        }
        let included=submenu("Include in Carousel")
        let checklist=CarouselChecklist(scenes:scenes,mask:prefs.enabledMask,onToggle:{[weak self] id in self?.toggleCarouselScene(id)},onDone:{[weak self] in self?.menu.cancelTracking()})
        let checklistItem=NSMenuItem();checklistItem.view=checklist;included.addItem(checklistItem);carouselChecklist=checklist
        let playback=submenu("Playback")
        add(prefs.enabledMask==0 ? "Carousel · paused (no scenes)":"Carousel",to:playback,action:#selector(playbackMode(_:)),tag:1,checked:prefs.carousel)
        add("Stay on selected scene",to:playback,action:#selector(playbackMode(_:)),tag:0,checked: !prefs.carousel)
        let interval=NSMenu();interval.autoenablesItems=false;add("Change scene every",to:playback).submenu=interval
        for minute in [1,5,10,15,30,60]{add("\(minute) minute\(minute==1 ? "":"s")",to:interval,action:#selector(changeInterval(_:)),tag:minute*60,checked:prefs.intervalSeconds==minute*60)}
        let profiles=submenu("Profiles")
        if let store=profileStore {
            if store.library.profiles.isEmpty{add("No saved profiles yet",to:profiles,enabled:false)}
            let snapshot=currentProfileSettings()
            for profile in store.library.profiles {
                let entry=add(profile.name,to:profiles,action:#selector(chooseProfile(_:)),checked:store.library.selectedProfileId==profile.id && profile.settings.matches(snapshot))
                entry.representedObject=profile.id
            }
            profiles.addItem(.separator())
            add("Save Current Settings as Profile…",to:profiles,action:#selector(saveProfile(_:)))
            if !store.library.profiles.isEmpty {
                let deletion=NSMenu();deletion.autoenablesItems=false;add("Delete Profile",to:profiles).submenu=deletion
                for profile in store.library.profiles {
                    let entry=add(profile.name,to:deletion,action:#selector(deleteProfile(_:)));entry.representedObject=profile.id
                }
            }
        }else{add("Profiles unavailable · saved file needs attention",to:profiles,enabled:false)}
        menu.addItem(.separator())
        let brightness=NSMenuItem();let box=SliderBox(value:actualBrightness,target:self,action:#selector(brightness(_:)),enabled:connected && device["boot_id"] != nil);box.label.stringValue="Brightness · \(actualBrightness)% · \(actualNight ? "night":"day")";brightness.view=box;sliderBox=box;menu.addItem(brightness);updateBrightnessFeedback()
        let appearance=submenu("Appearance")
        for (i,name) in ["Automatic · 17:00–07:30 night","Day","Night"].enumerated(){add(name,to:appearance,action:#selector(changeAppearance(_:)),tag:i,checked:prefs.appearance==i)}
        let off=(device["sleeping"] as? Int ?? (prefs.power==2 ? 1:0))==1
        add(off ? "Turn Screen On":"Turn Screen Off",action:#selector(toggleScreen(_:)))
        let power=submenu("Power")
        for (i,name) in ["Automatic · sleep 01:00–07:00","Always on","Screen off"].enumerated(){add(name,to:power,action:#selector(changePower(_:)),tag:i,checked:prefs.power==i)}
        let connections=submenu("Connection")
        let connectionMode=connectionSettings["mode"] ?? "usb",hasBluetooth=connectionSettings["bluetooth_id"] != nil && (connectionSettings["usb_serial"] == nil || connectionSettings["bluetooth_usb_serial"] == connectionSettings["usb_serial"] || connectionMode=="bluetooth")
        add("USB",to:connections,action:#selector(changeConnection(_:)),tag:0,checked:connectionMode=="usb")
        if wifiFirmware {
            add("Wi-Fi",to:connections,action:#selector(changeConnection(_:)),tag:3,checked:connectionMode=="wifi",enabled:connectionSettings["wifi_host"] != nil)
            add(wifiSetupRunning ? "Setting up Wi-Fi…":"Set Up Wi-Fi via USB…",to:connections,action:#selector(setupWifi(_:)),enabled: !wifiSetupRunning && connected && (health["transport"] as? String)=="usb")
        }else if wifiHardware{add("Wi-Fi needs a firmware update",to:connections,enabled:false)}
        else if hardware["identified"] as? Bool == true{add("USB & Bluetooth · this board has no Wi-Fi",to:connections,enabled:false)}
        else{add("Connect a display to identify its hardware",to:connections,enabled:false)}
        add("Bluetooth",to:connections,action:#selector(changeConnection(_:)),tag:1,checked:connectionMode=="bluetooth",enabled:hasBluetooth && (hardware["bluetooth"] as? Bool ?? false))
        add("Automatic · prefer USB",to:connections,action:#selector(changeConnection(_:)),tag:2,checked:connectionMode=="auto",enabled:connectionSettings["wifi_host"] != nil || hasBluetooth)
        add("Choose USB Display…",to:connections,action:#selector(findUSBDisplay(_:)))
        connections.addItem(.separator())
        add(bluetoothScanRunning ? "Searching for display…":"Find Bluetooth Display…",to:connections,action:#selector(findBluetooth(_:)),enabled: !bluetoothScanRunning)
        let sources=submenu("Data Sources")
        sourceEntries.removeAll()
        for key in ["codex","bambu","sun","weather"]{
            if let title=sourceTitle(key){sourceEntries[key]=add(title,to:sources,enabled:false)}
        }
        sources.addItem(.separator())
        add("Weather data · Open-Meteo (CC BY 4.0)…",to:sources,action:#selector(weatherCredits(_:)))
        menu.addItem(.separator());add("Quit Helper",action:#selector(quitHelper(_:)))
        lastMenuSignature=menuSignature
    }
    @objc func weatherCredits(_ sender:NSMenuItem){
        NSWorkspace.shared.open(URL(string:"https://open-meteo.com/")!)
    }
    func toggleClockFace(_ id:Int){
        load();guard id == -1 || clockFaces.contains(where:{$0.id==id}) else{return}
        let all=clockFaces.reduce(0){$0 | (1 << $1.id)}
        prefs.includedClocks=id == -1 ? (prefs.includedClocks==all ? 0:all):(prefs.includedClocks ^ (1 << id))
        if save(){clockChecklist?.update(mask:prefs.includedClocks)}
    }
    @objc func selectWeatherLocation(_ sender:NSMenuItem){
        guard !weatherLocationBusy else{return}
        menu.cancelTracking();NSApp.activate(ignoringOtherApps:true)
        weatherLocationBusy=true;prepareMenuIfNeeded()
        let panel=NSPanel(contentRect:NSRect(x:0,y:0,width:380,height:130),styleMask:[.titled],backing:.buffered,defer:false)
        panel.title="Weather location";panel.isReleasedWhenClosed=false
        let label=NSTextField(labelWithString:"Finding your location for local weather…")
        label.frame=NSRect(x:22,y:75,width:340,height:22);panel.contentView?.addSubview(label)
        let spinner=NSProgressIndicator(frame:NSRect(x:24,y:30,width:20,height:20));spinner.style = .spinning
        spinner.startAnimation(nil);panel.contentView?.addSubview(spinner)
        let cancel=NSButton(title:"Cancel",target:self,action:#selector(cancelWeatherLocation(_:)));cancel.bezelStyle = .rounded
        cancel.frame=NSRect(x:276,y:22,width:82,height:32);panel.contentView?.addSubview(cancel)
        weatherLocationPanel=panel;panel.center();panel.makeKeyAndOrderFront(nil)
        let request=WeatherLocationRequest();weatherLocationRequest=request
        DispatchQueue.main.async{[weak self] in
          guard let owner=self,owner.weatherLocationBusy,owner.weatherLocationRequest===request else{return}
          request.begin{[weak self] result in
            guard let self=self else{return};self.weatherLocationBusy=false;self.weatherLocationRequest=nil
            self.weatherLocationPanel?.close();self.weatherLocationPanel=nil
            switch result {
            case .success(let place):
                do{
                    try place.save(to:self.weatherLocationURL);self.weatherPlace=place
                    self.load();self.prefs.selectedScene=11;self.prefs.selectionToken=nextToken(self.prefs.selectionToken);self.save()
                    let alert=NSAlert();alert.messageText="Weather location saved"
                    alert.informativeText="\(place.city) is saved on this Mac. The forecast will refresh now. Location access stops after this fix; choose Select Location again if you move."
                    alert.runModal()
                }catch{self.showError("Could not save the weather location: \(error.localizedDescription)")}
            case .failure(let error):if (error as? WeatherLocationError) != .cancelled{self.showError(error.localizedDescription)}
            }
            self.prepareMenuIfNeeded()
          }
        }
    }
    @objc func cancelWeatherLocation(_ sender:NSButton){
        weatherLocationRequest?.cancel()
        weatherLocationRequest=nil;weatherLocationBusy=false;weatherLocationPanel?.close();weatherLocationPanel=nil
        prepareMenuIfNeeded()
    }
    func sourceTitle(_ key:String)->String? {
        guard let values=health["sources"] as? [String:[String:Any]],let value=values[key] else{return nil}
        let label=value["label"] as? String ?? key
        let delivered=value["source_observed_at"] as? Double ?? value["last_fetched_at"] as? Double ?? 0
        let age=max(0,Int(Date().timeIntervalSince1970-delivered))
        let status=(value["pending"] as? Bool ?? false) ? "Updating location…":delivered==0 ? "Waiting":age<60 ? "Updated \(age)s ago":"Updated \(age/60)m ago"
        return "\(label) · \(status)"
    }
    func refreshSourceItems(){
        refreshRideItems()
        for (key,entry) in sourceEntries{if let title=sourceTitle(key),entry.title != title{entry.title=title}}
    }
    func saveConnection(){
        do{
            let data=try JSONSerialization.data(withJSONObject:connectionSettings,options:[.prettyPrinted,.sortedKeys])
            try data.write(to:connectionURL,options:.atomic);try rememberConnection();prepareMenuIfNeeded()
        }catch{showError("Could not save connection: \(error.localizedDescription)")}
    }
    @objc func changeConnection(_ sender:NSMenuItem){
        guard (0...3).contains(sender.tag) else{return}
        if selectedRole=="gym" {
            if sender.tag==1 && hardware["watch_csc"] as? Bool == true{return}
            if sender.tag==1 && hardware["bluetooth"] as? Bool != true{return}
            if sender.tag==3 && !wifiFirmware{return}
            connectionSettings["bluetooth_for_workout"]="1"
        }
        connectionSettings["mode"]=["usb","bluetooth","auto","wifi"][sender.tag];connectionSettings["selection_epoch"]=UUID().uuidString;gymSendWork?.cancel();gymSendWork=nil;gymDesired=[:];gymPendingValues=[:];gymPendingToken=nil;saveConnection()
    }
    @objc func findUSBDisplay(_ sender:NSMenuItem){
        let python=Bundle.main.object(forInfoDictionaryKey:"XIAOPythonPath") as? String ?? ""
        let script=Bundle.main.resourceURL!.appendingPathComponent("companion/display_transport.py")
        DispatchQueue.global(qos:.userInitiated).async{[weak self] in
            let process=Process(),output=Pipe();process.executableURL=URL(fileURLWithPath:python)
            process.arguments=["-B",script.path,"--usb"];process.standardOutput=output
            var result:[String:Any]=[:]
            do{
                try process.run()
                let timeout=DispatchWorkItem{if process.isRunning{process.terminate()}}
                DispatchQueue.global().asyncAfter(deadline:.now()+10,execute:timeout)
                let data=output.fileHandleForReading.readDataToEndOfFile();process.waitUntilExit();timeout.cancel()
                result=(try? JSONSerialization.jsonObject(with:data)) as? [String:Any] ?? [:]
            }catch{result=["error":error.localizedDescription]}
            let snapshot=result
            DispatchQueue.main.async{
                guard let self=self else{return}
                guard let devices=snapshot["devices"] as? [[String:String]],!devices.isEmpty else{self.showError("No XIAO USB board found. Check the data cable and USB connection.");return}
                NSApp.activate(ignoringOtherApps:true)
                let alert=NSAlert();alert.messageText="Choose USB display";alert.informativeText="Select the connected board to control. Your scenes and profiles are kept."
                let choices=NSPopUpButton(frame:NSRect(x:0,y:0,width:390,height:28),pullsDown:false)
                for device in devices{choices.addItem(withTitle:"\(device["name"] ?? "USB board") · \(device["serial"] ?? "")")}
                if let index=devices.firstIndex(where:{$0["serial"]==self.connectionSettings["usb_serial"]}){choices.selectItem(at:index)}
                alert.accessoryView=choices;alert.addButton(withTitle:"Use Display");alert.addButton(withTitle:"Cancel")
                if alert.runModal() == .alertFirstButtonReturn {
                    let chosen=devices[choices.indexOfSelectedItem]
                    let serial=chosen["serial"] ?? ""
                    let id="usb:"+serial.filter{$0.isLetter || $0.isNumber}.lowercased()
                    self.selectDevice(id,record:["usb_serial":serial,"usb_device":chosen["port"] ?? ""],forceUSB:true)
                }
            }
        }
    }
    @objc func findBluetooth(_ sender:NSMenuItem){
        guard !bluetoothScanRunning else{return}
        if selectedRole=="gym" && hardware["watch_csc"] as? Bool == true{showWatchPairing(sender);return}
        if selectedRole=="gym" && hardware["bluetooth"] as? Bool != true{return}
        bluetoothScanRunning=true
        let requestedId=selectedDeviceId,requestedEpoch=selectionEpoch
        let python=Bundle.main.object(forInfoDictionaryKey:"XIAOPythonPath") as? String ?? ""
        let script=Bundle.main.resourceURL!.appendingPathComponent("companion/display_transport.py")
        DispatchQueue.global(qos:.userInitiated).async{[weak self] in
            let scanResult=BluetoothSearch.run(python:python,script:script)
            DispatchQueue.main.async{
                guard let self=self,!self.quitting else{return};self.bluetoothScanRunning=false
                guard self.selectedDeviceId==requestedId,self.selectionEpoch==requestedEpoch else{return}
                defer{self.prepareMenuIfNeeded()}
                if let error=scanResult["error"] as? String{self.showError(error+". Check System Settings → Privacy & Security → Bluetooth.");return}
                guard let devices=scanResult["devices"] as? [[String:String]],!devices.isEmpty else{
                    self.showError("No display found. Keep it powered on nearby. Its firmware must include Bluetooth support.");return
                }
                let alert=NSAlert();alert.messageText="Connect to Bluetooth Display"
                alert.informativeText="Choose your display. If macOS asks for a pairing code, enter the six digits shown on its screen."
                alert.addButton(withTitle:"Connect");alert.addButton(withTitle:"Cancel")
                let choices=NSPopUpButton(frame:NSRect(x:0,y:0,width:310,height:28))
                for device in devices{choices.addItem(withTitle:(device["name"] ?? "Display")+" · "+String((device["id"] ?? "").suffix(6)))}
                alert.accessoryView=choices;NSApp.activate(ignoringOtherApps:true)
                if alert.runModal() == .alertFirstButtonReturn{
                    guard self.selectedDeviceId==requestedId,self.selectionEpoch==requestedEpoch else{return}
                    self.connectionSettings["mode"]="bluetooth";self.connectionSettings["bluetooth_id"]=devices[choices.indexOfSelectedItem]["id"] ?? ""
                    self.connectionSettings["bluetooth_usb_serial"]=self.connectionSettings["usb_serial"]
                    self.saveConnection()
                }
            }
        }
    }
    @objc func selectScene(_ sender:NSMenuItem){load();prefs.selectedScene=sender.tag;prefs.selectionToken=prefs.selectionToken==UInt32.max ? 1:prefs.selectionToken+1;save()}
    func toggleCarouselScene(_ id:Int){
        load();guard id == -1 || scenes.contains(where:{$0.id==id}) else{return}
        let all=scenes.reduce(0){$0 | (1 << ($1.id-2))}
        prefs.enabledMask=id == -1 ? (prefs.enabledMask==all ? 0:all):prefs.enabledMask ^ (1 << (id-2))
        if !save(){load()}
        carouselChecklist?.update(mask:prefs.enabledMask)
    }
    @objc func playbackMode(_ sender:NSMenuItem){
        load();prefs.carousel=sender.tag==1
        if !prefs.carousel {
            let scene=currentScene;prefs.selectedScene=scenes.contains{$0.id==scene} ? scene:2
            prefs.selectionToken=prefs.selectionToken==UInt32.max ? 1:prefs.selectionToken+1
        };save()
    }
    @objc func changeInterval(_ sender:NSMenuItem){load();prefs.intervalSeconds=sender.tag;save()}
    @objc func changeAppearance(_ sender:NSMenuItem){load();prefs.appearance=sender.tag;save()}
    @objc func changePower(_ sender:NSMenuItem){load();prefs.power=sender.tag;save()}
    func currentProfileSettings()->ProfileSettings {
        let fresh=connected && device["error"]==nil && (device["revision"] as? UInt32)==prefs.revision
        let scene=fresh ? currentScene:((2...11).contains(prefs.selectedScene) ? prefs.selectedScene:currentScene)
        return ProfileSettings(prefs,currentScene:scene,day:fresh ? device["day_brightness"] as? Int:nil,night:fresh ? device["night_brightness"] as? Int:nil)
    }
    @objc func chooseProfile(_ sender:NSMenuItem){
        guard let store=profileStore,let id=sender.representedObject as? String,let profile=store.library.profiles.first(where:{$0.id==id}) else{return}
        load()
        let applyBrightness=connected && device["error"]==nil && device["boot_id"] != nil
        prefs=profile.settings.applying(to:prefs,applyBrightness:applyBrightness)
        guard save() else{return}
        if applyBrightness {
            pendingBrightnessRevision=prefs.revision;pendingBrightnessValue=actualNight ? prefs.nightBrightness:prefs.dayBrightness
            pendingBrightnessProfile=actualNight ? "night_brightness":"day_brightness"
        }
        do{try store.select(id);profileMenuGeneration+=1}catch{showError("Settings applied, but the selected profile could not be saved: \(error.localizedDescription)")}
    }
    @objc func saveProfile(_ sender:NSMenuItem){
        guard let store=profileStore else{return};load();let snapshot=currentProfileSettings()
        let alert=NSAlert();alert.messageText="Save Current Settings as Profile"
        alert.informativeText="Save the included scenes, playback, interval, appearance, brightness and power settings."
        alert.addButton(withTitle:"Save Profile");alert.addButton(withTitle:"Cancel")
        let field=NSTextField(frame:NSRect(x:0,y:0,width:300,height:24));field.placeholderString="Profile name, e.g. Work"
        alert.accessoryView=field;alert.window.initialFirstResponder=field;NSApp.activate(ignoringOtherApps:true)
        while alert.runModal() == .alertFirstButtonReturn {
            do{
                let name=try ProfileStore.cleanName(field.stringValue)
                if let existing=store.named(name) {
                    let replace=NSAlert();replace.messageText="Replace “\(existing.name)”?";replace.informativeText="Its saved settings will be replaced with the current settings."
                    replace.addButton(withTitle:"Replace");replace.addButton(withTitle:"Cancel")
                    if replace.runModal() != .alertFirstButtonReturn{continue}
                }
                try store.save(name:name,settings:snapshot);profileMenuGeneration+=1;return
            }catch{alert.informativeText=error.localizedDescription}
        }
    }
    @objc func deleteProfile(_ sender:NSMenuItem){
        guard let store=profileStore,let id=sender.representedObject as? String,let profile=store.library.profiles.first(where:{$0.id==id}) else{return}
        let alert=NSAlert();alert.messageText="Delete “\(profile.name)”?";alert.informativeText="This removes the saved profile. Your current settings and all scenes are kept."
        alert.addButton(withTitle:"Delete Profile");alert.addButton(withTitle:"Cancel");NSApp.activate(ignoringOtherApps:true)
        if alert.runModal() == .alertFirstButtonReturn{do{try store.delete(id);profileMenuGeneration+=1}catch{showError(error.localizedDescription)}}
    }
    @objc func toggleScreen(_ sender:NSMenuItem){
        load();let off=(device["sleeping"] as? Int ?? 0)==1
        if off {
            var c=Calendar(identifier:.gregorian);c.timeZone=displayTimezone
            let hour=c.component(.hour,from:Date());prefs.power=(hour>=1 && hour<7) ? 1:0
        }else{prefs.power=2};save()
    }
    @objc func brightness(_ sender:NSSlider){
        load();let value=Int(sender.doubleValue.rounded())
        // Preserve the other profile's real current value, not an old saved level.
        prefs.dayBrightness=device["day_brightness"] as? Int ?? 0;prefs.nightBrightness=device["night_brightness"] as? Int ?? 0
        if actualNight{prefs.nightBrightness=value}else{prefs.dayBrightness=value}
        prefs.brightnessToken=prefs.brightnessToken==UInt32.max ? 1:prefs.brightnessToken+1;save()
        pendingBrightnessRevision=prefs.revision;pendingBrightnessValue=value;pendingBrightnessProfile=actualNight ? "night_brightness":"day_brightness"
        sliderBox?.label.stringValue="Brightness · \(value)%";sliderBox?.feedback.stringValue="Applying \(value)%…"
    }
    func showError(_ message:String){let alert=NSAlert();alert.messageText="XIAO Display";alert.informativeText=message;alert.runModal()}
    @objc func quitHelper(_ sender:NSMenuItem){shutdown(unload:true)}
    func applicationShouldTerminate(_ sender:NSApplication)->NSApplication.TerminateReply {
        if quitting{return .terminateNow};shutdown(unload:false);return .terminateCancel
    }
    func applicationWillTerminate(_ notification:Notification){quitting=true;endMenuActivity();timer?.invalidate();signalWorker(SIGTERM)}

}
@main struct HelperMain {
    static func main(){
        if CommandLine.arguments.contains("--version"){print("XIAO Display Helper 2.14.1");return}
        let app=NSApplication.shared
        let delegate=AppDelegate();app.delegate=delegate;withExtendedLifetime(delegate){app.run()}
    }
}
