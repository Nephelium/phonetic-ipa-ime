import Foundation
import Combine
import SwiftUI

struct ReleaseVersion: Comparable {
    let text: String
    let parts: [Int]
    init?(_ text: String) {
        let tokens = text.split(separator: ".", omittingEmptySubsequences: false)
        guard tokens.count == 3, tokens.allSatisfy({ token in
            !token.isEmpty && token.count <= 5 && (token.count == 1 || token.first != "0") && token.allSatisfy({ $0 >= "0" && $0 <= "9" })
        }) else { return nil }
        self.text = text; parts = tokens.compactMap { Int($0) }
    }
    static func < (lhs: Self, rhs: Self) -> Bool { lhs.parts.lexicographicallyPrecedes(rhs.parts) }
}

struct UpdateDownload: Codable {
    let url: String
    let githubUrl: String
    let sha256: String
    let bytes: Int64
    static func trusted(_ value: String) -> Bool {
        guard let url = URLComponents(string: value), url.scheme == "https", url.user == nil, url.password == nil,
              url.query == nil, url.fragment == nil, url.port == nil,
              !url.path.contains(".."), !url.path.contains("\\") else { return false }
        return (url.host == "www.phonetictoolbox.com" && url.path.hasPrefix("/releases/phonetic-ipa-ime/")) ||
            (url.host == "github.com" && url.path.hasPrefix("/Nephelium/phonetic-ipa-ime/releases/download/"))
    }
    var valid: Bool {
        Self.trusted(url) && Self.trusted(githubUrl) && bytes > 0 && bytes <= 2_147_483_648 &&
            sha256.count == 64 && sha256.allSatisfy { $0.isASCII && ($0.isNumber || ("a"..."f").contains(String($0))) }
    }
}

struct UpdateFeed: Codable {
    let schema: Int
    let product: String
    let version: String
    let notes: String
    let platforms: [String: UpdateDownload]
    static func parse(_ data: Data, platform: String) -> UpdateFeed? {
        guard data.count <= 65_536, let value = try? JSONDecoder().decode(Self.self, from: data),
              value.schema == 1, value.product == "phonetic-ipa-ime", ReleaseVersion(value.version) != nil,
              value.notes.count <= 4000, value.platforms[platform]?.valid == true else { return nil }
        return value
    }
    static func newest(_ feeds: [Self]) -> Self? {
        feeds.filter { ReleaseVersion($0.version) != nil }.max { ReleaseVersion($0.version)! < ReleaseVersion($1.version)! }
    }
}

private final class UpdateRedirectPolicy: NSObject, URLSessionTaskDelegate {
    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) {
        completionHandler(Self.allowed(request.url) ? request : nil)
    }
    static func allowed(_ url: URL?) -> Bool {
        guard let url, url.scheme == "https", url.user == nil, url.password == nil else { return false }
        return ["www.phonetictoolbox.com", "raw.githubusercontent.com"].contains(url.host ?? "")
    }
}

@MainActor final class UpdateStore: ObservableObject {
    static let shared = UpdateStore()
    static let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.1.7"
    static let sources = [
        "https://www.phonetictoolbox.com/releases/phonetic-ipa-ime/update.json",
        "https://raw.githubusercontent.com/Nephelium/phonetic-ipa-ime/ipa-prototype/updates/latest.json"
    ]
    #if os(iOS)
    static let platform = "ios"
    #else
    static let platform = "macos"
    #endif
    @Published var status = "自动检查官网与 GitHub 的新版本"
    @Published var checking = false
    @Published var latest: UpdateFeed?
    @Published var noticeVisible = false
    @Published var automatic: Bool {
        didSet { defaults.set(automatic, forKey: "ipa.update.automatic") }
    }
    var onAvailable: (() -> Void)?
    private let defaults: UserDefaults
    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        automatic = defaults.object(forKey: "ipa.update.automatic") as? Bool ?? true
        if let data = defaults.data(forKey: "ipa.update.latest"), let feed = UpdateFeed.parse(data, platform: Self.platform),
           ReleaseVersion(feed.version)! > ReleaseVersion(Self.version)! { latest = feed }
    }
    var download: UpdateDownload? { latest?.platforms[Self.platform] }
    private static func fetch(_ endpoint: String) async -> UpdateFeed? {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.timeoutIntervalForRequest = 8; configuration.timeoutIntervalForResource = 12
        configuration.httpCookieStorage = nil; configuration.urlCache = nil
        let session = URLSession(configuration: configuration, delegate: UpdateRedirectPolicy(), delegateQueue: nil)
        defer { session.invalidateAndCancel() }
        do {
            var request = URLRequest(url: URL(string: endpoint)!)
            request.setValue("PhoneticIPA/" + version, forHTTPHeaderField: "User-Agent")
            request.setValue("application/json", forHTTPHeaderField: "Accept")
            let (stream, response) = try await session.bytes(for: request)
            guard let http = response as? HTTPURLResponse, http.statusCode == 200,
                  UpdateRedirectPolicy.allowed(http.url), http.expectedContentLength <= 65_536 else { return nil }
            var data = Data()
            for try await byte in stream {
                guard data.count < 65_536 else { return nil }
                data.append(byte)
            }
            return UpdateFeed.parse(data, platform: platform)
        } catch { return nil }
    }
    func check(manual: Bool = false) async {
        guard !checking else { return }
        if ProcessInfo.processInfo.environment["IPA_UPDATE_DISABLE_NETWORK"] == "1" { return }
        let now = Date().timeIntervalSince1970
        if !manual {
            guard automatic, now - defaults.double(forKey: "ipa.update.success") >= 86_400,
                  now - defaults.double(forKey: "ipa.update.attempt") >= 600 else { return }
        }
        checking = true; status = "正在检查官网与 GitHub…"
        defaults.set(now, forKey: "ipa.update.attempt")
        let feeds = await withTaskGroup(of: UpdateFeed?.self, returning: [UpdateFeed].self) { group in
            for endpoint in Self.sources { group.addTask { await Self.fetch(endpoint) } }
            var values: [UpdateFeed] = []
            for await feed in group { if let feed { values.append(feed) } }
            return values
        }
        checking = false
        guard let newest = UpdateFeed.newest(feeds) else { status = "暂时无法连接更新服务，请稍后重试"; return }
        // A failed source gets a short retry window instead of being treated as current for a day.
        if feeds.count == Self.sources.count { defaults.set(now, forKey: "ipa.update.success") }
        if ReleaseVersion(newest.version)! > ReleaseVersion(Self.version)! {
            if latest == nil || ReleaseVersion(newest.version)! >= ReleaseVersion(latest!.version)! {
                latest = newest; defaults.set(try? JSONEncoder().encode(newest), forKey: "ipa.update.latest")
            }
            status = "发现新版本 " + latest!.version
            if defaults.string(forKey: "ipa.update.notified") != latest!.version {
                defaults.set(latest!.version, forKey: "ipa.update.notified")
                noticeVisible = true; onAvailable?()
            }
        } else if let latest {
            status = "发现新版本 " + latest.version
        } else {
            status = feeds.count == 2 ? "已是最新版本" : "已检查一个来源，另一来源暂时无法连接"
        }
    }
}

struct UpdateSettingsView: View {
    @ObservedObject var updates = UpdateStore.shared
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("软件更新").font(.headline)
                Spacer()
                Text("当前版本 " + UpdateStore.version).font(.caption).foregroundStyle(.secondary)
            }
            Toggle("自动检查新版本", isOn: $updates.automatic)
            Text(updates.status).font(.subheadline).foregroundStyle(.secondary).accessibilityIdentifier("update-status")
            HStack {
                Button(updates.checking ? "正在检查…" : "检查更新") { Task { await updates.check(manual: true) } }
                    .disabled(updates.checking).accessibilityIdentifier("check-for-update")
                if let download = updates.download {
                    Link("官网下载", destination: URL(string: download.url)!)
                    Link("GitHub 下载", destination: URL(string: download.githubUrl)!)
                }
            }
            if let latest = updates.latest { Text(latest.notes).font(.caption).textSelection(.enabled) }
        }.padding(16).background(Color.secondary.opacity(0.06), in: RoundedRectangle(cornerRadius: 14))
    }
}
