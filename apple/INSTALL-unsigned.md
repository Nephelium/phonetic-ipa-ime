# iOS 待签名 IPA 安装

交付文件名带 `待签名` 或 `unsigned`。其中包含面向 iPhone/iPad 的 arm64 App 和系统键盘扩展，最低 iOS 16。模拟器版本不能作为此文件的替代。

## 签名

IPA 当前没有有效的 Apple 开发者签名，不能直接点开安装。使用自己的开发者身份及匹配的描述文件重新签名，或从随附源码在 Xcode 中构建安装。

需要一起签名两个目标：

| 目标 | 默认标识 |
|---|---|
| 配套 App | `org.nephelium.phonetic-ipa.ios` |
| 键盘扩展 | `org.nephelium.phonetic-ipa.ios.keyboard` |
| 共享 App Group | `group.org.nephelium.phonetic-ipa` |

签名工具须保留 `PlugIns/PhoneticIPA-Keyboard.appex`，并为扩展提供有效签名。设备授权、有效期取决于使用的证书和描述文件。Apple 官方流程见 [分发说明](https://developer.apple.com/documentation/xcode/distributing-your-app-for-beta-testing-and-releases) 和 [Ad Hoc 描述文件](https://developer.apple.com/help/account/provisioning-profiles/create-an-ad-hoc-provisioning-profile)。

如果签名身份不能使用默认 App Group，应在源码中同时修改 `iOS/Shared.entitlements` 和 `Shared/InputModel.swift` 的组名，并为 App、扩展配置相同的新组。单纯替换 bundle ID 不足以保证设置共享。部分重签名工具会移除 App Group 权限，此时默认键盘可以独立加载资料，但 App 编辑的键位未必传到键盘。此情形尚未实机验收。

## 安装后

1. 打开国际音标输入法 App，查看“说明”。
2. 系统设置 → 通用 → 键盘 → 键盘 → 添加新键盘，添加国际音标输入法。
3. 需要 App 与键盘共享自定义方案时，为该键盘打开“允许完全访问”。本原型无联网输入功能。
4. 在普通文本框中长按地球键切换。密码等受系统限制的输入框会使用系统键盘。
5. 中模式输入拼音，点汉字或下方音标分别上屏。读音有歧义时先选读音，再点音标。
6. IPA 模式输入 CIN 编码。上划数字 2、1、4，再点候选，可输入 `˨˩˦`。底部“说明”随时可打开。

已通过的模拟器输入流程与尚未验证的实机签名、设置共享，在项目验证记录中分开列出。
