import Foundation
import Combine
import CoreText

struct IPAOption: Codable, Identifiable {
    var pinyin: String
    var ipa: String
    var id: String { pinyin + ipa }
}
struct ReadingPart: Codable {
    var word: String
    var selected: Int
    var options: [IPAOption]
}
struct Reading: Codable {
    var text: String
    var ambiguous: Bool
    var parts: [ReadingPart]
}
struct Candidate: Identifiable {
    var word: String
    var canonical: String
    var ipa: String
    var symbol: Bool
    var reading: Reading?
    var id: String { word + "\t" + canonical }
}
struct KeyChord: Codable {
    var modifiers: UInt
    var virtualKey: UInt
    var output: String
}
struct KeyboardProfile: Codable {
    var format = "phonetic-ipa-keymap"
    var version = 1
    var layout = "104"
    var pageSize = 6
    var mappings: [String: [String]]
    var hotkeys: [KeyChord] = []
    enum CodingKeys: String, CodingKey { case format, version, layout, pageSize, hotkeys; case mappings = "sequences" }
}
extension KeyboardProfile {
    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        format = try container.decode(String.self, forKey: .format)
        version = try container.decode(Int.self, forKey: .version)
        layout = try container.decode(String.self, forKey: .layout)
        pageSize = try container.decodeIfPresent(Int.self, forKey: .pageSize) ?? 6
        mappings = try container.decode([String: [String]].self, forKey: .mappings)
        hotkeys = try container.decodeIfPresent([KeyChord].self, forKey: .hotkeys) ?? []
    }
}

enum IPAResources {
    static var directory: URL {
        Bundle.main.resourceURL!.appendingPathComponent("IPA")
    }
    static func registerFonts() {
        for name in ["DoulosSIL-Regular.ttf", "PTBIPAPlus-Regular.ttf"] {
            CTFontManagerRegisterFontsForURL(directory.appendingPathComponent(name) as CFURL, .process, nil)
        }
    }
    static var profileDirectory: URL {
        #if os(iOS)
        if let shared = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.org.nephelium.phonetic-ipa") {
            return shared
        }
        #endif
        return FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("PhoneticIPA", isDirectory: true)
    }
}

@MainActor final class InputModel: ObservableObject {
    @Published var raw = ""
    @Published var pinyin = true
    @Published var candidates: [Candidate] = []
    @Published var page = 0
    @Published var pageSize = 6
    @Published var selectedIndex = 0
    @Published var pending: Candidate?
    @Published var status = ""
    @Published var mappings: [String: [String]] = [:]
    var profileLayout = "104"
    var profileHotkeys: [KeyChord] = []
    private var engine: IPAEngine?
    var onCommit: (String) -> Void = { _ in }
    var onChange: () -> Void = {}

    init() {
        IPAResources.registerFonts()
        do {
            engine = try IPAEngine(resourcePath: IPAResources.directory.path)
            mappings = engine?.mappings() ?? [:]
            if let engine { captureMetadata(engine) }
            reloadProfile()
        } catch { status = error.localizedDescription }
    }
    var visible: [Candidate] {
        Array(candidates.dropFirst(page * pageSize).prefix(pageSize))
    }
    var pendingPartIndex: Int? {
        pending?.reading?.parts.firstIndex { $0.selected < 0 }
    }
    var readingOptions: [IPAOption] {
        guard let i = pendingPartIndex else { return [] }
        return pending?.reading?.parts[i].options ?? []
    }
    func refresh() {
        page = 0; selectedIndex = 0; pending = nil
        let records = engine?.candidates(raw, pinyin: pinyin) as? [[String: Any]] ?? []
        candidates = records.compactMap { item in
            guard let word = item["word"] as? String else { return nil }
            var reading: Reading?
            if let object = item["reading"], let data = try? JSONSerialization.data(withJSONObject: object) {
                reading = try? JSONDecoder().decode(Reading.self, from: data)
            }
            return Candidate(word: word, canonical: item["canonical"] as? String ?? "",
                             ipa: item["ipa"] as? String ?? "", symbol: item["symbol"] as? Bool ?? false,
                             reading: reading)
        }
        onChange()
    }
    func append(_ text: String) {
        guard text.utf8.count <= 64, raw.utf8.count + text.utf8.count <= 128 else { return }
        raw += text; refresh()
    }
    func tone(_ digit: String) {
        guard ["1", "2", "3", "4", "5"].contains(digit) else { return }
        guard raw.isEmpty || raw.hasPrefix("tone:") else {
            status = "先确认当前候选，再输入声调"; onChange(); return
        }
        status = ""
        if !raw.hasPrefix("tone:") { raw = "tone:" }
        append(digit)
    }
    func backspace() {
        if pending != nil { pending = nil; onChange(); return }
        if !raw.isEmpty { raw.removeLast(); refresh() }
    }
    func clear() {
        if status == "读音已选择，点击音标上屏" { status = "" }
        raw = ""; candidates = []; pending = nil; page = 0; selectedIndex = 0; onChange()
    }
    func toggleMode() { clear(); pinyin.toggle() }
    func turnPage(_ delta: Int) {
        page = min(max(0, page + delta), max(0, (candidates.count - 1) / pageSize))
        selectedIndex = 0; onChange()
    }
    func move(_ delta: Int) {
        selectedIndex = min(max(0, selectedIndex + delta), max(0, visible.count - 1)); onChange()
    }
    func choose(_ index: Int, ipa: Bool) {
        if pending != nil {
            chooseReading(index); return
        }
        guard visible.indices.contains(index) else { return }
        let item = visible[index]
        if ipa && !item.symbol {
            if item.reading?.ambiguous == true { pending = item; onChange(); return }
            guard !item.ipa.isEmpty else { status = "当前字表没有完整读音"; return }
        }
        onCommit(ipa && !item.symbol ? item.ipa : item.word)
        clear()
    }
    func chooseReading(_ index: Int) {
        guard var item = pending, var reading = item.reading,
              let partIndex = pendingPartIndex,
              reading.parts[partIndex].options.indices.contains(index) else { return }
        reading.parts[partIndex].selected = index
        if reading.parts.allSatisfy({ $0.selected >= 0 }) {
            reading.text = reading.parts.map { $0.options[$0.selected].ipa }.joined(separator: " ")
            reading.ambiguous = false; item.ipa = reading.text; item.reading = reading
            if let i = candidates.firstIndex(where: { $0.id == item.id }) { candidates[i] = item }
            pending = nil
            status = "读音已选择，点击音标上屏"
        } else { item.reading = reading; pending = item }
        onChange()
    }
    func reopenReading(_ index: Int) {
        guard visible.indices.contains(index), let engine else { return }
        var item = visible[index]
        let snapshot = engine.transcription(item.word, canonical: item.canonical)
        if let data = try? JSONSerialization.data(withJSONObject: snapshot),
           var reading = try? JSONDecoder().decode(Reading.self, from: data) {
            for i in reading.parts.indices where reading.parts[i].options.count > 1 { reading.parts[i].selected = -1 }
            guard reading.parts.contains(where: { $0.selected < 0 }) else { return }
            item.reading = reading; pending = item; onChange()
        }
    }
    func applyMappings(_ entries: [String: [String]], pageSize: Int) throws {
        guard (3...9).contains(pageSize), !entries.isEmpty, entries.count <= 10000,
              entries.allSatisfy({ !$0.key.isEmpty && $0.key.utf8.count <= 128 && !$0.key.unicodeScalars.contains(where: { $0.value <= 32 || $0.value == 127 }) && !$0.value.isEmpty && $0.value.count <= 200 && $0.value.allSatisfy({ !$0.isEmpty && !$0.contains("\n") && !$0.contains("\r") && !$0.contains("\t") && !$0.contains("\0") && $0.utf8.count <= 1024 }) }) else {
            throw NSError(domain: "PhoneticIPA", code: 2, userInfo: [NSLocalizedDescriptionKey: "方案格式无效，请检查编码、候选和每页数量"])
        }
        var normalized = entries
        for key in (1...9).map(String.init) {
            if let values = normalized.removeValue(forKey: key) {
                normalized["\\", default: []] += values
            }
        }
        let folder = IPAResources.profileDirectory
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let snapshot = KeyboardProfile(layout: profileLayout, pageSize: pageSize, mappings: normalized, hotkeys: profileHotkeys)
        let encoded = try JSONEncoder().encode(snapshot)
        let candidateURL = folder.appendingPathComponent("candidate-\(UUID().uuidString).cin")
        try cinText(normalized).write(to: candidateURL, atomically: true, encoding: .utf8)
        let validator = try IPAEngine(resourcePath: IPAResources.directory.path)
        _ = try validator.loadCIN(candidateURL.path)
        try encoded.write(to: folder.appendingPathComponent("profile.json"), options: .atomic)
        _ = try engine?.loadCIN(candidateURL.path)
        mappings = normalized; self.pageSize = pageSize; status = "方案已保存"; refresh()
    }
    func cinText(_ entries: [String: [String]]) -> String {
        "%gen_inp\n%ename PhoneticIPA\n%selkey 123456789\n%ipa_layout \(profileLayout)\n" + profileHotkeys.map { "%ipa_hotkey \($0.modifiers) \($0.virtualKey)\t\($0.output)\n" }.joined() + "%chardef begin\n" + entries.keys.sorted().flatMap { key in
            entries[key, default: []].map { key + "\t" + $0 }
        }.joined(separator: "\n") + "\n%chardef end\n"
    }
    func reloadProfile() {
        let file = IPAResources.profileDirectory.appendingPathComponent("profile.json")
        guard FileManager.default.fileExists(atPath: file.path) else { return }
        do {
            let data = try Data(contentsOf: file)
            guard data.count <= 8 * 1024 * 1024 else { throw CocoaError(.fileReadTooLarge) }
            let profile = try JSONDecoder().decode(KeyboardProfile.self, from: data)
            // Loading performs no shared writes. The derived CIN stays in this process's temp directory.
            guard profile.format == "phonetic-ipa-keymap", profile.version == 1, (3...9).contains(profile.pageSize), !profile.mappings.isEmpty else { throw CocoaError(.fileReadCorruptFile) }
            profileLayout = profile.layout; profileHotkeys = profile.hotkeys
            let temporary = FileManager.default.temporaryDirectory.appendingPathComponent("ipa-profile-\(UUID().uuidString).cin")
            try cinText(profile.mappings).write(to: temporary, atomically: true, encoding: .utf8)
            _ = try engine?.loadCIN(temporary.path)
            mappings = profile.mappings; pageSize = profile.pageSize
        } catch { status = "方案读取失败：\(error.localizedDescription)" }
    }
    func importProfile(_ url: URL) throws {
        let data = try Data(contentsOf: url)
        guard data.count <= 8 * 1024 * 1024 else { throw CocoaError(.fileReadTooLarge) }
        if url.pathExtension.lowercased() == "json" {
            let profile = try JSONDecoder().decode(KeyboardProfile.self, from: data)
            guard profile.format == "phonetic-ipa-keymap", profile.version == 1,
                  ["104", "87", "75", "65", "60"].contains(profile.layout), profile.hotkeys.count <= 1000 else { throw CocoaError(.fileReadCorruptFile) }
            let previousLayout = profileLayout; let previousChords = profileHotkeys
            profileLayout = profile.layout; profileHotkeys = profile.hotkeys
            do { try applyMappings(profile.mappings, pageSize: profile.pageSize) }
            catch { profileLayout = previousLayout; profileHotkeys = previousChords; throw error }
        } else {
            let temporary = try IPAEngine(resourcePath: IPAResources.directory.path)
            _ = try temporary.loadCIN(url.path)
            let previousLayout = profileLayout; let previousChords = profileHotkeys
            captureMetadata(temporary)
            do { try applyMappings(temporary.mappings(), pageSize: pageSize) }
            catch { profileLayout = previousLayout; profileHotkeys = previousChords; throw error }
        }
    }
    private func captureMetadata(_ source: IPAEngine) {
        let metadata = source.profileMetadata()
        profileLayout = metadata["layout"] as? String ?? "104"
        if let object = metadata["hotkeys"], let data = try? JSONSerialization.data(withJSONObject: object),
           let chords = try? JSONDecoder().decode([KeyChord].self, from: data) { profileHotkeys = chords }
    }
}
