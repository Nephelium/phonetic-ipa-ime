import AppKit
import SwiftUI

@main struct Snapshots {
    @MainActor static func main() throws {
        _ = NSApplication.shared
        NSApp.setActivationPolicy(.prohibited)
        let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
        try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
        let model = InputModel()
        func require(_ value: Bool, _ message: String) throws {
            if !value { throw NSError(domain: "InputModelChecks", code: 1, userInfo: [NSLocalizedDescriptionKey: message]) }
        }
        try require(model.mappings["a"]?.isEmpty == false, "Resources not loaded")
        var commits: [String] = []
        model.onCommit = { commits.append($0) }
        model.append("wu")
        guard let index = model.candidates.firstIndex(where: { $0.word == "恶" }) else { throw CocoaError(.fileReadCorruptFile) }
        model.page = index / model.pageSize
        let localIndex = index % model.pageSize
        model.choose(localIndex, ipa: true)
        try require(model.pending != nil && commits.isEmpty, "Opening readings committed text")
        try require(model.readingOptions.allSatisfy { $0.pinyin.hasPrefix("wu") }, "Unrelated syllable leaked")
        guard let wu4 = model.readingOptions.firstIndex(where: { $0.pinyin == "wu4" }) else { throw CocoaError(.fileReadCorruptFile) }
        model.chooseReading(wu4)
        try require(model.pending == nil && commits.isEmpty, "Choosing a reading committed text")
        model.choose(localIndex, ipa: true)
        try require(commits == ["wu˥˩"] && model.raw.isEmpty, "IPA commit differs")
        model.append("wu"); model.page = index / model.pageSize
        try require(model.visible[localIndex].reading?.ambiguous == true, "Reading leaked across compositions")
        model.choose(localIndex, ipa: false)
        try require(commits.last == "恶", "Hanzi action committed IPA")
        model.toggleMode(); model.append("a"); model.choose(0, ipa: false)
        try require(commits.last == "a", "CIN commit differs")
        model.tone("2"); model.tone("1"); model.tone("4")
        try require(model.candidates.map(\.word) == ["˨˩˦", "²¹⁴", "214"], "Tone candidates differ")
        model.choose(0, ipa: false)
        try require(commits.last == "˨˩˦", "Tone commit differs")
        let format = KeyboardProfile(mappings: model.mappings)
        let roundTrip = try JSONDecoder().decode(KeyboardProfile.self, from: JSONEncoder().encode(format))
        try require(roundTrip.mappings == format.mappings && roundTrip.format == "phonetic-ipa-keymap", "Profile round trip changed Unicode")
        print("Swift session: separate commits, explicit readings, composition isolation, CIN, tones and profile round trip passed")
        model.toggleMode(); model.status = ""
        try render(SettingsView(model: model), size: NSSize(width: 1180, height: 880), file: output.appendingPathComponent("mac-settings.png"))
        model.append("haoxue")
        if let position = model.candidates.firstIndex(where: { $0.word == "好学" }) {
            model.page = position / model.pageSize
            model.choose(position % model.pageSize, ipa: true)
        }
        try render(CandidateStrip(model: model), size: NSSize(width: 720, height: 190), file: output.appendingPathComponent("mac-reading-choice.png"))
        for system in ["ipa", "extipa", "voqs"] {
            try render(PlusPicker(initialSystem: system) { _ in }, size: NSSize(width: 1160, height: 820), file: output.appendingPathComponent("mac-plus-\(system).png"))
        }
    }
    @MainActor static func render<Content: View>(_ content: Content, size: NSSize, file: URL) throws {
        let window = NSWindow(contentRect: NSRect(origin: .zero, size: size), styleMask: .borderless, backing: .buffered, defer: false)
        window.appearance = NSAppearance(named: .aqua)
        let host = NSHostingView(rootView: content.frame(width: size.width, height: size.height).background(Color.white))
        window.contentView = host
        host.frame = NSRect(origin: .zero, size: size)
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.5))
        guard let representation = host.bitmapImageRepForCachingDisplay(in: host.bounds) else { throw CocoaError(.fileWriteUnknown) }
        host.cacheDisplay(in: host.bounds, to: representation)
        guard let png = representation.representation(using: .png, properties: [:]) else { throw CocoaError(.fileWriteUnknown) }
        try png.write(to: file)
    }
}
