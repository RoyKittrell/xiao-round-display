import Foundation

@main struct WeatherLocationChecks {
 static func main()throws {
  let dir=FileManager.default.temporaryDirectory.appendingPathComponent("weather-place-\(UUID().uuidString)")
  defer{try? FileManager.default.removeItem(at:dir)}
  let url=dir.appendingPathComponent("weather-location.json")
  let place=WeatherPlace(city:"Kathmandu",latitude:27.7172,longitude:85.324,utcOffsetSeconds:20700,
      timezone:"Asia/Kathmandu",accuracyMetres:42,savedAt:1794890000)
  try place.save(to:url)
  let restored=WeatherPlace.load(url)!
  assert(restored.timezone=="Asia/Kathmandu"&&restored.utcOffsetSeconds==20700)
  assert(restored.latitude==place.latitude&&restored.accuracyMetres==42)
  let saved=try Data(contentsOf:url)
  var invalid=place;invalid.latitude=100
  do{try invalid.save(to:url);fatalError("Invalid fix overwrote saved location")}catch{}
  let after=try Data(contentsOf:url);assert(after==saved)
  let attrs=try FileManager.default.attributesOfItem(atPath:url.path)
  assert((attrs[.posixPermissions] as? NSNumber)?.intValue==0o600)
  // Existing pre-geolocation KL files remain readable without a new request.
  try Data(#"{"city":"Kuala Lumpur","latitude":3.139,"longitude":101.6869,"utc_offset_seconds":28800}"#.utf8).write(to:url)
  assert(WeatherPlace.load(url)?.city=="Kuala Lumpur")
  print("PASS saved location: exact fix, timezone, legacy load, private file, invalid-fix preservation; no macOS location request made.")
 }
}
