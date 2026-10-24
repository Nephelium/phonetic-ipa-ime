import AppKit

@main @MainActor final class InstallerUITests: NSObject, NSApplicationDelegate {
    var controller: InstallerController?
    var exitCode: Int32 = 1
    @discardableResult func require(_ condition: Bool, _ message: String) -> Bool {
        guard condition else {
            print("FAIL " + message)
            exitCode = 1
            exit(self.exitCode)
        }
        return true
    }
    static func main() {
        let app = NSApplication.shared
        let delegate = InstallerUITests()
        app.delegate = delegate
        app.setActivationPolicy(.regular)
        withExtendedLifetime(delegate) { app.run() }
        exit(delegate.exitCode)
    }
    func applicationDidFinishLaunching(_ notification: Notification) {
        guard CommandLine.arguments.count == 3 else { exit(2) }
        let resources = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
        let screenshot = URL(fileURLWithPath: CommandLine.arguments[2])
        NSApp.applicationIconImage = NSImage(contentsOf: resources.appendingPathComponent("Installer.icns"))
        NSApp.appearance = NSAppearance(named: .aqua)
        let ui = InstallerController(payload: InstallerPayload(root: resources))
        controller = ui
        guard require(ui.payload.installed, "Upgrade test needs the existing dedicated input method"),
              require(ui.action.title == "升级并启用" && ui.action.isEnabled, "Initial upgrade controls") else { return }
        ui.completionObserver = { [weak self, weak ui] code in
            guard let self, let ui else { return }
            guard code == 0 else {
                print("FAIL Actual installer backend; log: " + (ui.logURL?.path ?? "unavailable"))
                self.exitCode = 1
                exit(self.exitCode)
            }
            guard self.require(!ui.busy && ui.closeButton.isEnabled && ui.action.isHidden && !ui.openSettings.isHidden, "Completion controls"),
                  self.require(ui.status.stringValue.contains("安装完成") && ui.logURL != nil, "Completion status") else { return }
            let backend = (try? String(contentsOf: ui.logURL!, encoding: .utf8)) ?? ""
            guard self.require(backend.contains("atomic-contents-exchange=1"), "Upgrade must atomically exchange Contents") else { return }
            guard self.require(!ui.logView.string.contains("TISProperty") && !ui.logView.string.contains("/Users/"), "Default summary must be readable without internal diagnostics"),
                  let logButton = ui.window.contentView?.subviews.compactMap({ $0 as? NSButton }).first(where: { $0.title == "查看详细记录" }) else { return }
            logButton.performClick(nil)
            guard self.require(ui.logView.string.contains("ready=1"), "Detailed installation log remains accessible") else { return }
            logButton.performClick(nil)
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                ui.window.displayIfNeeded()
                let screen = Process()
                screen.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
                screen.arguments = ["-x", "-o", "-l", String(ui.window.windowNumber), screenshot.path]
                do { try screen.run(); screen.waitUntilExit() } catch { print("Own-window capture unavailable: " + error.localizedDescription) }
                if screen.terminationStatus == 0 {
                    print("Native installer own-window screenshot and backend passed")
                    self.exitCode = 0; exit(self.exitCode)
                }
                guard let view = ui.window.contentView,
                      let bitmap = view.bitmapImageRepForCachingDisplay(in: view.bounds) else {
                    self.require(false, "Native render context unavailable"); return
                }
                view.cacheDisplay(in: view.bounds, to: bitmap)
                // The content view is transparent because NSWindow paints its
                // background. Include that background in the offscreen render.
                let rendered = NSImage(size: view.bounds.size)
                rendered.lockFocus()
                NSColor(calibratedWhite: 0.96, alpha: 1).setFill()
                NSBezierPath(rect: view.bounds).fill()
                let controls = NSImage(size: view.bounds.size)
                controls.addRepresentation(bitmap)
                controls.draw(in: view.bounds, from: .zero, operation: .sourceOver, fraction: 1)
                rendered.unlockFocus()
                guard let tiff = rendered.tiffRepresentation,
                      let png = NSBitmapImageRep(data: tiff)?.representation(using: .png, properties: [:]) else {
                    self.require(false, "Native PNG rendering failed"); return
                }
                do { try png.write(to: screenshot) } catch { self.require(false, "Could not save native render"); return }
                print("Native installer: initial state, real upgrade button, progress, completion, opaque AppKit render and log checks passed")
                self.exitCode = 0
                exit(self.exitCode)
            }
        }
        ui.window.center()
        ui.window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        ui.action.performClick(nil)
        guard require(ui.busy && !ui.action.isEnabled && !ui.closeButton.isEnabled, "Busy controls"),
              require(!ui.windowShouldClose(ui.window), "Closing during installation must be rejected") else { return }
        DispatchQueue.main.asyncAfter(deadline: .now() + 90) { [weak self] in
            guard let self, self.exitCode != 0 else { return }
            print("Installer UI timed out"); exit(self.exitCode)
        }
    }
}
