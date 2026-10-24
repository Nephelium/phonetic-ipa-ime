import AppKit
import Carbon

func fail(_ message: String) -> Never {
    fputs("FAIL " + message + "\n", stderr)
    exit(1)
}
guard CommandLine.arguments.count == 3, ["begin", "restore"].contains(CommandLine.arguments[1]) else {
    fputs("Usage: InstallerSelectionGuard begin|restore /new/session.json\n", stderr)
    exit(2)
}
let sources = TISCreateInputSourceList(nil, false).takeRetainedValue() as! [TISInputSource]
func identifier(_ source: TISInputSource) -> String {
    Unmanaged<CFString>.fromOpaque(TISGetInputSourceProperty(source, kTISPropertyInputSourceID)).takeUnretainedValue() as String
}
let recordURL = URL(fileURLWithPath: CommandLine.arguments[2])
if CommandLine.arguments[1] == "begin" {
    let current = TISCopyCurrentKeyboardInputSource().takeRetainedValue()
    guard let system = sources.first(where: { ["com.apple.keylayout.ABC", "com.apple.keylayout.US"].contains(identifier($0)) }) else { fail("A system keyboard is required before installation tests") }
    let record = ["source": identifier(current), "frontmost": NSWorkspace.shared.frontmostApplication?.bundleIdentifier ?? ""]
    try JSONSerialization.data(withJSONObject: record).write(to: recordURL, options: .withoutOverwriting)
    guard TISSelectInputSource(system) == noErr else { fail("Could not select the system keyboard") }
    print("System keyboard selected for dedicated installer checks")
} else {
    let record = try JSONSerialization.jsonObject(with: Data(contentsOf: recordURL)) as! [String: String]
    guard let source = sources.first(where: { identifier($0) == record["source"] }) else { fail("Original input source is missing") }
    guard TISSelectInputSource(source) == noErr,
          identifier(TISCopyCurrentKeyboardInputSource().takeRetainedValue()) == record["source"] else { fail("Could not restore the original input source") }
    if let bundle = record["frontmost"], !bundle.isEmpty,
       let app = NSRunningApplication.runningApplications(withBundleIdentifier: bundle).first {
        app.activate(options: [])
    }
    print("Original input source and foreground application restored")
}
