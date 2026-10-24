import SwiftUI
import UniformTypeIdentifiers
import CoreText

extension Font {
    static func ipa(_ size: CGFloat) -> Font {
        let fallback = CTFontDescriptorCreateWithNameAndSize("PTBIPAPlus-Regular" as CFString, size)
        let descriptor = CTFontDescriptorCreateWithAttributes([
            kCTFontNameAttribute: "DoulosSIL", kCTFontCascadeListAttribute: [fallback]
        ] as CFDictionary)
        return Font(CTFontCreateWithFontDescriptor(descriptor, size, nil))
    }
}

struct IPAFile: FileDocument {
    static var readableContentTypes: [UTType] { [.json, .plainText] }
    var data: Data
    init(data: Data = Data()) { self.data = data }
    init(configuration: ReadConfiguration) throws { data = configuration.file.regularFileContents ?? Data() }
    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper { FileWrapper(regularFileWithContents: data) }
}

struct UsageGuideView: View {
    var compact = false
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: compact ? 14 : 22) {
                #if os(iOS)
                if !compact {
                    section("开始使用", "在系统设置 → 通用 → 键盘 → 键盘 → 添加新键盘中，添加国际音标输入法。输入时长按地球键切换过来。")
                }
                #endif
                section("汉字与音标分别上屏", "在汉字模式输入拼音。点击候选上面的汉字输入汉字，点击下面的音标输入 IPA。按中 / IPA 切换输入模式。")
                section("多音字怎么选", "先按词语和输入拼音筛选读音。有多个带调读音时，点击音标会打开读音选择。选好后，再点击该候选的音标上屏。长按候选可重新选择读音。")
                #if os(iOS)
                section("数字与五度调值", "轻点数字 1–9 选择当前页的候选，没有候选时输入数字。上划数字 1–5 输入五度调值，1 最低、5 最高。依次上划 2、1、4，再点上方候选，即可输入 ˨˩˦、²¹⁴ 或 214。")
                section("先确认当前候选", "输入声调前，先让当前汉字或音标上屏。声调输入期间轻点 1–5 可继续追加调值，点空格确认。0 在 IPA 模式保留原有映射。")
                section("斜杠等符号键", "先切到 IPA，再点符号。点编码为 / 或 \\ 的键，从上方候选选择输出。\\ 收录原数字键上的声调音标。默认 / 输出斜杠，可以在设置中修改。长按可查看该键的候选。")
                #else
                section("数字与五度调值", "1–9 选择当前页的候选。按住 Shift，依次按 2、1、4，可得到 ˨˩˦、²¹⁴ 或 214，空格确认。0 保留原有映射。")
                section("Mac 快捷键", "Shift＋空格切换模式。Control＋回车输入 IPA，Control＋Shift＋回车重新选择读音。Page Up / Page Down 翻页。")
                section("斜杠等符号键", "IPA 模式直接按实体键盘的 / 或 \\，再选择候选。\\ 收录原数字键上的声调音标。默认 / 输出斜杠，可以在设置中修改。")
                #endif
                section("音标和组合编码", "IPA 模式按 CIN 编码输入音标，例如 a。长按字母可查看这个键和以它开头的组合编码，例如 v 下的 voqs。含数字的编码可用 code: 前缀输入。")
                section("键帽上的小字", "左上角的小字表示输入编码，中央的大字表示该键的第一个音标候选。例如 g 键输出的 ɡ 是 IPA 字符 U+0261。")
                section("修改键位", "打开设置，点击模拟键盘上的按键。候选从左到右排列，可直接编辑，也可用 IPA Plus 点选符号。保存方案后生效。每页候选可选 3–9 个，支持导入和导出 CIN / JSON。")
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

struct CandidateStrip: View {
    @ObservedObject var model: InputModel
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(model.raw.isEmpty ? (model.pinyin ? "拼音 → 汉字 / 音标" : "IPA · 输入编码") : model.raw)
                    .font(.system(.body, design: .monospaced)).foregroundStyle(.secondary)
                Spacer()
                if !model.candidates.isEmpty {
                    Button("‹") { model.turnPage(-1) }.disabled(model.page == 0)
                    Text("\(model.page + 1)").font(.caption)
                    Button("›") { model.turnPage(1) }.disabled((model.page + 1) * model.pageSize >= model.candidates.count)
                }
            }
            if model.pending != nil {
                HStack { Text("选择带调读音").font(.caption.bold()); Spacer(); Button("返回") { model.pending = nil; model.onChange() } }
                ScrollView(.horizontal) {
                    HStack(spacing: 8) {
                        ForEach(Array(model.readingOptions.enumerated()), id: \.offset) { index, option in
                            Button { model.chooseReading(index) } label: {
                                VStack { Text("\(index + 1)  \(option.pinyin)").font(.caption); Text(option.ipa).font(.ipa(25)) }
                                    .padding(9).background(Color.accentColor.opacity(0.10), in: RoundedRectangle(cornerRadius: 12))
                            }.buttonStyle(.plain).accessibilityIdentifier("reading-" + option.pinyin)
                        }
                    }
                }
            } else if !model.candidates.isEmpty {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(alignment: .top, spacing: 6) {
                        ForEach(Array(model.visible.enumerated()), id: \.element.id) { index, candidate in
                            VStack(spacing: 2) {
                                Button { model.choose(index, ipa: false) } label: {
                                    HStack(spacing: 5) {
                                        Text("\(index + 1)").font(.caption2).foregroundStyle(.secondary)
                                        Text(candidate.word).font(candidate.symbol ? .ipa(29) : .system(size: 20))
                                    }.padding(.horizontal, 9).padding(.vertical, 5)
                                }
                                .accessibilityIdentifier("hanzi-" + candidate.word + "-" + candidate.canonical)
                                if !candidate.symbol {
                                    Button { model.choose(index, ipa: true) } label: {
                                        Text(candidate.ipa.isEmpty ? (candidate.reading?.ambiguous == true ? "选择读音…" : "暂无音标") : candidate.ipa)
                                            .font(.ipa(23)).foregroundStyle(Color.accentColor)
                                            .padding(.horizontal, 9).padding(.bottom, 7)
                                    }.disabled(candidate.ipa.isEmpty && candidate.reading?.ambiguous != true)
                                        .accessibilityIdentifier("ipa-" + candidate.word + "-" + candidate.canonical)
                                }
                            }
                            .buttonStyle(.plain)
                            .background(Color.accentColor.opacity(index == model.selectedIndex ? 0.14 : 0.05), in: RoundedRectangle(cornerRadius: 11))
                            .contextMenu { Button("切换读音") { model.reopenReading(index) } }
                        }
                    }
                }
                .accessibilityIdentifier("candidate-scroll")
            }
            if !model.status.isEmpty { Text(model.status).font(.caption2).foregroundStyle(.secondary).lineLimit(2) }
        }
        .padding(10)
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
    @Environment(\.dismiss) private var dismiss
    @State private var system = "ipa"
    @State private var search = ""
    private let catalog = PlusCatalog.load()
    init(initialSystem: String = "ipa", select: @escaping (String) -> Void) {
        self.select = select
        _system = State(initialValue: initialSystem)
    }
    var body: some View {
        NavigationStack {
            VStack(spacing: 10) {
                Picker("音标表", selection: $system) { Text("IPA").tag("ipa"); Text("extIPA").tag("extipa"); Text("VoQS").tag("voqs") }.pickerStyle(.segmented)
                TextField("搜索符号或中文名称", text: $search).textFieldStyle(.roundedBorder)
                ScrollView([.horizontal, .vertical]) {
                    VStack(alignment: .leading, spacing: 18) {
                        if search.isEmpty {
                            ForEach(catalog.charts[system, default: []].filter { $0.kind == "matrix" }) { chart in
                                Text(chart.title).font(.headline)
                                Grid(horizontalSpacing: 1, verticalSpacing: 1) {
                                    GridRow { Text("方式 / 部位").font(.caption); ForEach(chart.columns ?? [], id: \.self) { Text($0).font(.caption).frame(minWidth: 66) } }
                                    ForEach(Array((chart.rows ?? []).enumerated()), id: \.offset) { _, row in
                                        GridRow {
                                            Text(row.label).font(.caption).frame(width: 76)
                                            ForEach(Array(row.cells.enumerated()), id: \.offset) { _, cell in
                                                LazyVGrid(columns: [GridItem(.fixed(29)), GridItem(.fixed(29))], spacing: 4) {
                                                    ForEach(cell.ids, id: \.self) { id in
                                                        if let entry = catalog.entries.first(where: { $0.id == id }) { symbol(entry) }
                                                    }
                                                }.frame(minWidth: 65, minHeight: 38).padding(.vertical, 5).background(Color.secondary.opacity(0.05))
                                            }
                                        }
                                    }
                                }
                            }
                            ForEach(catalog.charts[system, default: []].filter { $0.kind == "vowels" }) { chart in
                                Text(chart.title).font(.headline)
                                ZStack {
                                    Path { path in
                                        for y in [0.09, 0.37, 0.65, 0.91] {
                                            let left = 0.20 + (y - 0.09) * 0.22 / 0.82
                                            path.move(to: CGPoint(x: left * 500, y: y * 320))
                                            path.addLine(to: CGPoint(x: 0.83 * 500, y: y * 320))
                                        }
                                        path.move(to: CGPoint(x: 100, y: 28.8)); path.addLine(to: CGPoint(x: 210, y: 291.2))
                                        path.move(to: CGPoint(x: 415, y: 28.8)); path.addLine(to: CGPoint(x: 415, y: 291.2))
                                    }.stroke(Color.secondary.opacity(0.3), lineWidth: 1)
                                    ForEach(Array((chart.points ?? []).enumerated()), id: \.offset) { _, point in
                                        HStack(spacing: 2) {
                                            ForEach(point.ids, id: \.self) { id in
                                                if let entry = catalog.entries.first(where: { $0.id == id }) { symbol(entry) }
                                            }
                                        }.padding(.horizontal, 3).background(.background).position(x: point.x * 5, y: point.y * 3.2)
                                    }
                                }.frame(width: 500, height: 320)
                            }
                        }
                        ForEach(sections, id: \.self) { section in
                            Text(sectionTitle(section)).font(.headline)
                            LazyVGrid(columns: Array(repeating: GridItem(.fixed(54)), count: 8), spacing: 7) {
                                ForEach(entries.filter { $0.section == section }) { entry in symbol(entry).frame(width: 54, height: 40).background(Color.secondary.opacity(0.07), in: RoundedRectangle(cornerRadius: 7)) }
                            }
                        }
                    }.padding(4)
                }
            }.padding().navigationTitle("IPA Plus")
                .toolbar { Button("完成") { dismiss() } }
        }.frame(minWidth: 320, idealWidth: 1050, minHeight: 420, idealHeight: 680)
    }
    var entries: [PlusEntry] {
        let matrixIDs = Set(catalog.charts[system, default: []].flatMap { chart in
            (chart.rows ?? []).flatMap { $0.cells.flatMap(\.ids) } + (chart.points ?? []).flatMap(\.ids)
        })
        return catalog.entries.filter { $0.system == system && (search.isEmpty ? !matrixIDs.contains($0.id) : ($0.display.contains(search) || $0.nameZh.contains(search))) }
    }
    var sections: [String] { Array(Set(entries.map(\.section))).sorted() }
    func symbol(_ entry: PlusEntry) -> some View {
        Button { select(entry.insertText) } label: { Text(entry.display).font(.ipa(23)).frame(minWidth: 27, minHeight: 30) }
            .buttonStyle(.plain).help(entry.nameZh).accessibilityLabel(entry.nameZh)
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
                    Button("使用说明") { help = true }
                }
                KeyboardMap(mappings: model.mappings, edit: chooseCode)
                UpdateSettingsView()
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
                                    TextField("音标", text: $outputs[index]).font(.ipa(30)).textFieldStyle(.plain)
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
                    Button("导入 CIN / JSON") { importing = true }
                    Button("导出 JSON") { export(json: true) }
                    Button("导出 CIN") { export(json: false) }
                }
                Text(model.status).font(.caption).foregroundStyle(.secondary)
                Text("五度调值：上划数字 1–5，或在 Mac 按 Shift＋1–5。普通候选用 1–9 选择，0 保留映射。").font(.caption).foregroundStyle(.secondary)
            }.padding(22)
        }
        .onAppear { chooseCode(code) }
        .sheet(isPresented: $help) {
            VStack {
                HStack { Text("使用说明").font(.title2.bold()); Spacer(); Button("关闭") { help = false } }.padding()
                UsageGuideView()
            }.frame(minWidth: 300, idealWidth: 580, minHeight: 400, idealHeight: 650)
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
