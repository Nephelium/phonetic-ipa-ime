import Foundation
@main struct UpdateTests {
    static func main() throws {
        let data = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let vectors = try JSONSerialization.jsonObject(with: data) as! [String: Any]
        let cases = vectors["cases"] as! [[String: Any]]
        for platform in ["windows", "macos", "ios", "android"] {
            for test in cases {
                let body = try JSONSerialization.data(withJSONObject: test["feed"]!)
                precondition((UpdateFeed.parse(body, platform: platform) != nil) == (test["valid"] as! Bool), test["name"] as! String)
            }
        }
        for pair in vectors["versions"] as! [[Any]] {
            let a = ReleaseVersion(pair[0] as! String)!, b = ReleaseVersion(pair[1] as! String)!
            precondition((a == b ? 0 : a > b ? 1 : -1) == (pair[2] as! Int))
        }
        var dict = cases[0]["feed"] as! [String: Any]
        let server = UpdateFeed.parse(try JSONSerialization.data(withJSONObject: dict), platform: "macos")!
        dict["version"] = "0.1.10"
        let github = UpdateFeed.parse(try JSONSerialization.data(withJSONObject: dict), platform: "macos")!
        precondition(UpdateFeed.newest([server, github])?.version == "0.1.10")
        precondition(UpdateFeed.newest([github, server])?.version == "0.1.10")
        precondition(UpdateFeed.newest([server])?.version == server.version && UpdateFeed.newest([]) == nil)
        precondition(UpdateFeed.parse(Data(repeating: 32, count: 65537), platform: "macos") == nil)
        print("Apple update manifest, source independence, numeric versions and URL checks passed")
    }
}
