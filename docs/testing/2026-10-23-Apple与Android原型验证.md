# Apple 与 Android 原型验证

日期：2026-10-23。Windows 为源码与备份主副本，Mac 为远程构建副本。全部 Windows 测试使用后台进程或本项目无窗口模拟器，没有占用当前桌面。

补充：本页保留最初跨平台检查点。随后已修复 Mac 输入源选择并通过真实宿主验证，最新状态见 [Mac 修复记录](2026-10-23-Mac输入源修复与真实输入验证.md)。

## 交付范围

| 平台 | 构建与验证结果 |
|---|---|
| iOS / iPadOS | App、键盘扩展、真机 arm64 Release archive 构建成功。iPhone 17 / iOS 27.0 模拟器实际系统键盘 UI 测试通过。提供待签名 IPA，尚未验证实体 iPhone 安装及 App Group 设置共享。 |
| Android | Android 10+，arm64-v8a / x86_64 测试 APK。Android 36.1 独立无窗口模拟器通过打包 JNI 核心与实际 InputMethodService 上屏测试。尚未验证实体 Android 手机。 |
| macOS | 原生应用、共享核心与 Swift 输入会话测试通过。系统输入源可以枚举、启用，但选择仍返回 `-50`，真实宿主上屏未通过。没有注销或重启 Mac。 |
| Windows | 本次共享读音头文件保持兼容，重新构建并通过 `IpaReadingTests`。未把本次手机测试算作 Windows TSF 宿主验证。 |

## 实际输入检查

- 使用原生候选按钮提交汉字、IPA，分别断言宿主文本。
- `wu → 恶` 的读音选项不包含 `e3`。选择 `wu4` 时不提前提交，再点 IPA 得到 `wu˥˩`。
- Android 另验证 `yanwu → 厌恶`。共享核心验证银行、长大、音乐、重庆等完整词语读音；好学保留歧义。
- IPA 模式的 `a` CIN 输入。
- 上划数字 2、1、4，选择 `˨˩˦`。无候选时轻点 6 输入数字。
- iOS 主流程宿主文本为 `恶wu˥˩a˨˩˦6`，Android 为 `厌恶wu˥˩a˨˩˦6`。
- 帮助页打开、滚动和返回不改变原有输入。
- 切符号页，点击 `\` 并选择 `˥`，点击 `/` 并选择 `/`，两个平台宿主文本都追加 `˥/`。
- 1–9 的独立音标映射迁至 `\`，0 原映射保留。每页候选 3–9。

Android 核心还覆盖补充平面 Unicode、含空格输出、无效方案的原子拒绝、分页与数字输入。Apple 核心和 Swift 会话覆盖词语读音、手动选择、不同输入会话之间的选择隔离、CIN、三种调值格式及 JSON 往返。

## 界面和问题修复

- 数字行保留，Shift 与退格移到 zxc 行两侧，asdf 行缩进，底部加宽空格、回车。
- 删除冗余声调页，增加键盘“说明”和 App 使用说明。键帽左上角小字为编码，中央大字为首个输出。
- IPA Plus 继续以分类矩阵排布，Android 实际打开 IPA、extIPA、VoQS 并截图。
- 新增小蓝鸟图标，完整提示词和原图哈希见 `assets/icons/README.md`。
- 修复 Android 重复构建字体导致的内存分配问题，复用 Typeface；自定义调值键保留 `performClick` 无障碍点击。
- 修复测试中误点 App 同名“说明”页的问题，键盘按钮使用独立标识。
- iOS 改用可自适应高度的 UIInputView。模拟器首次恢复输入焦点时曾出现键盘节点在屏幕外，测试先确认可点击，必要时重新点宿主文本框，随后通过真实按键与滑动验证。没有直接调用提交函数代替 UI 操作。实体设备的首次弹出表现仍待检查。

## 证据与命令

- iOS：`KeyboardUITests-17.xcresult`，1 项端到端测试、0 失败，包含上述多个断言与截图。使用 `xcodebuild test`、`PhoneticIPA-iOS` scheme，指定本项目模拟器 UDID。
- Apple：`build.sh` 构建日志与 `snapshots.sh` 的 `session-tests.log`。后台原生预览不能代替 Mac 真实输入。
- Android：`output/android-qa/verified-20261023-185543/` 的 `core.log`、`keyboard.log` 与 8 张截图。构建 10 成功。最近 lint 为 0 错误、1 个本地空旧资源目录提示，不影响打包；空目录不进入 Git。
- Windows：重建 `IpaReadingTests` 并用固定 `prototype/resources/ipa` 执行通过。两处 C++ 文件通过 clang-format 18.1.8 检查。
- 待签名包脚本检查 App 和扩展的 Mach-O 均为 arm64，平台为 iOS。恢复本地归档时逐文件校验远程 SHA-256，包含固定资料、字体许可和嵌入扩展。

Xcode 27.1 SDK 日志中的 `invalidDigitCount` 警告仍存在，实际构建和最终测试返回成功。测试结果以 `TEST SUCCEEDED`、进程结果与断言为准。

## 当前限制

手机与 Mac 使用固定出厂全拼词语索引，尚未迁移水杉的整句解码、简拼、模糊音、纠错与学习调频。当前固定北京音规则，自定义转换字表与规则切换尚未迁移。JSON/CIN 的物理热键元数据保留，Apple/Android 尚未执行自定义物理热键。App Group 权限、真实手机字体表现、横屏和不同手机尺寸仍需实机验证。

以上是可试验的原型，未作为商店正式版发布。Mac 的 `-50` 尚未归因于确定的系统缺陷。按用户要求保留当前 Mac 登录会话，后续再检查输入源会话缓存。

## 来源

代码沿用水杉 Windows v0.11.1 / `dd018f79858314ed542a47c528608752b13bd261`，CIN、多音字与调值规则复用同一 C++ 核心。出厂词库固定在 `MSIME-Engine` 的 `d0dc0c2b594b5540b5de99ad12085c786410626e`，SHA-256 在 `product-lock.json`。不包含个人学习库。

原始字体、CC-CEDICT、IPA/extIPA/VoQS、PhoneticToolbox 的来源和各自许可继续保留在 `prototype/THIRD_PARTY.md`、资源来源清单和应用资源中。平台接口参考 [Apple 自定义键盘](https://developer.apple.com/documentation/uikit/creating-a-custom-keyboard)、[UIInputView 自适应尺寸](https://developer.apple.com/documentation/uikit/uiinputview/allowsselfsizing) 与 [Android InputMethodService 指南](https://developer.android.com/develop/ui/views/touch-and-input/creating-input-method)。

## Android 编码标签补验

最终截图发现按键按钮的 elevation 遮住了小编码标签。现将标签置于按钮上方，复查截图可见反斜杠、感叹号、井号等键的编码。重新构建后，项目专用无窗口模拟器上的 JNI/core 与实际系统键盘 UI 测试全部通过，最终宿主文本为 厌恶wu˥˩a˨˩˦6˥/。新 APK 的 v2 签名验证通过。
