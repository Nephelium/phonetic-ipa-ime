当前使用说明、默认输出与验证范围见 [安装与使用](../docs/INSTALL.md)。

# 国际音标输入法 · Android 原型

Android 10 及以上的系统输入法与配套设置页，支持 arm64-v8a、x86_64。复用 `engine/ipa/` 的 IPA、五度调值和多音字核心，固定出厂 SQLite 词条索引与 Apple 端一致。

## 使用

安装本地测试 APK，打开 App，点“启用键盘”，再点“切换键盘”。首次启用默认拼音选音标，数字或空格选候选时上屏 IPA；候选栏可以切成汉字输出。底部 拼音／IPA 切换拼音与音标编码。有歧义时先选字，再按数字选读音并上屏。输入 `wu` 不会列出恶的 `e` 读音。

数字行保留 1–0。轻点 1–9 选择当前页候选，没有候选时输入数字。上划 1–5 输入五度调值，依次上划 2、1、4，再点候选输入 `˨˩˦`、`²¹⁴` 或 `214`。0 在 IPA 模式保留原映射。Shift、退格在 zxc 行两侧，底部“说明”打开帮助。带 ··· 的键短按打开音标选择，点选后输入一次。没有角标的单候选键直接输入。选择面板包含以该键开头的组合编码，关闭不输入。

设置页点击模拟键盘编辑候选，候选横向排列。IPA / extIPA / VoQS 使用分区矩阵，支持点选替换、追加。每页候选可设 3–9 个，支持系统文件选择器导入、导出 IPA 和版本 1 JSON。候选卡显示完整预览和 Unicode 码位，附加符号用圆圈辅助显示，输出仍保留原始字符。

键盘上方的 IPA Plus 可以连续点选 IPA、extIPA、VoQS。设置中的完整表格支持横向滑动，页面支持纵向滑动。手机默认字号 50%，可调 40%–130%，单元格随字号调整。主键盘采用固定键高及按实际字形居中的绘制方式。

当前使用完整词语拼音索引，尚未移植水杉的整句解码、简拼、模糊音和学习调频。已支持十套固定汉字转写规则快捷切换，自定义字表与物理热键执行尚未移植。物理热键元数据随方案保存。仅联网读取官网与 GitHub 的公开版本信息，不收集宿主输入。App 或键盘启动时每 24 小时自动检查，发现新版显示下载入口。设置页可关闭自动检查或手动检查。

## 构建

先按 `../apple/README.md` 准备 `apple/Generated/IPA`。只接受固定出厂词库，禁止传入个人学习库。JDK 17 或以上、Gradle 9.2.1、AGP 9.0.0、Android SDK 36。本次验证使用 JDK 24.0.2、NDK r30 LTS 30.0.16248370。

```sh
python scripts/build-native.py --ndk /path/to/android-ndk-r30 --source-root /path/to/repository
gradle --no-daemon :app:assembleDebug :app:assembleDebugAndroidTest :app:lintDebug
```

`local.properties` 仅写本机 `sdk.dir`，不提交。工具链可置于项目输出目录，无需修改全局环境。JNI 编译 arm64-v8a 和 x86_64，原型 APK 使用开发调试签名，本地交付不代表商店发布。

固定的 NDK Windows 下载来自 [Android 官方](https://developer.android.com/ndk/downloads)，r30 ZIP SHA-1 为 `9bf167a1985fa7d4a036186b78f702eab9179408`。构建脚本不自动下载和安装全局工具。

## 验证

`CoreTests` 在 Android 进程中运行打包的 JNI、Unicode、词语读音、候选会话及配置检查。`KeyboardChecks` 通过 Accessibility 和触摸操作真实 InputMethodService，在宿主文本框断言上屏结果，不调用会话接口代替实际输入。

`scripts/start-qa-emulator.py` 创建本项目单独的无窗口模拟器，需指定已有的 SDK、系统镜像、输出目录。`scripts/test-keyboard.py --adb /path/to/adb --output /path/to/qa` 只操作已记录的本项目设备，核对 AVD 名称，运行核心及真实键盘测试并拉取截图。不会接管其他正在运行的模拟器。

实际运行结果见根目录 `docs/testing/`。模拟器通过不等于实体 Android 设备验收。

## 来源

程序沿用主仓库 GPL-3.0，保留水杉与固定词库来源。字表、CC-CEDICT、Doulos SIL、PTB IPA Plus、IPA/extIPA/VoQS 的不同许可和引用见 `../prototype/THIRD_PARTY.md`。App 的来源说明也随固定资料打包。图标及提示词见 `../assets/icons/README.md`。
