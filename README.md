# 国际音标输入法 · Phonetic IPA IME

Windows 国际音标输入法开发原型：汉字下方显示 IPA，汉字与音标分别上屏，可视化编辑音标键盘，并处理词语读音与多音字选择。

本项目由 [Nephelium](https://github.com/Nephelium) 基于 [水杉输入法 msime-windows](https://github.com/metasequoiaime/msime-windows) 开发，固定基线为 **v0.11.1 / dd018f79858314ed542a47c528608752b13bd261**，改动日期 2026-10-23。保留上游历史、GPLv3 许可和第三方声明。本仓为独立衍生原型，问题反馈请提交到本仓。

- **候选与读音**：完整词语优先匹配带调读音，歧义由用户选择，输入 wu 时“恶”不会显示 e 的读音。
- **分开输入**：空格或数字选汉字，Ctrl+Enter 选音标，未确定读音时先打开选择页。
- **键盘编辑**：模拟键盘、放大的悬停预览、组合输入、横向候选编辑、CIN/JSON 导入导出。
- **音标工具**：IPA / extIPA / VoQS 分区表，Shift+1–5 输入上标调值，每页 3–9 个候选。
- **字体**：Doulos SIL 7.000，扩展字形回退 PTB IPA Plus。

![读音选择与 Doulos SIL](prototype/docs/previews/15-reading-choice.png)

[构建、试用与验证](prototype/README.md) · [来源、引用与许可](prototype/THIRD_PARTY.md) · [修改清单](MODIFICATIONS.md) · [水杉原始 README](README.upstream.md)

当前提交公开源码、固定资源和构建工具，尚未发布安装包。后台算法、原生控件及 Server 通信检查已通过，记事本/Word 等真实 TSF 宿主上屏仍待验证。截图来自实际原生控件的后台绘制。

代码遵循 [GPLv3](LICENSE)。CIN 码表由 Nephelium 自编；北京音字表和 IPA Plus 目录取自作者的 PhoneticToolbox v3。CC-CEDICT、字体及图表衍生数据分别保留其许可，完整作者、版本、改动说明、参考文献和 SHA-256 见上方来源清单。该声明仅涉及本仓收录部分，不改变 PhoneticToolbox 其他部分的许可。
