# 国际音标输入法 · Phonetic IPA IME

## 0.1.1 测试版

[四平台安装与使用](docs/INSTALL.md) · [GitHub 下载](https://github.com/Nephelium/phonetic-ipa-ime/releases/tag/v0.1.1) · [官网下载](https://www.phonetictoolbox.com/#ipa-ime-download)

Windows、macOS、iOS 和 Android 已加入官网与 GitHub 双来源更新检测。默认每天自动检查，也可在设置页手动检查或关闭。iOS 只在配套 App 前台检查，键盘扩展不联网。下载后的升级由用户确认并安装。

Windows 请使用 r2 修复包，已下载初版可用约 4 KB 安装补丁覆盖。Windows ZIP 含注册脚本，macOS ZIP 需要命令行工具，iOS 为待签名 IPA，Android 为测试签名 APK。Mac 专用宿主、iOS/Android 模拟器真实键盘和 Windows 后台控件检查分别通过，实体手机与更多桌面宿主仍待验证。见[本次验证记录](docs/testing/2026-10-23-四平台更新与官网发布.md)。

<img src="assets/icons/ipa-bird-concept-v1.png" alt="抱着音标键帽的小鸟" width="128">

汉字候选下方显示国际音标，汉字与 IPA 分别输入，支持词语读音、手动多音字选择与可编辑 CIN 音标键盘。现有 Windows 原型，并新增 macOS、iOS/iPadOS 和 Android 平台实现。

本项目由 [Nephelium](https://github.com/Nephelium) 基于 [水杉输入法 msime-windows](https://github.com/metasequoiaime/msime-windows) 开发，固定基线为 **v0.11.1 / dd018f79858314ed542a47c528608752b13bd261**，改动日期 2026-10-23。保留上游历史、GPLv3 许可和第三方声明。本仓为独立衍生原型，问题反馈请提交到本仓。

## 各平台状态

| 平台 | 当前状态 | 构建与试用 |
|---|---|---|
| Windows | TSF / Server / 设置程序已构建，后台控件、IPC、读音检查通过。真实 TSF 宿主仍待验收。 | [Windows 说明](prototype/README.md) |
| iOS / iPadOS | App 与系统键盘扩展。模拟器真实上屏通过，已生成真机待签名 IPA。实体 iPhone 签名、安装与设置共享待验收。 | [Apple 构建](apple/README.md) · [签名与安装](apple/INSTALL-unsigned.md) |
| Android | 系统输入法和设置 App。打包 JNI 与无窗口模拟器真实上屏通过，提供本地测试 APK。实体手机待验收。 | [Android 说明](android/README.md) |
| macOS | 输入源选择已修复。专用 NSTextView 真实宿主通过汉字、IPA、CIN、Shift 声调、标点和手动读音上屏。 | [Apple 构建](apple/README.md) |

[Mac 输入源与真实宿主验证](docs/testing/2026-10-23-Mac输入源修复与真实输入验证.md) · [本次跨平台验证](docs/testing/2026-10-23-Apple与Android原型验证.md) · [来源、引用与许可](prototype/THIRD_PARTY.md) · [修改清单](MODIFICATIONS.md) · [水杉原始 README](README.upstream.md)

## 输入与编辑

- 完整词语优先匹配带调读音，歧义由用户选择。输入 `wu` 时“恶”不会混入 `e` 的读音，选好读音后再提交 IPA。
- 点候选汉字或下方音标分别输入。每页候选可设 3–9 个。
- 模拟键盘、首键与组合编码预览、横向候选编辑、CIN/JSON 导入导出。
- IPA / extIPA / VoQS 分区表，Doulos SIL 7.000 主字体，扩展字形回退 PTB IPA Plus。
- 手机保留 1–0 数字行，上划 1–5 输入五度调值，Shift/退格位于 zxc 行两端。键盘“说明”可随时查看操作方法。
- 手机 IPA 模式点“符号”可输入 `/`、`\` 等编码。原数字键音标移至 `\`，0 保留原映射。

Apple/Android 当前使用固定全拼词语索引与北京音规则，尚未移植整句解码、简拼、模糊音、学习调频、自定义汉字转换字表和物理热键执行。平台验证分别记录，不用模拟器通过替代实体设备验收。

本仓公开源码、固定资料、来源记录和复现脚本，未发布商店版本。大型词库索引由固定出厂资料生成，个人学习库、用户配置、签名凭据和工具链缓存不进入仓库。

代码遵循 [GPLv3](LICENSE)。CIN 码表由 Nephelium 自编；北京音字表和 IPA Plus 目录取自作者的 PhoneticToolbox v3。CC-CEDICT、字体及图表衍生数据分别保留其许可，完整作者、版本、修改说明、参考文献和 SHA-256 见来源清单。该声明仅涉及本仓收录部分，不改变 PhoneticToolbox 其他部分的许可。新图标的原图与生成提示词见 [图标记录](assets/icons/README.md)。
