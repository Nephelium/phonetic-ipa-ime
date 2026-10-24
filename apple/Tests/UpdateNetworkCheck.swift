import Foundation
@main struct UpdateNetworkCheck {
    @MainActor static func main() async {
        let store = UpdateStore(defaults: UserDefaults(suiteName: "org.nephelium.ipa.update-network-check")!)
        await store.check(manual: true)
        print(store.status)
        precondition(store.status == "已是最新版本" && store.latest == nil, "Both public update sources must be current")
        print("Both live update sources current: PASS")
    }
}
