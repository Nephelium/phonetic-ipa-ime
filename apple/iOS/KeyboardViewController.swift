import UIKit
import SwiftUI

final class KeyboardViewController: UIInputViewController {
    private let model = InputModel()
    private var host: UIHostingController<TouchKeyboard>?
    private var keyboardHeight: NSLayoutConstraint?
    private var landscape = false
    override func loadView() {
        let keyboardView = UIInputView(frame: CGRect(x: 0, y: 0, width: 0, height: 296), inputViewStyle: .keyboard)
        keyboardView.allowsSelfSizing = true
        view = keyboardView
    }
    private func keyboard() -> TouchKeyboard {
        TouchKeyboard(model: model, showsGlobe: needsInputModeSwitchKey, compactLayout: landscape,
                      nextKeyboard: { [weak self] in self?.advanceToNextInputMode() },
                      delete: { [weak self] in self?.textDocumentProxy.deleteBackward() },
                      newline: { [weak self] in self?.textDocumentProxy.insertText("\n") })
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        model.pinyin = UserDefaults.standard.object(forKey: "keyboard-pinyin-mode") as? Bool ?? true
        model.outputIpa = UserDefaults.standard.object(forKey: "keyboard-output-ipa") as? Bool ?? true
        model.onCommit = { [weak self] text in self?.textDocumentProxy.insertText(text) }
        let host = UIHostingController(rootView: keyboard())
        self.host = host
        host.view.backgroundColor = .clear
        addChild(host); view.addSubview(host.view); host.didMove(toParent: self)
        host.view.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            host.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            host.view.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            host.view.topAnchor.constraint(equalTo: view.topAnchor),
            host.view.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
        let height = view.heightAnchor.constraint(equalToConstant: 296)
        height.priority = UILayoutPriority(999); height.isActive = true
        keyboardHeight = height
        model.onChange = { [weak self] in
            guard let self else { return }
            self.updateKeyboardLayout()
            UserDefaults.standard.set(self.model.outputIpa, forKey: "keyboard-output-ipa")
            if UserDefaults.standard.bool(forKey: "keyboard-pinyin-mode") != self.model.pinyin { UserDefaults.standard.set(self.model.pinyin, forKey: "keyboard-pinyin-mode") }
        }
    }
    override func viewWillAppear(_ animated: Bool) { super.viewWillAppear(animated); model.reloadProfile(); host?.rootView = keyboard() }
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        updateKeyboardLayout()
        view.setNeedsLayout()
    }
    override func viewDidLayoutSubviews() { super.viewDidLayoutSubviews(); updateKeyboardLayout() }
    private func updateKeyboardLayout() {
        let next = view.window?.windowScene?.interfaceOrientation.isLandscape ?? false
        if next != landscape { landscape = next; host?.rootView = keyboard() }
        let height: CGFloat = (landscape ? 216 : 296) + (model.raw.isEmpty ? 0 : 50)
        if let keyboardHeight, abs(keyboardHeight.constant - height) > 0.5 { keyboardHeight.constant = height }
    }
    override func textWillChange(_ textInput: UITextInput?) { model.clear() }
}

struct TouchKeyboard: View {
    @ObservedObject var model: InputModel
    var showsGlobe: Bool
    var compactLayout = false
    var nextKeyboard: () -> Void
    var delete: () -> Void
    var newline: () -> Void
    @State private var help = false
    @State private var rules = false
    @State private var plus = false
    @State private var uppercase = false
    @State private var numbers = false
    @State private var symbolKey: String?
    @Environment(\.colorScheme) private var scheme
    private var accent: Color { scheme == .dark ? Color(red: 0.54, green: 0.84, blue: 0.82) : Color(red: 0.13, green: 0.36, blue: 0.40) }
    private var surface: Color { scheme == .dark ? Color(red: 0.13, green: 0.16, blue: 0.18) : Color(red: 0.90, green: 0.93, blue: 0.95) }
    private var keyColor: Color { scheme == .dark ? Color(red: 0.24, green: 0.28, blue: 0.30) : .white }
    private var letterHeight: CGFloat { compactLayout ? 28 : 44 }
    private var actionHeight: CGFloat { compactLayout ? 28 : 40 }
    var body: some View {
        ZStack(alignment: .top) {
            keyboardContent.allowsHitTesting(!help && !plus && !rules && symbolKey == nil).accessibilityHidden(help || plus || rules || symbolKey != nil)
            if help {
                VStack(spacing: 0) {
                    HStack {
                        Text("键盘使用说明").font(.headline)
                        Spacer()
                        Button("返回键盘") { help = false }.accessibilityIdentifier("help-close")
                    }.padding(12)
                    Divider()
                    UsageGuideView(compact: true)
                }.frame(maxWidth: .infinity, maxHeight: .infinity).background(surface)
            } else if rules {
                VStack(spacing: 0) {
                    HStack {
                        Text("国际音标转写规则").font(.headline)
                        Spacer()
                        Button("返回键盘") { rules = false }.accessibilityIdentifier("rules-close")
                    }.padding(10)
                    Divider()
                    ScrollView {
                        LazyVStack(spacing: 4) {
                            ForEach(model.rules) { rule in
                                Button {
                                    model.selectRule(rule.id); rules = false
                                } label: {
                                    HStack {
                                        Text(rule.name).font(.system(size: 14)).multilineTextAlignment(.leading)
                                        Spacer()
                                        if rule.id == model.ruleId { Image(systemName: "checkmark") }
                                    }.padding(.horizontal, 12).frame(minHeight: 34)
                                        .background(keyColor, in: RoundedRectangle(cornerRadius: 8))
                                }.buttonStyle(.plain).accessibilityIdentifier("rule-" + rule.id)
                            }
                        }.padding(8)
                    }.accessibilityIdentifier("transcription-rule-list")
                }.frame(maxWidth: .infinity, maxHeight: .infinity).background(surface)
            } else if plus {
                PlusPicker(compact: true, close: { plus = false }) { output in model.onCommit(output); model.clear() }
                    .background(surface)
            } else if let symbolKey {
                TouchSymbolPicker(key: symbolKey, mappings: model.mappings, accent: accent, surface: surface, keyColor: keyColor,
                                  select: { output in
                                      self.symbolKey = nil
                                      model.onCommit(output); model.clear()
                                  }, close: { self.symbolKey = nil })

            }
        }
    }
    private var keyboardContent: some View {
        VStack(spacing: compactLayout ? 3 : 6) {
            CandidateStrip(model: model, openPlus: { if model.raw.isEmpty { plus = true } else { model.status = "先确认当前候选，再打开音标表" } }, openRules: { rules = true }).frame(height: model.raw.isEmpty ? 44 : 94, alignment: .top)
                .background(keyColor.opacity(0.45), in: RoundedRectangle(cornerRadius: 10))
            HStack(spacing: 4) {
                ForEach(Array("1234567890").map(String.init), id: \.self) { digit in
                    Button { typeDigit(digit) } label: {
                        Text(digit).font(.custom("DoulosSIL", size: compactLayout ? 19 : 23))
                            .frame(maxWidth: .infinity).frame(height: compactLayout ? 24 : 36)
                            .overlay(alignment: .topTrailing) {
                                if (1...5).contains(Int(digit) ?? 0) {
                                    Image(systemName: "chevron.up").font(.system(size: 6, weight: .semibold)).foregroundStyle(accent.opacity(0.65)).padding(4)
                                }
                            }
                    }.buttonStyle(KeyboardKeyStyle(fill: keyColor.opacity(0.72), ink: .primary))
                        .accessibilityIdentifier("digit-" + digit)
                        .accessibilityHint((1...5).contains(Int(digit) ?? 0) ? "上划输入五度调值" : "")
                        .highPriorityGesture(DragGesture(minimumDistance: 16).onEnded { gesture in
                            if gesture.translation.height < -20 && abs(gesture.translation.height) > abs(gesture.translation.width) * 1.3 {
                                model.tone(digit)
                            }
                        })
                }
            }
            Group {
                VStack(spacing: compactLayout ? 3 : 5) {
                ForEach(numbers ? ["-/:;()$&@", "[]{}#%^*+=", ".,?!'\\_~<>"] : ["qwertyuiop", "asdfghjkl", "zxcvbnm"], id: \.self) { row in
                    HStack(spacing: 4) {
                        if row == "zxcvbnm" { function("⇧", width: 43, height: letterHeight) { uppercase.toggle() } }
                        ForEach(Array(row).map(String.init), id: \.self) { key in
                            Button {
                                typeKey(uppercase ? key.uppercased() : key)
                            } label: {
                                VStack(spacing: 0) {
                                    let typedKey = uppercase ? key.uppercased() : key
                                    let output = model.mappings[typedKey]?.first ?? typedKey
                                    if model.pinyin || output == typedKey {
                                        Text(typedKey).font(.ipa(compactLayout ? 20 : 25)).frame(height: letterHeight - 4)
                                    } else {
                                        Text(IPAVisuals.display(output)).font(.ipa(compactLayout ? 20 : 25)).frame(maxWidth: .infinity).frame(height: letterHeight - 4)
                                            .overlay(alignment: .topLeading) {
                                                if typedKey.lowercased() != "g" {
                                                    Text(typedKey).font(.custom("PingFangSC-Regular", size: 9)).foregroundStyle(.secondary).padding(.leading, 4).padding(.top, 1)
                                                }
                                            }
                                    }
                                }.frame(maxWidth: .infinity).frame(height: letterHeight)
                            }.overlay(alignment: .bottomTrailing) {
                                if !model.pinyin && directOutput(uppercase ? key.uppercased() : key) == nil {
                                    Text("···").font(.system(size: 10, weight: .semibold)).foregroundStyle(accent).padding(.trailing, 3).padding(.bottom, 1).allowsHitTesting(false)
                                }
                            }.buttonStyle(KeyboardKeyStyle(fill: keyColor, ink: .primary)).accessibilityIdentifier("key-" + key)
                                .accessibilityHint(model.pinyin ? "输入拼音" : directOutput(uppercase ? key.uppercased() : key) == nil ? "多个音标，点按选择" : "单个符号，点按直接输入")
                        }
                        if row == "zxcvbnm" { function("⌫", width: 43, height: letterHeight) { if model.raw.isEmpty { delete() } else { model.backspace() } } }
                    }.padding(.horizontal, row == "asdfghjkl" ? 16 : 0)
                }
                }.frame(height: letterHeight * 3 + (compactLayout ? 6 : 10))
            }
            HStack(spacing: 5) {
                if showsGlobe { function("🌐", width: 32, action: nextKeyboard) }
                function(model.pinyin ? "拼音" : "CIN", width: 40) { model.toggleMode() }
                function("说明", width: 42) { help = true }
                if numbers { function("⇧", width: 32) { uppercase.toggle() } }
                function(numbers ? "ABC" : "符号", width: 36) { numbers.toggle() }
                Button { if model.raw.isEmpty { model.onCommit(" ") } else { model.chooseDefault(model.selectedIndex) } } label: {
                    Text("空格").frame(maxWidth: .infinity).frame(height: actionHeight)
                }.buttonStyle(KeyboardKeyStyle(fill: keyColor, ink: .primary))
                if numbers { function("⌫", width: 38) { if model.raw.isEmpty { delete() } else { model.backspace() } } }
                Button { if model.raw.isEmpty { newline() } else if model.candidates.isEmpty { model.onCommit(model.raw); model.clear() } else { model.chooseDefault(model.selectedIndex) } } label: {
                    Image(systemName: "return").font(.system(size: 20)).frame(width: numbers ? 38 : 56, height: actionHeight)
                }.buttonStyle(KeyboardKeyStyle(fill: accent, ink: .white))
                    .accessibilityLabel("↵")
            }.font(.custom("PingFangSC-Medium", size: 13))
        }
        .padding(.horizontal, 6).padding(.vertical, compactLayout ? 4 : 7)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(surface).tint(accent).ignoresSafeArea(.container)
    }

    private func directOutput(_ key: String) -> String? {
        if let values = model.mappings[key], values.count != 1 { return nil }
        if model.mappings.keys.contains(where: { $0 != key && $0.hasPrefix(key) }) { return nil }
        return model.mappings[key]?.first ?? key
    }
    private func typeKey(_ key: String) {
        if model.pinyin { model.append(key) }
        else if model.raw.isEmpty { if let output = directOutput(key) { model.onCommit(output); model.clear() } else { symbolKey = key } }
        else { model.status = "先确认当前候选，再选择音标" }
    }
    private func typeDigit(_ digit: String) {
        if model.raw.hasPrefix("tone:") {
            model.tone(digit)
        } else if digit == "0" {
            if model.pinyin && model.raw.isEmpty { model.onCommit(digit) }
            else if model.raw.isEmpty { typeKey(digit) }
            else { model.append(digit) }
        } else if model.raw.hasPrefix("code:") {
            model.append(digit)
        } else if !model.raw.isEmpty {
            model.chooseDefault((Int(digit) ?? 1) - 1)
        } else { model.onCommit(digit) }
    }
    private func function(_ label: String, width: CGFloat, height: CGFloat? = nil, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Group {
                if label == "⇧" || label == "⌫" {
                    Image(systemName: label == "⇧" ? (uppercase ? "shift.fill" : "shift") : "delete.left").font(.system(size: 18))
                } else { Text(label) }
            }.frame(width: width, height: height ?? actionHeight)
        }.accessibilityLabel(label)
            .accessibilityIdentifier(label == "说明" ? "keyboard-help" : "function-" + label)
            .buttonStyle(KeyboardKeyStyle(fill: keyColor.opacity(0.52), ink: .primary))
    }
}

private struct KeyboardKeyStyle: ButtonStyle {
    var fill: Color
    var ink: Color
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.foregroundStyle(ink)
            .background(fill.opacity(configuration.isPressed ? 0.65 : 1), in: RoundedRectangle(cornerRadius: 7))
            .overlay(RoundedRectangle(cornerRadius: 7).strokeBorder(Color.primary.opacity(0.035), lineWidth: 0.5))
            .shadow(color: .black.opacity(configuration.isPressed ? 0 : 0.12), radius: 0, y: 1)
            .scaleEffect(configuration.isPressed ? 0.96 : 1)
            .animation(.easeOut(duration: 0.09), value: configuration.isPressed)
    }
}


private struct TouchSymbolPicker: View {
    let key: String
    let mappings: [String: [String]]
    let accent: Color
    let surface: Color
    let keyColor: Color
    let select: (String) -> Void
    let close: () -> Void
    @State private var selectedCode: String?
    private var code: String { selectedCode ?? codes[0] }
    private var codes: [String] {
        let matches = mappings.keys.filter { $0.hasPrefix(key) }.sorted()
        return matches.isEmpty ? [key] : ([key].filter { matches.contains($0) } + ["voqs"].filter { matches.contains($0) && $0 != key } + matches.filter { $0 != key && $0 != "voqs" })
    }
    var body: some View {
        VStack(spacing: 4) {
            HStack {
                Text(key + " · 选择音标").font(.headline)
                Spacer()
                Button("关闭", action: close).buttonStyle(.plain).frame(minWidth: 48, minHeight: 40)
                    .accessibilityIdentifier("symbol-picker-close")
            }
            ScrollView(.horizontal, showsIndicators: false) {
                LazyHStack(spacing: 7) {
                    ForEach(codes, id: \.self) { item in
                        Button { selectedCode = item } label: {
                            Text((item == key ? "单键 · " : "组合 · ") + item).font(.caption)
                                .padding(.horizontal, 10).padding(.vertical, 7)
                                .background(item == code ? accent.opacity(0.16) : keyColor, in: Capsule())
                        }.buttonStyle(.plain).accessibilityIdentifier("symbol-code-" + item)
                    }
                }.padding(.horizontal, 2)
            }.frame(height: 36)
            ScrollView {
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 56, maximum: 110))], spacing: 7) {
                    ForEach(Array((mappings[code] ?? [code]).enumerated()), id: \.offset) { index, output in
                        Button { select(output) } label: {
                            Text(IPAVisuals.display(output)).font(.ipa(27)).fixedSize(horizontal: false, vertical: true)
                                .frame(maxWidth: .infinity, minHeight: 48).padding(.horizontal, 3).padding(.vertical, 3)
                        }.buttonStyle(KeyboardKeyStyle(fill: keyColor, ink: .primary))
                            .accessibilityIdentifier("symbol-choice-" + code + "-" + String(index))
                            .accessibilityLabel(output)
                    }
                }.padding(.horizontal, 2).padding(.bottom, 8)
            }.id(code).accessibilityIdentifier("symbol-picker-list")
        }.padding(.horizontal, 10).padding(.vertical, 6)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(surface, in: RoundedRectangle(cornerRadius: 12)).tint(accent)
    }
}
