import SwiftUI

@main struct PhoneticIPAApp: App {
    @StateObject private var model = InputModel()
    @State private var selectedTab = 0
    @Environment(\.scenePhase) private var scenePhase
    @Environment(\.openURL) private var openURL
    @StateObject private var updates = UpdateStore.shared
    var body: some Scene {
        WindowGroup {
            TabView(selection: $selectedTab) {
                NavigationStack {
                    SettingsView(model: model).navigationTitle("国际音标输入法")
                    .safeAreaInset(edge: .bottom) {
                        Text("在系统设置 → 通用 → 键盘中添加“国际音标输入法”。")
                            .font(.caption).padding(10).frame(maxWidth: .infinity).background(.regularMaterial)
                    }
                }.tabItem { Label("设置", systemImage: "keyboard") }.tag(0)
                NavigationStack { InputTrialView().navigationTitle("试打") }.tabItem { Label("试打", systemImage: "text.cursor") }.tag(1)
                NavigationStack { UsageGuideView().navigationTitle("使用说明") }.tabItem { Label("说明", systemImage: "questionmark.circle") }.tag(2)
            }.tint(Color(red: 0.32, green: 0.36, blue: 0.72))
            .task { await updates.check() }
            .onChange(of: scenePhase) { phase in
                if phase == .active { Task { await updates.check() } }
            }
            .alert("发现新版本 " + (updates.latest?.version ?? ""), isPresented: $updates.noticeVisible) {
                if let download = updates.download {
                    Button("官网下载") { openURL(URL(string: download.url)!) }
                    Button("GitHub 下载") { openURL(URL(string: download.githubUrl)!) }
                }
                Button("稍后", role: .cancel) {}
            } message: { Text(updates.latest?.notes ?? "") }
        }
    }
}

private struct InputTrialView: View {
    @State private var text = ""
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("点下面开始输入，再用地球键切换到国际音标输入法。").font(.subheadline).foregroundStyle(.secondary)
            TextEditor(text: $text).font(.ipa(28)).accessibilityIdentifier("typing-area")
                .padding(8).background(Color.secondary.opacity(0.06), in: RoundedRectangle(cornerRadius: 14))
        }.padding()
    }
}
