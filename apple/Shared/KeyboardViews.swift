import SwiftUI
import UniformTypeIdentifiers
import CoreText

extension Font {
    static func ipa(_ size: CGFloat) -> Font {
        let fallback = CTFontDescriptorCreateWithNameAndSize("DoulosSIL" as CFString, size)
        let descriptor = CTFontDescriptorCreateWithAttributes([
            kCTFontNameAttribute: "PTBIPAPlus-Regular", kCTFontCascadeListAttribute: [fallback]
        ] as CFDictionary)
        return Font(CTFontCreateWithFontDescriptor(descriptor, size, nil))
    }
}

enum IPAVisuals {
    static func display(_ value: String) -> String {
        if value == " " { return "␠" }
        guard let first = value.unicodeScalars.first else { return "" }
        switch first.properties.generalCategory {
        case .nonspacingMark, .spacingMark, .enclosingMark: return "◌" + value
        default: return value
        }
    }
    static func codePoints(_ value: String) -> String { value.unicodeScalars.map { String(format: "U+%04X", $0.value) }.joined(separator: " ") }
}

struct IPAFile: FileDocument {
    static var readableContentTypes: [UTType] { [.json, .plainText] }
    var data: Data
    init(data: Data = Data()) { self.data = data }
    init(configuration: ReadConfiguration) throws { data = configuration.file.regularFileContents ?? Data() }
    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper { FileWrapper(regularFileWithContents: data) }
}

private enum WebGuide {
    static var url: URL {
        #if os(iOS)
        URL(string: "https://www.phonetictoolbox.com/ipa-ime-guide/?platform=ios")!
        #else
        URL(string: "https://www.phonetictoolbox.com/ipa-ime-guide/?platform=macos")!
        #endif
    }
}

enum FirstUseContent {
    static let titles = ["先认清当前输入状态",
"用拼音输入音标或汉字",
"切换到直接音标键盘",
"输入声调，查找更多符号"]
    static var details: [String] {
        #if os(iOS)
        ["① 启用并切换到国际音标输入法。Android 在试打页点“启用键盘”后点“切换键盘”；iPhone/iPad 在系统设置→通用→键盘中添加，输入时长按地球键选择。\n\n② 看左下角：显示“拼音”表示当前在拼音查字，点一下变成“IPA”，进入直接音标键盘；再点一次返回。按钮显示的是当前模式。\n\n③ 拼音模式下，上方“音标／汉字”是另一个开关。显示“音标”时，数字／空格默认选 IPA；点成“汉字”后默认选汉字。空闲时左侧会显示“拼音 → 音标”或“拼音 → 汉字”。新安装默认是拼音查字、输出音标，已有偏好会保留。",
"① 先让左下角显示“拼音”，上方输出按钮显示“音标”。依次输入 hao，候选上行显示汉字，下行显示对应音标。\n\n② 选择“好”。可点下行音标，或按“好”前面的数字。出现 hao3、hao4 时，再按读音前的数字，或直接点所需读音。选 hao4 会输入 xau˥˩。候选编号以当前页为准。单一读音直接输入，有多个读音才再选一次。\n\n③ 想输入汉字：把上方按钮切成“汉字”，再输入 hao 并选“好”，结果是汉字“好”。无论默认输出是什么，点候选上行汉字都能输入汉字，点下行音标都能输入 IPA。\n\n④ 可先试完整词语 yanwu，再点“厌恶”下方音标，输入 jan˥˩ wu˥˩。候选确认后才上屏，空格先确认候选，再按一次才输入空格。",
"① 点左下角的“拼音”，直到它显示“IPA”。此时按键用于选择音标，输入普通 a、g 等键就能查找相应符号。回到拼音查字时，再点左下角，直到显示“拼音”。\n\n② 带 ··· 的键有多个音标。短按一次即打开选择面板，再点想要的音标，例如点 g 键后选择 ɠ。打开或关闭面板本身不会输入字母。没有 ··· 的单项键直接输入。\n\n③ 也可用上方 IPA Plus：打开完整 IPA／extIPA／VoQS 音标表，搜索或滑动后点符号，可以连续输入。先确认已有候选，再打开音标表。\n\n④ 点“符号”查看 /、\\ 等标点键。\\ 的候选收录声调符号，/ 默认输入斜杠。按住退格可连续删除，松手停止。",
"① 先确认正在输入的汉字或音标，再开始加声调。数字 1 最低、5 最高。依次在 2、1、4 三个数字键上向上划动，上方出现调值候选。\n\n② 看到 tone:214 时，表示调值还在组合。点选 ˨˩˦、²¹⁴ 或 214 中想要的一项，也可点空格确认首项。确认后声调才上屏。输入声调时轻点 1–5 可继续追加调值。\n\n③ 普通候选出现时，轻点 1–9 选当前页候选；无候选时，普通数字键输入数字。IPA 模式的 0 保留键位方案中的映射。\n\n④ “规则”按钮切换 Standard Chinese Beijing、UntPhesoca 等十套转写方案。规则决定拼音查字的转写结果，直接音标键盘按自己的键位方案输入。设置中的“说明”可随时重看教学。"]
        #else
        ["① 双击“安装国际音标输入法.app”，点击安装或升级。完成后从菜单栏输入法菜单选择国际音标输入法，再点进要打字的输入框。无需命令行工具。\n\n② 候选栏空闲时显示状态：“拼音 → 音标”表示拼音候选默认输入 IPA；“拼音 → 汉字”表示默认输入汉字；“IPA · 点选音标”表示直接音标键盘。\n\n③ 轻按并松开 Shift，切换拼音与直接音标；Shift＋空格也能切换。拼音模式下，Caps Lock 切换 IPA／汉字输出，也可点候选栏“音标／汉字”按钮。新安装默认拼音查字、输出音标，已有偏好会保留。",
"① 回到“拼音 → 音标”状态，依次输入 hao。候选上行显示汉字，下行显示 IPA。\n\n② 找到“好”，按它前面的数字，或点下行音标。出现 hao3、hao4 时，再按读音前的数字。选 hao4 输入 xau˥˩。单一读音直接输入，有多个读音才再选一次，编号以当前页为准。\n\n③ 想输入汉字：在拼音模式按 Caps Lock，或把候选栏按钮切为“汉字”，再输入 hao 并选“好”，结果就是汉字“好”。点上行汉字或下行音标也能分别输入。\n\n④ 空格确认高亮候选，上下键移动，Page Up／Page Down 翻页，Esc 取消组合，Control＋Shift＋回车重新选择读音。完整词语 yanwu 可直接使用厌恶的词语读音。",
"① 单按并松开 Shift，直到候选栏显示“IPA · 点选音标”。此时字母或符号用于查找键位音标。按 a，再按 1 或空格，确认首项 a。\n\n② 看到候选和正在组合的编码时，表示仍需选择。按候选前的数字确认后，音标才上屏。各键备选可在设置的音标键盘中查看与修改。\n\n③ 再单按 Shift 返回拼音查字，然后用 Caps Lock 或“音标／汉字”按钮决定默认输出。单按 Shift 和按住 Shift＋数字的作用不同。\n\n④ 直接音标模式中也能按 /、\\ 等符号键。\\ 收录声调符号，/ 默认输出斜杠。Esc 取消未确认的编码。",
"① 先确认当前汉字或音标，再按住 Shift，依次按 2、1、4，随后松开 Shift。数字 1 最低、5 最高。\n\n② 看到 tone:214 时，调值还在组合。空格选首项 ˨˩˦，也可用数字选择 ²¹⁴ 或 214；再按一次空格才输入空格。\n\n③ 候选栏的规则菜单切换十套拼音转写方案。设置中的 IPA Plus 提供 IPA／extIPA／VoQS 音标表，可用于编辑键位。\n\n④ 设置→使用说明可重看教学。若切换后仍看不到音标候选，先确认已选择国际音标输入法，再查看候选栏的模式名称；在直接音标模式按 a、1 即可先试一个音标。"]
        #endif
    }
}

struct FirstUseGuide: View {
    var finish: () -> Void
    @State private var page = 0
    private var titles: [String] { FirstUseContent.titles }
    private var details: [String] { FirstUseContent.details }
    private var examples: [String] {
        #if os(iOS)
        ["左下角：拼音 ↔ IPA\n上方：音标 ↔ 汉字", "hao → 选好 → 选 hao4\nxau˥˩", "ɡ ··· → 选择 ɠ\nIPA Plus → 全部符号", "2 ↑   1 ↑   4 ↑\n˨˩˦   ²¹⁴   214"]
        #else
        ["单按 Shift：拼音 ↔ IPA\nCaps Lock：音标 ↔ 汉字", "hao → 选好 → 选 hao4\nxau˥˩", "单按 Shift → IPA\na → 1 → a", "按住 Shift + 2、1、4\n空格 → ˨˩˦"]
        #endif
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack { Text("国际音标输入法").font(.headline); Spacer(); Button("稍后") { finish() }.accessibilityIdentifier("onboarding-skip") }
            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    Link("打开完整图文教程（网页）", destination: WebGuide.url).font(.subheadline)
                    Text(titles[page]).font(.title2.bold()).fixedSize(horizontal: false, vertical: true)
                    Text(examples[page]).font(.ipa(28)).multilineTextAlignment(.center).frame(maxWidth: .infinity).padding(24)
                        .background(Color.accentColor.opacity(0.09), in: RoundedRectangle(cornerRadius: 16))
                    Text(details[page]).font(.body).fixedSize(horizontal: false, vertical: true)
                }.padding(.vertical, 8)
            }.id(page).accessibilityIdentifier("onboarding-content")
            HStack {
                if page > 0 { Button("上一步") { page -= 1 } }
                Spacer(); Text("\(page + 1) / 4").font(.caption).foregroundStyle(.secondary); Spacer()
                Button(page == 3 ? "开始使用" : "下一步") { if page < 3 { page += 1 } else { finish() } }
                    .buttonStyle(.borderedProminent).accessibilityIdentifier("onboarding-next")
            }
        }.padding(24).frame(minWidth: 280, idealWidth: 540, minHeight: 340, idealHeight: 550)
    }
}

struct UsageGuideView: View {
    var compact = false
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: compact ? 14 : 22) {
                if !compact {
                    Link("打开完整图文教程（网页）", destination: WebGuide.url)
                        .accessibilityIdentifier("web-usage-guide")
                }
                #if os(iOS)
                if !compact {
                    section("开始使用", "在系统设置 → 通用 → 键盘 → 键盘 → 添加新键盘中，添加国际音标输入法。输入时长按地球键切换过来。")
                }
                #endif
                ForEach(0..<FirstUseContent.titles.count, id: \.self) { index in
                    section(FirstUseContent.titles[index], FirstUseContent.details[index])
                }
                section("汉字与音标分别上屏", "在汉字模式输入拼音。点击候选上面的汉字输入汉字，点击下面的音标输入 IPA。默认数字键和空格选音标，键盘上方可切换为汉字输出。用模式按钮切换拼音与直接音标输入。")
                section("多音字怎么选", "先按词语和输入拼音筛选读音。有多个带调读音时，点击音标会打开读音选择。选定读音后直接上屏。长按候选可重新选择读音。")
                #if os(iOS)
                section("数字与五度调值", "轻点数字 1–9 选择当前页的候选，没有候选时输入数字。上划数字 1–5 输入五度调值，1 最低、5 最高。依次上划 2、1、4，再点上方候选，即可输入 ˨˩˦、²¹⁴ 或 214。")
                section("先确认当前候选", "输入声调前，先让当前汉字或音标上屏。声调输入期间轻点 1–5 可继续追加调值，点空格确认。0 在 IPA 模式保留原有映射。")
                section("斜杠等符号键", "先切到 IPA，再点符号。点编码为 / 或 \\ 的键，在弹出的面板中点选输出。\\ 收录原数字键上的声调音标。默认 / 输出斜杠，可以在设置中修改。面板可上下滑动，关闭时不输入。键盘上方的 IPA Plus 可打开分类音标表并连续点选输入；先确认已有的拼音或声调候选。")
                #else
                section("数字与五度调值", "1–9 选择当前页的候选。按住 Shift，依次按 2、1、4，可得到 ˨˩˦、²¹⁴ 或 214，空格确认。0 保留原有映射。")
                section("Mac 快捷键", "单按 Shift 或 Shift＋空格切换拼音／IPA。Caps Lock 切换音标／汉字输出。Control＋回车输入 IPA，Control＋Shift＋回车重新选择读音。Page Up / Page Down 翻页。")
                section("斜杠等符号键", "IPA 模式直接按实体键盘的 / 或 \\，再选择候选。\\ 收录原数字键上的声调音标。默认 / 输出斜杠，可以在设置中修改。")
                #endif
                #if os(iOS)
                section("音标和组合编码", "手机首次启用使用拼音输入音标。带 ··· 的键轻点后弹出音标，点哪个就输入哪个；没有角标的单候选键直接输入。带有组合输入标记的分区收录以此键开头的编码，例如 v 下的 voqs。打开或关闭面板不会输入字母。")
                #else
                section("音标和组合编码", "IPA 模式按编码输入音标，例如 a。悬停键位可查看这个键和以它开头的组合编码，例如 v 下的 voqs。含数字的编码可用 code: 前缀输入。")
                #endif
                section("音标与字母的字形", "g 键中央的 ɡ 是国际音标字符 U+0261，字形与普通拉丁字母 g 可能不同。")
                section("修改键位", "打开设置，点击模拟键盘上的按键。候选从左到右排列，可直接编辑，也可用 IPA Plus 点选符号。保存方案后生效。每页候选可选 3–9 个，支持导入和导出 IPA / JSON。")
            }.frame(maxWidth: .infinity, alignment: .leading).padding(compact ? 12 : 22)
        }.accessibilityIdentifier("usage-guide")
    }
    private func section(_ title: String, _ text: String) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title).font(compact ? .subheadline.bold() : .headline)
            Text(text).font(compact ? .caption : .body).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
}

private struct CandidateRowHeightKey: PreferenceKey {
    static var defaultValue: CGFloat = 0
    static func reduce(value: inout CGFloat, nextValue: () -> CGFloat) { value = max(value, nextValue()) }
}

private struct CandidateScrollEdges: ViewModifier {
    let enabled: Bool
    @ViewBuilder func body(content: Content) -> some View {
        #if os(iOS)
        if #available(iOS 26.0, *), enabled { content.scrollEdgeEffectHidden() }
        else { content }
        #else
        content
        #endif
    }
}

struct CandidateStrip: View {
    @ObservedObject var model: InputModel
    var openPlus: (() -> Void)? = nil
    var openRules: (() -> Void)? = nil
    var fittingRows = false
    @State private var candidateRowHeight: CGFloat = 80
    var body: some View {
        VStack(alignment: .leading, spacing: fittingRows ? 3 : 6) {
            HStack {
                Text(model.raw.isEmpty ? (model.pinyin ? (model.outputIpa ? "拼音 → 音标" : "拼音 → 汉字") : "IPA · 点选音标") : model.raw)
                    .font(.system(.body, design: .monospaced)).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
                    .accessibilityIdentifier("composition-code")
                Spacer()
                if model.pinyin {
                    Button(model.outputIpa ? "音标" : "汉字") { model.outputIpa.toggle(); model.onChange() }
                        .font(.caption).accessibilityIdentifier("output-mode")
                }
                if model.pinyin, let openRules {
                    Button("规则", action: openRules).font(.caption).accessibilityIdentifier("transcription-rules")
                } else if model.pinyin {
                    Menu("规则") {
                        ForEach(model.rules) { rule in
                            Button { model.selectRule(rule.id) } label: {
                                if rule.id == model.ruleId { Label(rule.name, systemImage: "checkmark") }
                                else { Text(rule.name) }
                            }.accessibilityIdentifier("rule-" + rule.id)
                        }
                    }.font(.caption).accessibilityIdentifier("transcription-rules")
                }
                if let openPlus { Button("IPA Plus", action: openPlus).font(.caption).accessibilityIdentifier("keyboard-plus") }
                if !model.candidates.isEmpty {
                    Button("‹") { model.turnPage(-1) }.disabled(model.page == 0)
                    Text("\(model.page + 1)").font(.caption)
                    Button("›") { model.turnPage(1) }.disabled((model.page + 1) * model.pageSize >= model.candidates.count)
                }
            }
            if model.pending != nil {
                HStack { Text("选择带调读音").font(fittingRows ? .system(size: 10, weight: .medium) : .caption.bold()); Spacer(); Button("返回") { model.pending = nil; model.onChange() } }
                ScrollView(.horizontal) {
                    HStack(spacing: 8) {
                        ForEach(Array(model.readingOptions.enumerated()), id: \.offset) { index, option in
                            Button { model.chooseReading(index) } label: {
                                VStack(spacing: fittingRows ? 2 : 8) { Text("\(index + 1)  \(option.pinyin)").font(fittingRows ? .system(size: 10) : .caption).lineLimit(1); Text(option.ipa).font(.ipa(fittingRows ? 18 : 25)).lineLimit(1) }
                                    .padding(fittingRows ? 7 : 9).background(Color.accentColor.opacity(0.10), in: RoundedRectangle(cornerRadius: 12))
                            }.buttonStyle(.plain).accessibilityIdentifier("reading-" + option.pinyin)
                        }
                    }
                    .padding(.vertical, fittingRows ? 4 : 0)
                    .fixedSize(horizontal: false, vertical: true)
                    .background(GeometryReader { proxy in Color.clear.preference(key: CandidateRowHeightKey.self, value: proxy.size.height) })
                }
                .frame(height: fittingRows ? candidateRowHeight : nil)
                .modifier(CandidateScrollEdges(enabled: fittingRows))
            } else if !model.candidates.isEmpty {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(alignment: .top, spacing: 6) {
                        ForEach(Array(model.visible.enumerated()), id: \.element.id) { index, candidate in
                            VStack(spacing: fittingRows ? 0 : 2) {
                                Button { model.choose(index, ipa: false) } label: {
                                    HStack(spacing: 5) {
                                        Text("\(index + 1)").font(.caption2).foregroundStyle(.secondary)
                                        Text(candidate.word).font(candidate.symbol ? .ipa(fittingRows ? 23 : 29) : .system(size: fittingRows ? 17 : 20)).lineLimit(1)
                                    }.padding(.horizontal, 9).padding(.vertical, fittingRows ? 3 : 5)
                                }
                                .accessibilityIdentifier("hanzi-" + candidate.word + "-" + candidate.canonical)
                                if !candidate.symbol {
                                    Button { model.choose(index, ipa: true) } label: {
                                        Text(candidate.ipa.isEmpty ? (candidate.reading?.ambiguous == true ? "选择读音…" : "暂无音标") : candidate.ipa)
                                            .font(.ipa(fittingRows ? 18 : 23)).foregroundStyle(Color.accentColor).lineLimit(1)
                                            .padding(.horizontal, 9).padding(.bottom, fittingRows ? 3 : 7)
                                    }.disabled(candidate.ipa.isEmpty && candidate.reading?.ambiguous != true)
                                        .accessibilityIdentifier("ipa-" + candidate.word + "-" + candidate.canonical)
                                }
                            }
                            .buttonStyle(.plain)
                            .background(Color.accentColor.opacity(index == model.selectedIndex ? 0.14 : 0.05), in: RoundedRectangle(cornerRadius: 11))
                            .contextMenu { Button("切换读音") { model.reopenReading(index) } }
                        }
                    }
                    .padding(.vertical, fittingRows ? 4 : 0)
                    .fixedSize(horizontal: false, vertical: true)
                    .background(GeometryReader { proxy in Color.clear.preference(key: CandidateRowHeightKey.self, value: proxy.size.height) })
                }
                .frame(height: fittingRows ? candidateRowHeight : nil)
                .modifier(CandidateScrollEdges(enabled: fittingRows))
                .accessibilityIdentifier("candidate-scroll")
            }
            if !model.status.isEmpty { Text(model.status).font(.caption2).foregroundStyle(.secondary).lineLimit(2) }
        }
        .padding(fittingRows ? 8 : 10)
        .onPreferenceChange(CandidateRowHeightKey.self) { height in
            guard fittingRows, height.isFinite, height > 0, abs(candidateRowHeight - height) > 0.5 else { return }
            candidateRowHeight = ceil(height)
        }
    }
}

struct KeyPreview: View {
    let key: String
    let mappings: [String: [String]]
    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            Text("\(key) · 按键输出").font(.headline)
            Text(mappings[key, default: []].joined(separator: "  ")).font(.ipa(32))
            ForEach(mappings.keys.filter { $0.hasPrefix(key) && $0 != key }.sorted().prefix(16), id: \.self) { code in
                HStack(alignment: .firstTextBaseline) {
                    Text("\(code) · 组合输入").font(.caption).foregroundStyle(.secondary)
                    Text(mappings[code, default: []].joined(separator: "  ")).font(.ipa(24))
                }
            }
        }
        .padding(16).frame(maxWidth: 620, alignment: .leading)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 18))
        .overlay(RoundedRectangle(cornerRadius: 18).stroke(Color.accentColor.opacity(0.4)))
        .shadow(radius: 18, y: 5).allowsHitTesting(false)
    }
}

struct KeyboardMap: View {
    let mappings: [String: [String]]
    var edit: (String) -> Void
    @State private var hovered: String?
    private let rows = ["`1234567890-=", "qwertyuiop[]\\", "asdfghjkl;'", "zxcvbnm,./"]
    var body: some View {
        VStack(spacing: 6) {
            ForEach(rows, id: \.self) { row in
                HStack(spacing: 5) {
                    ForEach(Array(row).map(String.init), id: \.self) { key in
                        Button { edit(key) } label: {
                            VStack(spacing: 2) {
                                Text(key).font(.caption).foregroundStyle(.secondary)
                                Text(mappings[key]?.first ?? "")
                                    .font(.ipa(26)).frame(height: 32)
                            }
                            .frame(maxWidth: .infinity).padding(.vertical, 5)
                            .background(Color.accentColor.opacity(hovered == key ? 0.20 : 0.07), in: RoundedRectangle(cornerRadius: 9))
                        }
                        .buttonStyle(.plain)
                        .onHover { inside in withAnimation(.easeOut(duration: 0.13)) { hovered = inside ? key : (hovered == key ? nil : hovered) } }
                    }
                }
            }
        }
        .overlay(alignment: .bottom) {
            if let hovered { KeyPreview(key: hovered, mappings: mappings).offset(y: -55).transition(.scale(scale: 0.96).combined(with: .opacity)).allowsHitTesting(false) }
        }
    }
}

struct PlusEntry: Decodable, Identifiable {
    var id: String
    var system: String
    var section: String
    var display: String
    var insertText: String
    var nameZh: String
}
struct PlusCell: Decodable { var ids: [String] }
struct PlusRow: Decodable { var label: String; var cells: [PlusCell] }
struct VowelPoint: Decodable { var ids: [String]; var x: Double; var y: Double }
struct PlusChart: Decodable, Identifiable {
    var id: String; var title: String; var kind: String
    var columns: [String]?
    var rows: [PlusRow]?
    var points: [VowelPoint]?
}
struct PlusCatalog: Decodable {
    var entries: [PlusEntry]
    var charts: [String: [PlusChart]]
    static func load() -> PlusCatalog {
        guard let data = try? Data(contentsOf: IPAResources.directory.appendingPathComponent("plus-catalog.json")),
              let value = try? JSONDecoder().decode(Self.self, from: data) else { return Self(entries: [], charts: [:]) }
        return value
    }
}

struct PlusPicker: View {
    let select: (String) -> Void
    var compact = false
    var close: (() -> Void)? = nil
    @Environment(\.dismiss) private var dismiss
    @State private var system = "ipa"
    @State private var search = ""
    @State private var zoom: CGFloat = 1
    private let catalog = PlusCatalog.load()
    init(initialSystem: String = "ipa", compact: Bool = false, close: (() -> Void)? = nil, select: @escaping (String) -> Void) {
        self.select = select; self.compact = compact; self.close = close
        _system = State(initialValue: initialSystem)
        #if os(iOS)
        _zoom = State(initialValue: 0.5)
        #endif
    }
    var body: some View {
        Group {
            if compact { panelContent }
            else {
                NavigationStack {
                    panelContent.navigationTitle("IPA Plus")
                        .toolbar { Button("完成") { dismiss() }.accessibilityIdentifier("plus-close") }
                        #if os(iOS)
                        .navigationBarTitleDisplayMode(.inline)
                        #endif
                }
            }
        }
        #if os(macOS)
        .frame(minWidth: 420, idealWidth: 1050, minHeight: 420, idealHeight: 680)
        #else
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        #endif
    }
    private var panelContent: some View {
            VStack(spacing: 5) {
                if compact {
                    HStack { Text("IPA Plus").font(.headline); Spacer(); Button("返回键盘") { close?() }.accessibilityIdentifier("plus-close") }
                }
                Picker("音标表", selection: $system) { Text("IPA").tag("ipa"); Text("extIPA").tag("extipa"); Text("VoQS").tag("voqs") }.pickerStyle(.segmented)
                if !compact { TextField("搜索符号或中文名称", text: $search).textFieldStyle(.roundedBorder).accessibilityIdentifier("plus-search") }
                if !compact { HStack {
                    Button("字号 −") { zoom = max(0.2, zoom - 0.1) }
                    Text("\(Int((zoom * 100).rounded()))%").font(.caption).monospacedDigit()
                    Button("字号 ＋") { zoom = min(1.3, zoom + 0.1) }
                    Spacer()
                }.buttonStyle(.bordered) }
                Text(compact ? "上下滑动分类 · 点选直接输入" : "表格可左右滑动 · 页面可上下滑动").font(.caption).foregroundStyle(.secondary).frame(maxWidth: .infinity, alignment: .leading)
                ScrollView(.vertical) {
                    VStack(alignment: .leading, spacing: 8) {
                        if search.isEmpty && !compact {
                            ForEach(catalog.charts[system, default: []].filter { $0.kind == "matrix" }) { chart in
                                Text(chart.title).font(.headline)
                                matrix(chart)
                            }
                            ForEach(catalog.charts[system, default: []].filter { $0.kind == "vowels" }) { chart in
                                Text(chart.title).font(.headline)
                                vowels(chart)
                            }
                        }
                        ForEach(sections, id: \.self) { section in
                            Text(sectionTitle(section)).font(.headline)
                            LazyVGrid(columns: [GridItem(.adaptive(minimum: max(30, 48 * zoom), maximum: max(40, 90 * zoom)))], spacing: 3) {
                                ForEach(entries.filter { $0.section == section }) { entry in
                                    symbol(entry).frame(maxWidth: .infinity).padding(.vertical, 3)
                                        .background(Color.secondary.opacity(0.07), in: RoundedRectangle(cornerRadius: 7))
                                }
                            }
                        }
                    }.frame(maxWidth: .infinity, alignment: .leading).padding(.bottom, 12)
                }.accessibilityIdentifier("plus-page")
            }.font(.system(size: 13)).padding(compact ? 6 : 8)
    }
    private var unit: CGFloat { 36 * zoom }
    private var symbolHeight: CGFloat { max(24, 48 * zoom) }
    private var labelWidth: CGFloat {
        #if os(iOS)
        return 56
        #else
        return 78
        #endif
    }
    private var headerHeight: CGFloat { 24 }
    private func rowHeight(_ row: PlusRow) -> CGFloat {
        CGFloat(max(1, row.cells.map { ($0.ids.count + 1) / 2 }.max() ?? 1)) * symbolHeight + 4
    }
    private func matrix(_ chart: PlusChart) -> some View {
        let rows = chart.rows ?? []
        let height = rows.reduce(headerHeight) { $0 + rowHeight($1) + 1 }
        return ScrollView(.horizontal) {
            Grid(horizontalSpacing: 1, verticalSpacing: 1) {
                GridRow {
                    Text("方式 / 部位").font(.system(size: 10)).frame(width: labelWidth, height: headerHeight)
                    ForEach(chart.columns ?? [], id: \.self) { Text($0).font(.system(size: 10)).frame(width: unit * 2 + 6, height: headerHeight) }
                }
                ForEach(Array(rows.enumerated()), id: \.offset) { _, row in
                    GridRow {
                        Text(row.label).font(.system(size: 10)).frame(width: labelWidth, height: rowHeight(row))
                        ForEach(Array(row.cells.enumerated()), id: \.offset) { _, cell in
                            VStack(spacing: 0) {
                                ForEach(0..<max(1, (cell.ids.count + 1) / 2), id: \.self) { line in
                                    HStack(spacing: 2) {
                                        ForEach(0..<2, id: \.self) { column in
                                            let index = line * 2 + column
                                            if index < cell.ids.count, let entry = catalog.entries.first(where: { $0.id == cell.ids[index] }) {
                                                symbol(entry).frame(width: unit, height: symbolHeight)
                                            } else { Color.clear.frame(width: unit, height: symbolHeight) }
                                        }
                                    }
                                }
                                Spacer(minLength: 0)
                            }.padding(.top, 2).frame(width: unit * 2 + 6, height: rowHeight(row))
                                .background(Color.secondary.opacity(0.06))
                        }
                    }
                }
            }
        }.frame(height: height).accessibilityIdentifier("plus-matrix-" + chart.id)
    }
    private func vowels(_ chart: PlusChart) -> some View {
        let factor = max(0.5, zoom)
        return ScrollView(.horizontal) {
            ZStack {
                Path { path in
                    for y in [0.09, 0.37, 0.65, 0.91] {
                        let left = 0.20 + (y - 0.09) * 0.22 / 0.82
                        path.move(to: CGPoint(x: left * 500 * factor, y: y * 320 * factor)); path.addLine(to: CGPoint(x: 415 * factor, y: y * 320 * factor))
                    }
                    path.move(to: CGPoint(x: 100 * factor, y: 28.8 * factor)); path.addLine(to: CGPoint(x: 210 * factor, y: 291.2 * factor))
                    path.move(to: CGPoint(x: 415 * factor, y: 28.8 * factor)); path.addLine(to: CGPoint(x: 415 * factor, y: 291.2 * factor))
                }.stroke(Color.secondary.opacity(0.3), lineWidth: 1)
                ForEach(Array((chart.points ?? []).enumerated()), id: \.offset) { _, point in
                    HStack(spacing: 2) {
                        ForEach(point.ids, id: \.self) { id in
                            if let entry = catalog.entries.first(where: { $0.id == id }) { symbol(entry) }
                        }
                    }.padding(.horizontal, 2).background(.background).position(x: point.x * 5 * factor, y: point.y * 3.2 * factor)
                }
            }.frame(width: 500 * factor, height: 320 * factor)
        }.frame(height: 320 * factor)
    }
    var entries: [PlusEntry] {
        let matrixIDs = Set(catalog.charts[system, default: []].flatMap { chart in
            (chart.rows ?? []).flatMap { $0.cells.flatMap(\.ids) } + (chart.points ?? []).flatMap(\.ids)
        })
        return catalog.entries.filter { $0.system == system && (search.isEmpty ? (compact || !matrixIDs.contains($0.id)) : ($0.display.contains(search) || $0.nameZh.contains(search))) }
    }
    var sections: [String] {
        let order = ["pulmonic", "consonants", "nonpulmonic", "vowels", "diacritics", "suprasegmentals", "tones", "other", "combinations"]
        return Array(Set(entries.map(\.section))).sorted { a, b in
            let x = order.firstIndex(of: a) ?? 99, y = order.firstIndex(of: b) ?? 99
            return x == y ? a < b : x < y
        }
    }
    func symbol(_ entry: PlusEntry) -> some View {
        Button { select(entry.insertText) } label: {
            Text(entry.display).font(.ipa(23 * zoom)).fixedSize(horizontal: false, vertical: true)
                .frame(minWidth: 28 * zoom, minHeight: symbolHeight)
        }.buttonStyle(.plain).help(entry.nameZh).accessibilityLabel(entry.nameZh)
            .accessibilityIdentifier("plus-" + entry.id)
    }
    func sectionTitle(_ value: String) -> String {
        if let chart = catalog.charts[system, default: []].first(where: { $0.id == value }) { return chart.title }
        return ["vowels": "元音", "diacritics": "附加符号", "suprasegmentals": "超音段", "other": "其他符号", "tones": "声调", "pulmonic": "辅音", "nonpulmonic": "非肺部气流辅音", "combinations": "组合符号"][value] ?? value
    }
}

struct SettingsView: View {
    @ObservedObject var model: InputModel
    @State private var code = "a"
    @State private var outputs: [String] = []
    @State private var selectedOutput = 0
    @State private var plus = false
    @State private var importing = false
    @State private var exporting = false
    @State private var document = IPAFile()
    @State private var exportType = UTType.json
    @State private var appendSymbol = false
    @State private var help = false
    @State private var walkthrough = false
    @AppStorage("onboarding-v2-completed") private var introCompleted = false
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("让每个音，都有自己的键位。").font(.title2.bold())
                        Text("点击键位编辑，组合编码也会显示在首键的预览中。").font(.subheadline).foregroundStyle(.secondary)
                    }
                    Spacer()
                    Picker("每页候选", selection: $model.pageSize) { ForEach(3...9, id: \.self) { Text("\($0) 个").tag($0) } }.frame(maxWidth: 150)
                    Button("使用说明") { walkthrough = false; help = true }
                }
                Picker("国际音标转写规则", selection: Binding(get: { model.ruleId }, set: { model.selectRule($0) })) {
                    ForEach(model.rules) { Text($0.name).tag($0.id) }
                }
                KeyboardMap(mappings: model.mappings, edit: chooseCode)
                GroupBox {
                    VStack(alignment: .leading, spacing: 13) {
                        HStack {
                            Text("输入编码").font(.headline)
                            TextField("例如 a、voqs", text: $code).textFieldStyle(.roundedBorder).frame(maxWidth: 200)
                            Button("载入") { chooseCode(code) }
                            Text(code.count > 1 ? "组合输入" : "单键输入").font(.caption).foregroundStyle(.secondary)
                        }
                        Text("候选从左向右排列，输入时依次用 1–9 选择。点击一个候选后可用 IPA Plus 修改。").font(.caption).foregroundStyle(.secondary)
                        LazyVGrid(columns: [GridItem(.adaptive(minimum: 100, maximum: 180))], alignment: .leading, spacing: 10) {
                            ForEach(outputs.indices, id: \.self) { index in
                                VStack(spacing: 5) {
                                    HStack { Text("\(index + 1)").font(.caption2); Spacer(); Button("×") { outputs.remove(at: index); selectedOutput = min(selectedOutput, max(0, outputs.count - 1)) }.buttonStyle(.plain) }
                                    Text(IPAVisuals.display(outputs[index])).font(.ipa(32)).fixedSize(horizontal: false, vertical: true).frame(maxWidth: .infinity, minHeight: 70).padding(.vertical, 5)
                                        .accessibilityIdentifier("output-preview-" + String(index))
                                    Text(IPAVisuals.codePoints(outputs[index])).font(.system(size: 10, design: .monospaced)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                                    TextField("编辑输出", text: $outputs[index]).font(.ipa(24)).textFieldStyle(.roundedBorder).frame(minHeight: 44).accessibilityIdentifier("output-" + String(index))
                                }.padding(9).background(Color.accentColor.opacity(selectedOutput == index ? 0.16 : 0.06), in: RoundedRectangle(cornerRadius: 9)).onTapGesture { selectedOutput = index }
                            }
                            Button("＋ 添加候选") { outputs.append(""); selectedOutput = outputs.count - 1 }.frame(height: 65)
                        }
                        HStack {
                            Button("IPA Plus 选音标") { if outputs.isEmpty { outputs = [""] }; plus = true }.buttonStyle(.borderedProminent)
                            Toggle("追加到当前候选", isOn: $appendSymbol).font(.caption)
                        }
                        HStack {
                            Button("保存方案") { save() }.buttonStyle(.borderedProminent)
                            Button("撤销编辑") { chooseCode(code) }
                            Button("删除映射") { outputs = []; save() }
                        }
                    }.padding(7)
                }
                HStack {
                    Button("导入 IPA / JSON") { importing = true }
                    Button("导出 JSON") { export(json: true) }
                    Button("导出 IPA") { export(json: false) }
                }
                UpdateSettingsView()
                Link(destination: URL(string: "https://www.phonetictoolbox.com/")!) {
                    Label("PhoneticToolbox 语音工具箱官网", systemImage: "globe")
                }
                .accessibilityIdentifier("phonetictoolbox-website")
                .font(.subheadline)
                Text(model.status).font(.caption).foregroundStyle(.secondary)
                Text("五度调值：上划数字 1–5，或在 Mac 按 Shift＋1–5。普通候选用 1–9 选择，0 保留映射。").font(.caption).foregroundStyle(.secondary)
            }.padding(22)
        }
        .onAppear { chooseCode(code); if !introCompleted { walkthrough = true; help = true } }
        .sheet(isPresented: $help) {
            if walkthrough {
                FirstUseGuide { introCompleted = true; help = false }
            } else { VStack {
                HStack { Text("使用说明").font(.title2.bold()); Spacer(); Button("关闭") { help = false } }.padding()
                Button("重新查看上手教学") { walkthrough = true }
                UsageGuideView()
            }.frame(minWidth: 300, idealWidth: 580, minHeight: 400, idealHeight: 650) }
        }
        .sheet(isPresented: $plus) { PlusPicker { value in
            guard !outputs.isEmpty else { return }
            selectedOutput = min(selectedOutput, outputs.count - 1)
            if appendSymbol { outputs[selectedOutput] += value } else { outputs[selectedOutput] = value }
        } }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.json, .plainText, .data]) { result in
            do {
                let url = try result.get(); let accessed = url.startAccessingSecurityScopedResource()
                defer { if accessed { url.stopAccessingSecurityScopedResource() } }
                try model.importProfile(url); chooseCode(code)
            } catch { model.status = error.localizedDescription }
        }
        .fileExporter(isPresented: $exporting, document: document, contentType: exportType, defaultFilename: "音标键位方案") { result in
            if case .failure(let error) = result { model.status = error.localizedDescription }
        }
    }
    func chooseCode(_ key: String) { code = key; outputs = model.mappings[key, default: []]; selectedOutput = 0 }
    func save() {
        do {
            var next = model.mappings
            next[code] = outputs.filter { !$0.isEmpty }
            if next[code]?.isEmpty == true { next.removeValue(forKey: code) }
            try model.applyMappings(next, pageSize: model.pageSize); chooseCode(code)
        } catch { model.status = error.localizedDescription }
    }
    func export(json: Bool) {
        do {
            let data = json ? try JSONEncoder().encode(KeyboardProfile(layout: model.profileLayout, pageSize: model.pageSize, mappings: model.mappings, hotkeys: model.profileHotkeys)) : Data(model.cinText(model.mappings).utf8)
            document = IPAFile(data: data); exportType = json ? .json : .plainText; exporting = true
        } catch { model.status = error.localizedDescription }
    }
}
