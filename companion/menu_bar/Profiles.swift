import Foundation

struct Preferences: Codable {
    var schemaVersion=1, revision:UInt32=1, enabledMask=1023
    var carousel=true, intervalSeconds=900, dayBrightness=60, nightBrightness=60
    var appearance=0, power=0, selectedScene = -1
    var selectionToken:UInt32=0, brightnessToken:UInt32=0
    // Optional storage keeps older preferences decodable; absent means all.
    var clockMask:Int?=nil
    var includedClocks:Int {get{clockMask ?? 1023}set{clockMask=newValue}}
}

func nextToken(_ value:UInt32)->UInt32 { value == UInt32.max ? 1:value+1 }

// Only user settings belong in a snapshot. USB revisions and action tokens
// always come from the live preferences, never from a saved profile.
struct ProfileSettings: Codable, Equatable {
    var enabledMask:Int, carousel:Bool, intervalSeconds:Int
    var dayBrightness:Int, nightBrightness:Int, appearance:Int, power:Int
    var selectedScene:Int
    var clockMask:Int?=nil
    var includedClocks:Int {clockMask ?? 1023}

    init(_ prefs:Preferences, currentScene:Int, day:Int?=nil, night:Int?=nil) {
        enabledMask=prefs.enabledMask;carousel=prefs.carousel;intervalSeconds=prefs.intervalSeconds
        dayBrightness=day ?? prefs.dayBrightness;nightBrightness=night ?? prefs.nightBrightness
        appearance=prefs.appearance;power=prefs.power
        clockMask=prefs.includedClocks
        selectedScene=(2...11).contains(currentScene) ? currentScene:2
        if carousel && enabledMask != 0 && enabledMask & (1 << (selectedScene-2)) == 0 {
            selectedScene=(2...11).first{enabledMask & (1 << ($0-2)) != 0} ?? 2
        }
    }
    func validate() throws {
        guard (0...1023).contains(enabledMask), (0...1023).contains(includedClocks), (60...3600).contains(intervalSeconds),
              (0...100).contains(dayBrightness), (0...100).contains(nightBrightness),
              (0...2).contains(appearance), (0...2).contains(power),
              (2...11).contains(selectedScene) else {throw ProfileError.invalidSettings}
    }
    func applying(to current:Preferences, applyBrightness:Bool)->Preferences {
        var result=current
        result.enabledMask=enabledMask;result.carousel=carousel;result.intervalSeconds=intervalSeconds
        result.appearance=appearance;result.power=power
        result.clockMask=includedClocks
        result.selectedScene=selectedScene
        if carousel && enabledMask != 0 && enabledMask & (1 << (selectedScene-2)) == 0 {
            result.selectedScene=(2...11).first{enabledMask & (1 << ($0-2)) != 0} ?? 2
        }
        result.selectionToken=nextToken(current.selectionToken)
        // Choosing a profile is an explicit action. Offline profile selection
        // still queues settings, but must not brighten a later device startup.
        if applyBrightness {
            result.dayBrightness=dayBrightness;result.nightBrightness=nightBrightness
            result.brightnessToken=nextToken(current.brightnessToken)
        }
        return result
    }
    func matches(_ current:ProfileSettings)->Bool {
        enabledMask==current.enabledMask && includedClocks==current.includedClocks && carousel==current.carousel && intervalSeconds==current.intervalSeconds &&
        dayBrightness==current.dayBrightness && nightBrightness==current.nightBrightness &&
        appearance==current.appearance && power==current.power && (carousel || selectedScene==current.selectedScene)
    }
}

struct DisplayProfile: Codable {
    var id:String, name:String, settings:ProfileSettings
}
struct ProfileLibrary: Codable {
    var schemaVersion=1
    var selectedProfileId:String?=nil
    var profiles:[DisplayProfile]=[]
}
enum ProfileError: LocalizedError {
    case invalidName, invalidSettings, invalidLibrary
    var errorDescription:String? {
        switch self {
        case .invalidName:return "Use a profile name between 1 and 80 characters."
        case .invalidSettings:return "This profile contains invalid display settings."
        case .invalidLibrary:return "The saved profiles file is invalid. It has been left untouched."
        }
    }
}

final class ProfileStore {
    let url:URL
    private(set) var library=ProfileLibrary()
    static func cleanName(_ name:String)throws->String {
        let value=name.trimmingCharacters(in:.whitespacesAndNewlines)
        guard !value.isEmpty, value.count<=80, !value.unicodeScalars.contains(where:{CharacterSet.controlCharacters.contains($0)}) else {throw ProfileError.invalidName}
        return value
    }
    init(url:URL)throws {
        self.url=url
        if FileManager.default.fileExists(atPath:url.path) {
            let decoder=JSONDecoder();decoder.keyDecodingStrategy = .convertFromSnakeCase
            library=try decoder.decode(ProfileLibrary.self,from:Data(contentsOf:url))
            guard library.schemaVersion==1, Set(library.profiles.map{$0.id}).count==library.profiles.count else {throw ProfileError.invalidLibrary}
            var names=Set<String>()
            for profile in library.profiles {
                let name=try Self.cleanName(profile.name)
                guard name==profile.name,!profile.id.isEmpty,names.insert(name.lowercased()).inserted else {throw ProfileError.invalidLibrary}
                try profile.settings.validate()
            }
        }
    }
    func named(_ name:String)->DisplayProfile? {
        library.profiles.first{$0.name.caseInsensitiveCompare(name) == .orderedSame}
    }
    private func commit(_ next:ProfileLibrary)throws {
        let encoder=JSONEncoder();encoder.keyEncodingStrategy = .convertToSnakeCase;encoder.outputFormatting=[.prettyPrinted,.sortedKeys]
        try FileManager.default.createDirectory(at:url.deletingLastPathComponent(),withIntermediateDirectories:true)
        try encoder.encode(next).write(to:url,options:.atomic)
        library=next
    }
    @discardableResult func save(name:String,settings:ProfileSettings)throws->DisplayProfile {
        let clean=try Self.cleanName(name);try settings.validate()
        var next=library
        let profile=DisplayProfile(id:named(clean)?.id ?? UUID().uuidString,name:clean,settings:settings)
        if let index=next.profiles.firstIndex(where:{$0.id==profile.id}){next.profiles[index]=profile}else{next.profiles.append(profile)}
        next.selectedProfileId=profile.id
        try commit(next);return profile
    }
    func select(_ id:String)throws {var next=library;next.selectedProfileId=id;try commit(next)}
    func delete(_ id:String)throws {
        var next=library;next.profiles.removeAll{$0.id==id}
        if next.selectedProfileId==id{next.selectedProfileId=nil}
        try commit(next)
    }
}
