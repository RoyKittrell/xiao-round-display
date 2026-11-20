import AppKit
import CoreLocation
import Foundation

struct WeatherPlace:Codable {
    var city:String,latitude:Double,longitude:Double,utcOffsetSeconds:Int
    var timezone:String?=nil,accuracyMetres:Double?=nil,savedAt:Double?=nil
    func validate() throws {
        guard latitude.isFinite,longitude.isFinite,(-90...90).contains(latitude),(-180...180).contains(longitude),
              !city.trimmingCharacters(in:.whitespacesAndNewlines).isEmpty,city.count<=120,
              (-43200...50400).contains(utcOffsetSeconds) else {throw WeatherLocationError.invalid}
    }
    func save(to url:URL)throws {
        try validate();let encoder=JSONEncoder();encoder.keyEncodingStrategy = .convertToSnakeCase
        encoder.outputFormatting=[.prettyPrinted,.sortedKeys]
        try FileManager.default.createDirectory(at:url.deletingLastPathComponent(),withIntermediateDirectories:true)
        try encoder.encode(self).write(to:url,options:.atomic)
        try FileManager.default.setAttributes([.posixPermissions:0o600],ofItemAtPath:url.path)
    }
    static func load(_ url:URL)->WeatherPlace? {
        let decoder=JSONDecoder();decoder.keyDecodingStrategy = .convertFromSnakeCase
        guard let data=try? Data(contentsOf:url),let place=try? decoder.decode(Self.self,from:data),
              (try? place.validate()) != nil else{return nil};return place
    }
}

enum WeatherLocationError:LocalizedError {
    case denied,timedOut,invalid,unavailable,cancelled
    var errorDescription:String? {
        switch self {
        case .denied:return "Location access is unavailable. Enable Location Services for XIAO Display Helper in System Settings → Privacy & Security → Location Services. Your saved weather location has been kept."
        case .timedOut:return "The Mac could not obtain a location in time. Check that Wi-Fi and Location Services are enabled, then try Select Location again. Your saved location has been kept."
        case .invalid:return "The location returned was invalid. Your saved weather location has been kept."
        case .unavailable:return "The Mac could not determine its location. Check Wi-Fi and Location Services, then try again. Your saved location has been kept."
        case .cancelled:return "Location selection cancelled."
        }
    }
}

// Created only after an explicit menu action. Never instantiated at login and
// never used by the forecast worker. One fix is saved; continuous tracking ends.
final class WeatherLocationRequest:NSObject,CLLocationManagerDelegate {
    private var manager:CLLocationManager?,geocoder:CLGeocoder?
    private var timer:Timer?,completion:((Result<WeatherPlace,Error>)->Void)?
    private var locating=false,resolving=false,accuracyRequested=false
    func cancel(){finish(.failure(WeatherLocationError.cancelled))}
    func begin(_ callback:@escaping(Result<WeatherPlace,Error>)->Void){
        guard completion==nil else{return};completion=callback
        let m=CLLocationManager();manager=m;m.delegate=self;m.desiredAccuracy=kCLLocationAccuracyBest
        timer=Timer.scheduledTimer(withTimeInterval:90,repeats:false){[weak self] _ in self?.finish(.failure(WeatherLocationError.timedOut))}
        RunLoop.main.add(timer!,forMode:.common)
        authorize(m)
    }
    private func authorize(_ m:CLLocationManager){
        guard completion != nil else{return}
        switch m.authorizationStatus {
        case .notDetermined:m.requestWhenInUseAuthorization()
        case .restricted,.denied:finish(.failure(WeatherLocationError.denied))
        default:
            guard !locating,!resolving else{return}
            if m.accuracyAuthorization == .reducedAccuracy && !accuracyRequested {
                accuracyRequested=true
                m.requestTemporaryFullAccuracyAuthorization(withPurposeKey:"WeatherLocation"){[weak self] _ in
                    DispatchQueue.main.async{guard let self=self,let manager=self.manager else{return};self.authorize(manager)}
                }
            }else{locating=true;m.requestLocation()}
        }
    }
    func locationManagerDidChangeAuthorization(_ manager:CLLocationManager){authorize(manager)}
    func locationManager(_ manager:CLLocationManager,didUpdateLocations locations:[CLLocation]){
        guard completion != nil,!resolving else{return}
        guard let fix=locations.filter({$0.horizontalAccuracy>=0 && abs($0.timestamp.timeIntervalSinceNow)<120 && CLLocationCoordinate2DIsValid($0.coordinate)}).min(by:{$0.horizontalAccuracy<$1.horizontalAccuracy}) else{return}
        resolving=true;manager.stopUpdatingLocation()
        let g=CLGeocoder();geocoder=g
        g.reverseGeocodeLocation(fix,preferredLocale:Locale(identifier:"en_US")){[weak self] placemarks,error in
            DispatchQueue.main.async{
                guard let self=self,self.completion != nil else{return}
                let mark=placemarks?.first
                let name=mark?.locality ?? mark?.subAdministrativeArea ?? mark?.administrativeArea ?? mark?.country ?? "Local weather"
                let zone=mark?.timeZone
                // Open-Meteo resolves the coordinate timezone if geocoding
                // cannot supply it; no assumption about the Mac’s timezone.
                let place=WeatherPlace(city:name,latitude:fix.coordinate.latitude,longitude:fix.coordinate.longitude,
                    utcOffsetSeconds:zone?.secondsFromGMT() ?? 0,timezone:zone?.identifier,
                    accuracyMetres:fix.horizontalAccuracy,savedAt:Date().timeIntervalSince1970)
                do{try place.validate();self.finish(.success(place))}catch{self.finish(.failure(error))}
            }
        }
    }
    func locationManager(_ manager:CLLocationManager,didFailWithError error:Error){
        guard completion != nil else{return}
        if let cl=error as? CLError,cl.code == .locationUnknown{
            DispatchQueue.main.asyncAfter(deadline:.now()+2){[weak self] in guard let self=self,self.completion != nil,!self.resolving else{return};self.manager?.requestLocation()};return
        }
        finish(.failure(error))
    }
    private func finish(_ result:Result<WeatherPlace,Error>){
        guard let callback=completion else{return};completion=nil
        timer?.invalidate();timer=nil;manager?.stopUpdatingLocation();manager?.delegate=nil;manager=nil
        geocoder?.cancelGeocode();geocoder=nil;callback(result)
    }
}
