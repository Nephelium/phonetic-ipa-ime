# 国际音标输入法 0.1.3

默认输入拼音，按数字或空格选择候选时输入国际音标与声调。点击汉字行仍可直接输入汉字。多音字先按数字选字，再按数字选择读音，确认后输入音标。词语优先使用匹配的带调读音，输入 wu 时不会混入恶的 e 读音。

| 操作 | Windows | macOS | Android / iOS |
|---|---|---|---|
| 音标／汉字输出 | Caps Lock | Caps Lock 或候选栏按钮 | 候选栏输出按钮 |
| 拼音／CIN 编码 | 单按 Shift | 单按 Shift，也支持 Shift＋空格 | 底部 中／CIN |
| 五度声调 | 按住 Shift 按 1–5，连续输入 ˩˨˧˦˥ | 同左 | 上划数字 1–5，点选声调候选 |
| 转写规则 | 悬浮工具栏的规则名称，或设置 → 输入与转写 | 候选栏规则菜单 | 候选栏规则按钮 |
| 全部音标表 | 设置 → IPA Plus | 设置 → IPA Plus | 键盘顶部 IPA Plus |

CIN 模式按字母查看对应音标。手机短按有多个输出的键会弹出选择窗，点哪个输入哪个。键帽的 ··· 表示有多个输出或组合输入。单项输出直接输入。长按也能查看。g 键保留主要音标，左上角不再显示另一个小 g。

符号页包含反斜杠、斜杠等编码。反斜杠的首项是 ˥。数字 1–9 用来选候选，不放置音标，原先数字 1 的输出移到反斜杠。0 保留原方案。含数字的组合编码可用 code: 入口。

设置可调整每页 3–9 个候选，横向编辑候选并导入／导出 CIN、JSON。手机 IPA Plus 默认字号 50%，支持缩放、检索和上下滑动，辅音矩阵也可左右滑动。附加符号使用虚线圆圈和码位辅助辨认，实际输入不包含辅助圆圈。

十套转写分别保留来源表的符号和声调，不把各套差异统一改写。列表包括北京宽式、北京严式、胡裕树、黄廖、钱乃荣、吴宗济、赵元任、方音字汇、Unt 宽式和 Unt 严式。来源、原始列名和文件 SHA-256 保存在 Rules-SOURCES.json。无确定读音时要求选择，缺少转写时不伪造输出。

## 安装

- Windows 10 / 11 x64：完整解压 ZIP，双击 安装.cmd，按系统提示允许注册独立输入法。窗口保留结果，失败日志位于 install-logs。可先运行 scripts/Install-Windows.ps1 -CheckOnly 检查包内容。
- macOS 13+，Intel / Apple 芯片：完整解压，在终端进入目录后运行 bash 安装.command。更新已有本输入法用 bash 安装.command --update。只安装到当前用户的 ~/Library/Input Methods/PhoneticIPA.app，需要 Xcode 命令行工具。保留旧版本备份。应用使用本地签名，未进行 Developer ID 公证。
- Android 10+，arm64 / x86_64：安装 APK，在 App 试打页启用键盘并切换。当前包使用测试签名，更新时保留相同签名。不要卸载旧版来解决更新问题，以免丢失方案。
- iOS 16+：交付 unsigned.ipa，需自行签名 App 和键盘扩展后安装。安装后在系统键盘设置中添加。签名与 App Group 说明见 apple/INSTALL-unsigned.md。

## 更新

默认每天检查官网和 GitHub 的公开版本 JSON，任一来源有新版就提示下载。设置可手动检查或关闭。不会自动覆盖安装包或中断当前输入。iOS 只在配套 App 前台检查。检查请求不包含按键、宿主文本、词库或个人方案。

## 验证与来源

0.1.3 已通过 Windows 专用 Win32 编辑框实际按键、macOS 专用 NSTextView 跨进程输入、Android 模拟器真实系统键盘触摸，以及 iOS 模拟器真实键盘扩展 UI 测试。iOS 浅色和深色、手机横屏与大字号也有定向验证。实机 iPhone 签名和 App Group 共享、Android 各厂商实体设备尚未逐一验证。Windows 提权安装确认需用户操作，自动验证检查了完整包及缺失文件时的错误日志。

音标使用 Doulos SIL，扩展缺字使用 PTB IPA Plus。接收文字的软件决定上屏后的字体。GPLv3 源码、水杉原作者、CC-CEDICT、字体 OFL、IPA/extIPA/VoQS 等来源见 prototype/THIRD_PARTY.md 与上游 THIRD_PARTY_NOTICES.txt。

源码：https://github.com/Nephelium/phonetic-ipa-ime/tree/ipa-prototype
