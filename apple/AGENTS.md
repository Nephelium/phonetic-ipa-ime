# Apple 输入法

- `Shared/`：共享 C++ 桥接、Swift 输入会话、资源和设置模型。
- `macOS/`：InputMethodKit 宿主及原生候选面板。
- `iOS/`：配套设置 App 与系统键盘扩展。
- `scripts/`：资源准备、工程生成、构建和后台验证。
- `Generated/`、`build/`：生成资源与构建产物，不提交。
- 复用 `engine/ipa/` 核心，Apple 平台层不得重新实现多音字规则。
- 原始词库只读，生成索引只使用固定出厂词库，禁止读取用户学习词库。
- 两端使用独立 bundle ID，普通构建不安装、不激活系统输入源。
- 编译、模拟器、真实宿主和实机输入分别记证据。许可证及来源随资源打包。
- 使用 `scripts/prepare.py` 准备资源，`scripts/project.py` 生成 Xcode 工程。构建与验证命令见 README。
