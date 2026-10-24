import AppKit

struct InstallerPayload {
    let root: URL
    var app: URL { root.appendingPathComponent("Payload/PhoneticIPA.app") }
    var helper: URL { root.appendingPathComponent("Registration/RegisterInputSource.app") }
    var script: URL { root.appendingPathComponent("Scripts/install-macos.sh") }
    var target: URL { FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Input Methods/PhoneticIPA.app") }
    var installed: Bool { FileManager.default.fileExists(atPath: target.path) }

    func validate() throws {
        for path in [app.appendingPathComponent("Contents/Info.plist"), helper.appendingPathComponent("Contents/MacOS/RegisterInputSource"), script] {
            guard FileManager.default.fileExists(atPath: path.path) else {
                throw NSError(domain: "PhoneticIPAInstaller", code: 1, userInfo: [NSLocalizedDescriptionKey: "安装内容不完整，请重新下载并完整解压安装包。"])
            }
        }
    }
}

@MainActor final class InstallerController: NSObject, NSWindowDelegate {
    let payload: InstallerPayload
    let window: NSWindow
    let action = NSButton(title: "安装并启用", target: nil, action: nil)
    let openSettings = NSButton(title: "打开输入法设置", target: nil, action: nil)
    let closeButton = NSButton(title: "关闭", target: nil, action: nil)
    let status = NSTextField(wrappingLabelWithString: "")
    let detail = NSTextField(wrappingLabelWithString: "")
    let progress = NSProgressIndicator()
    let logView = NSTextView()
    private let logToggle = NSButton(title: "查看详细记录", target: nil, action: nil)
    private var detailedLog = ""
    private var showingDetails = false
    private(set) var busy = false
    private(set) var logURL: URL?
    private var runningProcess: Process?
    var completionObserver: ((Int32) -> Void)?

    init(payload: InstallerPayload) {
        self.payload = payload
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 650, height: 510), styleMask: [.titled, .closable, .miniaturizable], backing: .buffered, defer: false)
        super.init()
        window.title = "国际音标输入法 · 安装"
        window.delegate = self
        window.isReleasedWhenClosed = false
        buildView()
        if payload.installed {
            action.title = "升级并启用"
            status.stringValue = "已安装国际音标输入法，可更新安装文件。"
        } else {
            status.stringValue = "安装后即可从输入法菜单选择国际音标输入法。"
        }
    }

    private func buildView() {
        guard let view = window.contentView else { return }
        let icon = NSImageView(frame: NSRect(x: 30, y: 390, width: 88, height: 88))
        icon.image = NSApp.applicationIconImage
        icon.imageScaling = .scaleProportionallyUpOrDown
        view.addSubview(icon)
        let title = NSTextField(labelWithString: "安装国际音标输入法")
        title.font = .systemFont(ofSize: 25, weight: .semibold)
        title.frame = NSRect(x: 138, y: 437, width: 480, height: 34)
        view.addSubview(title)
        let subtitle = NSTextField(labelWithString: "拼音选音标 · 多音字选读音 · IPA Plus 点选")
        subtitle.textColor = .secondaryLabelColor
        subtitle.frame = NSRect(x: 138, y: 407, width: 480, height: 24)
        view.addSubview(subtitle)
        detail.stringValue = "点击下方按钮即可安装，无需命令行工具。\n仅安装到当前用户的输入法目录；升级会保留旧版本备份。"
        detail.font = .systemFont(ofSize: 14)
        detail.frame = NSRect(x: 30, y: 320, width: 590, height: 56)
        view.addSubview(detail)
        status.font = .systemFont(ofSize: 14, weight: .medium)
        status.frame = NSRect(x: 30, y: 257, width: 590, height: 53)
        view.addSubview(status)
        progress.isIndeterminate = true
        progress.style = .bar
        progress.frame = NSRect(x: 30, y: 238, width: 590, height: 14)
        progress.isHidden = true
        view.addSubview(progress)
        let scroll = NSScrollView(frame: NSRect(x: 30, y: 92, width: 590, height: 131))
        scroll.borderType = .bezelBorder
        scroll.hasVerticalScroller = true
        logView.isEditable = false
        logView.font = .systemFont(ofSize: 13)
        logView.string = "将为当前用户安装独立输入法。\n安装完成后可以打开设置，阅读图文教学。\n升级时保留旧版本和键位方案。"
        logView.isVerticallyResizable = true
        logView.autoresizingMask = [.width]
        logView.textContainer?.widthTracksTextView = true
        scroll.documentView = logView
        view.addSubview(scroll)
        logToggle.frame = NSRect(x: 30, y: 26, width: 150, height: 36)
        logToggle.bezelStyle = .rounded
        logToggle.target = self; logToggle.action = #selector(toggleLog)
        logToggle.isHidden = true
        view.addSubview(logToggle)
        action.frame = NSRect(x: 442, y: 26, width: 180, height: 36)
        action.bezelStyle = .rounded
        action.keyEquivalent = "\r"
        action.target = self; action.action = #selector(install)
        view.addSubview(action)
        closeButton.frame = NSRect(x: 342, y: 26, width: 90, height: 36)
        closeButton.bezelStyle = .rounded
        closeButton.target = self; closeButton.action = #selector(close)
        view.addSubview(closeButton)
        openSettings.frame = action.frame
        openSettings.bezelStyle = .rounded
        openSettings.target = self; openSettings.action = #selector(showSettings)
        openSettings.isHidden = true
        view.addSubview(openSettings)
        let version = Bundle(url: payload.app)?.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
        let credit = NSTextField(labelWithString: "基于水杉输入法开发 · " + version)
        credit.textColor = .secondaryLabelColor
        credit.font = .systemFont(ofSize: 11)
        credit.frame = NSRect(x: 30, y: 66, width: 420, height: 18)
        view.addSubview(credit)
    }

    @objc func install() {
        guard !busy else { return }
        do { try payload.validate() } catch { finish(code: 1, text: error.localizedDescription); return }
        busy = true
        action.isEnabled = false
        closeButton.isEnabled = false
        status.stringValue = "正在安装并启用输入法，请稍候…"
        progress.isHidden = false
        progress.startAnimation(nil)
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/bin/bash")
        process.arguments = [payload.script.path, "--repair-enabled-list", "--no-open"] + (payload.installed ? ["--update"] : [])
        var environment = ProcessInfo.processInfo.environment
        environment["IPA_MAC_APP"] = payload.app.path
        environment["IPA_REGISTRATION_APP"] = payload.helper.path
        // The runtime needs only tools shipped with macOS.
        environment["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
        process.environment = environment
        process.currentDirectoryURL = FileManager.default.homeDirectoryForCurrentUser
        do {
            let directory = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Logs/PhoneticIPA/Installer", isDirectory: true)
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            let url = directory.appendingPathComponent(UUID().uuidString + ".log")
            guard FileManager.default.createFile(atPath: url.path, contents: nil) else { throw CocoaError(.fileWriteUnknown) }
            let output = try FileHandle(forWritingTo: url)
            logURL = url
            process.standardOutput = output
            process.standardError = output
            logView.string = "正在校验并复制安装内容，请保持安装窗口打开。"
            process.terminationHandler = { [weak self] process in
                try? output.close()
                let text = (try? Data(contentsOf: url)).map { String(decoding: $0, as: UTF8.self) } ?? "无法读取安装记录。"
                Task { @MainActor in self?.finish(code: process.terminationStatus, text: text) }
            }
            runningProcess = process
            try process.run()
        } catch {
            finish(code: 1, text: error.localizedDescription)
        }
    }

    private func finish(code: Int32, text: String) {
        busy = false
        runningProcess = nil
        progress.stopAnimation(nil)
        progress.isHidden = true
        closeButton.isEnabled = true
        detailedLog = text
        showingDetails = false
        logToggle.title = "查看详细记录"
        logToggle.isHidden = false
        logView.font = .systemFont(ofSize: 13)
        if code == 0 {
            status.stringValue = "安装完成。请从菜单栏输入法菜单选择国际音标输入法。"
            detail.stringValue = "可以打开输入法设置查看图文教学和试用音标键盘。\n升级备份和安装日志已保存在当前用户目录。"
            action.isHidden = true
            openSettings.isHidden = false
            window.defaultButtonCell = openSettings.cell as? NSButtonCell
            logView.string = "安装文件校验通过，输入法已启用。\n从菜单栏选择国际音标输入法，即可开始输入。\n点击打开输入法设置，可查看四页图文教学。"
        } else {
            status.stringValue = "安装未完成，请查看下方记录。"
            detail.stringValue = "可重新下载并完整解压安装包后重试。已有文件与日志会保留；如果记录提示已安装但未启用，可在系统键盘设置中添加。"
            action.title = "重试安装"
            action.isEnabled = true
            logView.string = "本次安装未完成。请查看详细记录后重试，或保留记录用于反馈。"
        }
        completionObserver?(code)
    }

    @objc private func toggleLog() {
        showingDetails.toggle()
        logToggle.title = showingDetails ? "收起详细记录" : "查看详细记录"
        logView.font = showingDetails ? .monospacedSystemFont(ofSize: 11, weight: .regular) : .systemFont(ofSize: 13)
        logView.string = showingDetails ? detailedLog : status.stringValue + "\n" + detail.stringValue
        logView.scrollRangeToVisible(NSRange(location: 0, length: 0))
    }

    @objc private func showSettings() {
        let configuration = NSWorkspace.OpenConfiguration()
        configuration.arguments = ["--settings"]
        NSWorkspace.shared.openApplication(at: payload.target, configuration: configuration) { _, error in
            if let error { Task { @MainActor in self.status.stringValue = "输入法已安装，设置窗口未能打开：" + error.localizedDescription } }
            else {
                DistributedNotificationCenter.default().postNotificationName(
                    Notification.Name("org.nephelium.phonetic-ipa.show-settings"), object: nil,
                    userInfo: nil, deliverImmediately: true)
            }
        }
    }
    @objc private func close() { if !busy { window.close() } }
    func windowShouldClose(_ sender: NSWindow) -> Bool { !busy }
}

#if !IPA_INSTALLER_TESTS
@main @MainActor final class InstallerApplication: NSObject, NSApplicationDelegate {
    private var controller: InstallerController?
    static func main() {
        let app = NSApplication.shared
        let delegate = InstallerApplication()
        app.delegate = delegate
        app.setActivationPolicy(.regular)
        withExtendedLifetime(delegate) { app.run() }
    }
    func applicationDidFinishLaunching(_ notification: Notification) {
        guard let resources = Bundle.main.resourceURL else { NSApp.terminate(nil); return }
        controller = InstallerController(payload: InstallerPayload(root: resources))
        controller?.window.center()
        controller?.window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        controller?.busy == true ? .terminateCancel : .terminateNow
    }
}
#endif
