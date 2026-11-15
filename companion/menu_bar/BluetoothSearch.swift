import Foundation
import Darwin

// Runs off the UI thread. Own only this short-lived search process; never
// terminate the persistent data worker or an unrelated Python process.
enum BluetoothSearch {
    static func run(python:String,script:URL,timeout:TimeInterval=45)->[String:Any] {
        let process=Process(),output=Pipe()
        process.executableURL=URL(fileURLWithPath:python)
        process.arguments=["-B",script.path];process.standardOutput=output
        do{try process.run()}catch{return ["error":error.localizedDescription]}
        let deadline=ProcessInfo.processInfo.systemUptime+timeout
        while process.isRunning && ProcessInfo.processInfo.systemUptime<deadline {
            Thread.sleep(forTimeInterval:0.05)
        }
        let timedOut=process.isRunning
        if timedOut {
            process.terminate()
            let grace=ProcessInfo.processInfo.systemUptime+1
            while process.isRunning && ProcessInfo.processInfo.systemUptime<grace {
                Thread.sleep(forTimeInterval:0.05)
            }
            if process.isRunning{kill(process.processIdentifier,SIGKILL)}
        }
        process.waitUntilExit()
        let data=output.fileHandleForReading.readDataToEndOfFile()
        if timedOut{return ["error":"Bluetooth search timed out. Allow the macOS Bluetooth prompt, then try again"]}
        guard process.terminationStatus==0 else{return ["error":"Bluetooth search exited before completing"]}
        return (try? JSONSerialization.jsonObject(with:data)) as? [String:Any]
            ?? ["error":"Bluetooth search did not return a result"]
    }
}
