当前 0.1.3 使用说明、默认输出与验证范围见 [安装与使用](../docs/INSTALL.md)。

# 国际音标输入法 · Apple 原型

此目录开发 macOS InputMethodKit 输入法，以及 iOS/iPadOS 系统键盘扩展和配套设置 App。

## 结构

- `Shared/IPAEngine.mm` 接入原有 `engine/ipa/` CIN、五度调值与多音字规则。
- `Shared/InputModel.swift` 统一候选、分页、读音选择和键位方案。
- `Shared/KeyboardViews.swift` 提供双行候选、模拟键盘、横向编辑和 IPA Plus 分类矩阵。
- `macOS/` 处理系统输入会话与光标附近的原生候选面板。
- `iOS/` 提供设置 App 和键盘扩展。

## 资源准备

使用主仓库 `prototype/resources/ipa` 的固定资料，以及 `product-lock.json` 中固定的出厂 `msime.db`。不得传入实际使用过的词库或个人学习库。

```sh
python3 scripts/prepare.py --resources /path/to/prototype/resources/ipa --factory-dictionary /path/to/factory/msime.db --output Generated/IPA
python3 scripts/project.py
bash scripts/build.sh
```

开发工作区中默认引擎路径为 `../msime-windows`。从公开仓库构建时设置 `IPA_SOURCE_ROOT` 为该仓库根目录。Xcode 通过 `DEVELOPER_DIR` 定位，构建脚本不修改系统默认工具路径。

原型查询由固定出厂词库生成的只读 SQLite 索引，沿用词条的原有权重，支持完整词语拼音及显式音节分隔。尚未接入水杉整句解码、简拼、模糊音、纠错和学习调频，不把索引查询称作完整引擎移植。多音字仍使用同一 C++ 核心，整词读音按需从 SQLite 查询。

## 输入方式

- 候选汉字与下方 IPA 可以分别点击上屏。
- 有多种读音时，数字选字进入读音选择，再按数字选择读音并上屏。显式重新打开读音预览时，仍可手动确认。
- Mac：默认数字／空格选 IPA，Caps Lock 或候选栏按钮切汉字输出。单按 Shift 或 Shift＋空格切拼音／CIN，Control＋回车提交 IPA，Control＋Shift＋回车重选读音。Shift＋1–5 连续输入五度调值。
- iOS：保留独立数字行。上划 1–5 输入五度调值，轻点 1–9 选择候选，无候选时输入数字。点“符号”切换标点。带 ··· 的键短按弹出候选，点选后输入，没有角标的单候选键直接输入。面板也包含首键相同的组合编码，关闭不输入。
- Shift、退格在 zxc 行两端。底部“说明”可随时打开使用方法，返回时保留输入。
- 设置中点击键位、编辑横向候选，再保存方案。支持 CIN 和本原型版本 1 JSON。未知 JSON 版本拒绝导入。

iOS 键盘上方的 IPA Plus 提供 IPA / extIPA / VoQS 分类浏览和连续点选。设置中的矩阵可以横向滑动，页面可以纵向滑动。iOS 默认字号 50%，可调 40%–130%，Mac 默认仍为 100%。编辑候选提供完整预览、Unicode 码位和实际输出输入框。

原型默认把独立的 1–9 音标映射迁至反斜杠键，0 保留。字符和组合附加符号保持原始 Unicode 序列。

Apple 端支持十套固定转写规则及候选栏快捷切换，默认北京宽式。自定义汉字字表尚未移植。JSON/CIN 中的物理组合热键元数据会保留，Apple 端尚未执行这些自定义热键。iOS 设置共享需要正确的 App Group 签名与键盘的完全访问权限，未完成实机验证。键盘不发送输入内容到网络。

## 构建、安装与验证

`build.sh` 先运行 Apple 桥接核心测试，再构建 Mac 应用和 iOS Simulator App/扩展。它不注册、激活或替换任何系统输入源。Mac 使用下述安装脚本放入用户 `~/Library/Input Methods/`，保留独立输入法标识。iOS 实机还需要配置开发者签名和 App Group。两端标识独立于水杉。

Mac 输入源选择问题已修复，专用 NSTextView 宿主验证了汉字、IPA、CIN、Shift 声调、标点和手动读音选择。详见 [Mac 验证记录](../docs/testing/2026-10-23-Mac输入源修复与真实输入验证.md)。安装需要 Xcode 命令行工具；首次安装执行：

```sh
bash scripts/install-macos.sh --repair-enabled-list
```

已有安装时增加 `--update`，旧应用保存在 `build/install-backups/`。`--repair-enabled-list` 会在需要时备份启用列表，仅补齐本输入法的父项与模式项。安装不选择当前输入源，也不注销。随后从系统输入法菜单选择国际音标输入法。

真实宿主测试使用 `bash scripts/test-macos-host.sh --foreground`，会短暂激活专用测试窗口，结束后恢复原应用与输入源。没有前台操作许可时不要使用该参数。

目前交付状态以 `../docs/testing/` 的最新验证记录为准。编译通过、核心测试、模拟器键盘和真实 Mac 宿主上屏分别记录。

图标已随源码提供。需要从原始图重新生成尺寸时，在 Mac 运行 `python3 scripts/prepare-icons.py`，只对 `../assets/icons/` 中的原图进行尺寸转换。

真机待签名包：`bash scripts/package-unsigned.sh`。它生成 `generic/platform=iOS` Release archive，核验 App 与嵌入扩展均为 arm64 / iOS，再包装 Payload。它不会申请证书或上传发布渠道。安装见 [待签名包说明](INSTALL-unsigned.md)。

iOS 系统键盘 UI 测试使用 `PhoneticIPA-iOS` scheme，指定自己创建的模拟器 UDID。`Tests/KeyboardUITests.swift` 从系统设置启用键盘，通过真实按钮和滑动验证汉字、音标、消歧、CIN、声调和帮助页。`scripts/snapshots.sh` 同时执行 Swift 输入会话检查并生成后台原生预览。

## 资料与许可

许可沿用主仓库 GPL-3.0。字表、字体、音标目录的原始许可与来源见 `prototype/THIRD_PARTY.md` 和 `prototype/resources/provenance.json`。准备脚本校验资源 SHA-256 并附带各字体、CC-CEDICT 和音标资料许可。拼音词库来自 `metasequoiaime/MSIME-Engine` 的固定词库提交 `d0dc0c2b594b5540b5de99ad12085c786410626e`，其原有来源声明一同保留。

`Generated/` 不提交大型数据库；提供可复现生成脚本与输入哈希。本地备份应同时包含固定资料及生成清单，Mac 只保留编译副本。
