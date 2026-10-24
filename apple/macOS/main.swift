import AppKit
import InputMethodKit
import SwiftUI

@MainActor final class ApplicationDelegate: NSObject, NSApplicationDelegate {
    private var server: IMKServer?
    private var settings: NSWindow?
    private var updateNotice: NSPanel?
    private var updateTimer: Timer?
    private let settingsModel = InputModel()
    func applicationDidFinishLaunching(_ notification: Notification) {
        server = IMKServer(name: Bundle.main.object(forInfoDictionaryKey: "InputMethodConnectionName") as? String, bundleIdentifier: Bundle.main.bundleIdentifier)
        NSLog("PhoneticIPA InputMethodKit server initialized: %@", server == nil ? "no" : "yes")
        if CommandLine.arguments.contains("--settings") { openSettings() }
        UpdateStore.shared.onAvailable = { [weak self] in self?.showUpdateNotice() }
        Task { await UpdateStore.shared.check() }
        updateTimer = Timer.scheduledTimer(withTimeInterval: 3600, repeats: true) { _ in
            Task { @MainActor in await UpdateStore.shared.check() }
        }
    }
    private func showUpdateNotice() {
        guard updateNotice == nil, let latest = UpdateStore.shared.latest,
              let download = UpdateStore.shared.download else { return }
        let panel = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 360, height: 175),
                            styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
        panel.level = .popUpMenu; panel.isFloatingPanel = true; panel.hidesOnDeactivate = false
        panel.isReleasedWhenClosed = false
        panel.contentView = NSHostingView(rootView:
            VStack(alignment: .leading, spacing: 12) {
                Text("国际音标输入法有更新").font(.headline)
                Text("版本 " + latest.version + " 已可下载").foregroundStyle(.secondary)
                HStack {
                    Link("官网下载", destination: URL(string: download.url)!)
                    Link("GitHub", destination: URL(string: download.githubUrl)!)
                    Spacer()
                    Button("稍后") { [weak self] in self?.updateNotice?.orderOut(nil); self?.updateNotice = nil }
                }
            }.padding(20).frame(width: 360, height: 175).background(.regularMaterial))
        let bounds = NSScreen.main?.visibleFrame ?? NSRect(x: 0, y: 0, width: 1024, height: 768)
        panel.setFrameOrigin(NSPoint(x: bounds.maxX - 380, y: bounds.maxY - 195))
        updateNotice = panel; panel.orderFrontRegardless()
    }
    @objc func openSettings() {
        if settings == nil {
            let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1040, height: 760),
                                  styleMask: [.titled, .closable, .resizable, .miniaturizable], backing: .buffered, defer: false)
            window.title = "国际音标输入法 · 键位与音标"
            window.contentView = NSHostingView(rootView: SettingsView(model: settingsModel).tint(Color(red: 0.32, green: 0.36, blue: 0.72)))
            window.isReleasedWhenClosed = false
            settings = window
        }
        settings?.center(); settings?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        Task { await UpdateStore.shared.check() }
    }
}

@objc(PhoneticInputController)
@MainActor final class PhoneticInputController: IMKInputController {
    private let model = InputModel()
    private var panel: NSPanel?
    private weak var inputClient: (any IMKTextInput)?
    override init!(server: IMKServer!, delegate: Any!, client inputClient: Any!) {
        super.init(server: server, delegate: delegate, client: inputClient)
        self.inputClient = inputClient as? any IMKTextInput
        model.onCommit = { [weak self] text in self?.inputClient?.insertText(text, replacementRange: NSRange(location: NSNotFound, length: 0)) }
        model.onChange = { [weak self] in self?.updatePresentation() }
    }
    override func activateServer(_ sender: Any!) { inputClient = sender as? any IMKTextInput; model.reloadProfile() }
    override func deactivateServer(_ sender: Any!) { model.clear(); panel?.orderOut(nil); inputClient = nil }
    override func commitComposition(_ sender: Any!) {
        guard !model.raw.isEmpty else { return }
        inputClient = sender as? any IMKTextInput
        if model.pending != nil { model.pending = nil }
        if model.candidates.isEmpty { model.onCommit(model.raw); model.clear() }
        else { model.choose(model.selectedIndex, ipa: false) }
    }
    override func recognizedEvents(_ sender: Any!) -> Int { Int(NSEvent.EventTypeMask.keyDown.rawValue) }
    override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
        guard let event else { return false }
        inputClient = sender as? any IMKTextInput
        let flags = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        if flags.contains(.command) || flags.contains(.option) { return false }
        if event.keyCode == 49 && flags.contains(.shift) { model.toggleMode(); return true }
        let digits: [UInt16: String] = [18:"1",19:"2",20:"3",21:"4",23:"5"]
        if flags.contains(.shift), !flags.contains(.control), let digit = digits[event.keyCode] { model.tone(digit); return true }
        if event.keyCode == 36 && flags.contains(.control) {
            let hadComposition = !model.raw.isEmpty
            if flags.contains(.shift) { model.reopenReading(model.selectedIndex) }
            else { model.choose(model.selectedIndex, ipa: true) }
            return hadComposition
        }
        if flags.contains(.control) { return false }
        switch event.keyCode {
        case 53: if model.raw.isEmpty { return false }; model.clear(); return true
        case 51: if model.raw.isEmpty { return false }; model.backspace(); return true
        case 125: if model.raw.isEmpty { return false }; model.move(1); return true
        case 126: if model.raw.isEmpty { return false }; model.move(-1); return true
        case 121: if model.raw.isEmpty { return false }; model.turnPage(1); return true
        case 116: if model.raw.isEmpty { return false }; model.turnPage(-1); return true
        case 36, 49:
            if model.raw.isEmpty { return false }
            if model.candidates.isEmpty { model.onCommit(model.raw); model.clear() }
            else { model.choose(model.selectedIndex, ipa: false) }
            return true
        default: break
        }
        guard let text = event.characters, text.count == 1, let scalar = text.unicodeScalars.first,
              scalar.value >= 0x21, scalar.value <= 0x7e else { return false }
        if model.raw.isEmpty, let digit = Int(text), (1...9).contains(digit) { return false }
        if !model.raw.isEmpty && !model.raw.hasPrefix("tone:") && !model.raw.hasPrefix("code:"), let digit = Int(text), (1...9).contains(digit) {
            model.choose(digit - 1, ipa: false); return true
        }
        if model.raw.isEmpty && model.pinyin && !text.allSatisfy({ $0.isLetter }) { return false }
        model.append(text); return true
    }
    override func menu() -> NSMenu! {
        let menu = NSMenu()
        let mode = NSMenuItem(title: "切换汉字 / IPA（Shift＋空格）", action: #selector(toggleMode), keyEquivalent: "")
        mode.target = self; menu.addItem(mode)
        let settings = NSMenuItem(title: "键位与音标设置…", action: #selector(ApplicationDelegate.openSettings), keyEquivalent: "")
        settings.target = NSApp.delegate; menu.addItem(settings)
        return menu
    }
    @objc private func toggleMode() { model.toggleMode() }
    private func updatePresentation() {
        guard !model.raw.isEmpty else {
            inputClient?.setMarkedText("", selectionRange: NSRange(location: 0, length: 0), replacementRange: NSRange(location: NSNotFound, length: 0))
            panel?.orderOut(nil); return
        }
        inputClient?.setMarkedText(model.raw, selectionRange: NSRange(location: model.raw.utf16.count, length: 0), replacementRange: NSRange(location: NSNotFound, length: 0))
        // A background client's composition must not put a floating panel over another app.
        if let bundleID = inputClient?.bundleIdentifier(),
           !NSRunningApplication.runningApplications(withBundleIdentifier: bundleID).contains(where: { $0.isActive }) {
            panel?.orderOut(nil)
            return
        }
        if panel == nil {
            let next = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 680, height: 150), styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
            next.level = .popUpMenu; next.isFloatingPanel = true; next.hidesOnDeactivate = false
            next.backgroundColor = .windowBackgroundColor
            next.contentView = NSHostingView(rootView: CandidateStrip(model: model))
            panel = next
        }
        var caret = NSRect.zero
        _ = inputClient?.attributes(forCharacterIndex: 0, lineHeightRectangle: &caret)
        let screen = NSScreen.screens.first { $0.frame.intersects(caret) } ?? NSScreen.main
        let bounds = screen?.visibleFrame ?? NSRect(x: 0, y: 0, width: 1024, height: 768)
        let width = min(680, bounds.width)
        let height: CGFloat = model.pending == nil ? 145 : 180
        let x = min(max(caret.minX, bounds.minX), bounds.maxX - width)
        let y = caret.minY - height >= bounds.minY ? caret.minY - height : min(caret.maxY, bounds.maxY - height)
        panel?.setFrame(NSRect(x: x, y: y, width: width, height: height), display: true)
        panel?.orderFrontRegardless()
    }
}

MainActor.assumeIsolated {
    let app = NSApplication.shared
    let delegate = ApplicationDelegate()
    app.delegate = delegate
    app.setActivationPolicy(.accessory)
    app.run()
}
