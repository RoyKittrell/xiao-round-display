import Foundation

@main struct ProfileChecks {
    static func main() throws {
        let root=FileManager.default.temporaryDirectory.appendingPathComponent("xiao-profiles-\(UUID().uuidString)")
        defer{try? FileManager.default.removeItem(at:root)}
        let url=root.appendingPathComponent("display-profiles.json")
        let store=try ProfileStore(url:url)
        var current=Preferences();current.enabledMask=384;current.carousel=true;current.intervalSeconds=300
        current.appearance=2;current.power=1;current.dayBrightness=40;current.nightBrightness=5
        current.revision=100;current.selectionToken=UInt32.max;current.brightnessToken=41
        // Saving from a manually selected excluded scene chooses a valid start.
        let snapshot=ProfileSettings(current,currentScene:3)
        assert(snapshot.selectedScene==9)
        let work=try store.save(name:"  Work  ",settings:snapshot)
        assert(work.name=="Work" && store.library.profiles.count==1)
        let reload=try ProfileStore(url:url)
        assert(reload.library.profiles[0].settings==snapshot && reload.library.selectedProfileId==work.id)
        // Older preferences/profile files have no face mask; that means all.
        let legacyDecoder=JSONDecoder();legacyDecoder.keyDecodingStrategy = .convertFromSnakeCase
        let oldPreferences=try legacyDecoder.decode(Preferences.self,from:Data(#"{"schema_version":1,"revision":1,"enabled_mask":511,"carousel":true,"interval_seconds":900,"day_brightness":60,"night_brightness":60,"appearance":0,"power":0,"selected_scene":2,"selection_token":0,"brightness_token":0}"#.utf8))
        assert(oldPreferences.includedClocks==1023)
        var subset=current;subset.includedClocks=513
        let selectedFaces=ProfileSettings(subset,currentScene:2)
        assert(selectedFaces.applying(to:Preferences(),applyBrightness:false).includedClocks==513)
        assert(!selectedFaces.matches(snapshot))
        let text=try String(contentsOf:url,encoding:.utf8)
        assert(text.contains("enabled_mask") && !text.contains("brightness_token") && !text.contains("revision"))

        var changed=Preferences();changed.revision=700;changed.selectionToken=UInt32.max;changed.brightnessToken=99
        let applied=snapshot.applying(to:changed,applyBrightness:true)
        assert(applied.revision==700 && applied.selectionToken==1 && applied.brightnessToken==100)
        assert(applied.enabledMask==384 && applied.carousel && applied.intervalSeconds==300 && applied.selectedScene==9)
        assert(applied.appearance==2 && applied.power==1 && applied.dayBrightness==40 && applied.nightBrightness==5)
        let offline=snapshot.applying(to:changed,applyBrightness:false)
        assert(offline.brightnessToken==99 && offline.dayBrightness==60 && offline.nightBrightness==60)
        // Carousel progress must not uncheck the active profile; hold differs.
        assert(snapshot.matches(ProfileSettings(applied,currentScene:10)))
        var held=snapshot;held.carousel=false
        var heldPrefs=applied;heldPrefs.carousel=false
        assert(!held.matches(ProfileSettings(heldPrefs,currentScene:10)))
        var altered=snapshot;altered.appearance=1
        assert(!snapshot.matches(altered))

        let replace=try reload.save(name:"work",settings:altered)
        assert(replace.id==work.id && reload.library.profiles.count==1)
        for name in ["", "   ",String(repeating:"a",count:81),"Work\nTest"] {
            do{_ = try reload.save(name:name,settings:snapshot);fatalError("Accepted invalid name")}catch ProfileError.invalidName{}
        }
        var emptyPrefs=current;emptyPrefs.enabledMask=0
        let empty=ProfileSettings(emptyPrefs,currentScene:10)
        assert(empty.selectedScene==10 && empty.applying(to:changed,applyBrightness:false).selectedScene==10)
        let paused=try reload.save(name:"Paused",settings:empty)
        let pausedReload=try ProfileStore(url:url);assert(pausedReload.library.profiles.last!.settings.enabledMask==0)
        try reload.delete(paused.id)
        var invalid=snapshot;invalid.enabledMask=1024
        let before=try Data(contentsOf:url)
        do{_ = try reload.save(name:"Broken",settings:invalid);fatalError("Accepted invalid scene mask")}catch ProfileError.invalidSettings{}
        let after=try Data(contentsOf:url);assert(after==before)
        let second=try reload.save(name:"All scenes",settings:ProfileSettings(Preferences(),currentScene:2))
        try reload.select(work.id);try reload.delete(second.id)
        assert(reload.library.selectedProfileId==work.id && reload.library.profiles.count==1)
        try reload.delete(work.id)
        assert(reload.library.profiles.isEmpty && reload.library.selectedProfileId==nil)
        try Data("bad json".utf8).write(to:url)
        do{_ = try ProfileStore(url:url);fatalError("Accepted corrupt file")}catch{}
        let corrupt=try String(contentsOf:url,encoding:.utf8);assert(corrupt=="bad json")
        print("PASS profiles: settings round trip, Work scene filtering, live tokens, offline brightness guard, match state, replacement, deletion and corrupt-file preservation")
    }
}
