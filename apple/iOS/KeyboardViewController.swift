import UIKit
import SwiftUI

final class KeyboardViewController: UIInputViewController {
    private let model = InputModel()
    private var host: UIHostingController<TouchKeyboard>?
    private var keyboardHeight: NSLayoutConstraint?
    override func loadView() {
        let keyboardView = UIInputView(frame: CGRect(x: 0, y: 0, width: 0, height: 296), inputViewStyle: .keyboard)
        keyboardView.allowsSelfSizing = true
        view = keyboardView
    }
    private func keyboard() -> TouchKeyboard {
        TouchKeyboard(model: model, showsGlobe: needsInputModeSwitchKey,
                      nextKeyboard: { [weak self] in self?.advanceToNextInputMode() },
                      delete: { [weak self] in self?.textDocumentProxy.deleteBackward() },
                      newline: { [weak self] in self?.textDocumentProxy.insertText("\n") })
    }
    override func viewDidLoad() {
        super.viewDidLoad()
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
            self.keyboardHeight?.constant = self.model.raw.isEmpty ? 296 : 346
        }
    }
    override func viewWillAppear(_ animated: Bool) { super.viewWillAppear(animated); model.reloadProfile(); host?.rootView = keyboard() }
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        keyboardHeight?.constant = model.raw.isEmpty ? 296 : 346
        view.setNeedsLayout()
    }
    override func textWillChange(_ textInput: UITextInput?) { model.clear() }
}

struct TouchKeyboard: View {
    @ObservedObject var model: InputModel
    var showsGlobe: Bool
    var nextKeyboard: () -> Void
    var delete: () -> Void
    var newline: () -> Void
    @State private var help = false
    @State private var uppercase = false
    @State private var numbers = false
    @State private var preview: String?
    @Environment(\.colorScheme) private var scheme
    private let accent = Color(red: 0.13, green: 0.36, blue: 0.40)
    private var surface: Color { scheme == .dark ? Color(red: 0.13, green: 0.16, blue: 0.18) : Color(red: 0.90, green: 0.93, blue: 0.95) }
    private var keyColor: Color { scheme == .dark ? Color(red: 0.24, green: 0.28, blue: 0.30) : .white }
    var body: some View {
        VStack(spacing: 6) {
            CandidateStrip(model: model).frame(height: model.raw.isEmpty ? 44 : 94, alignment: .top)
                .background(keyColor.opacity(0.45), in: RoundedRectangle(cornerRadius: 10))
            HStack(spacing: 4) {
                ForEach(Array("1234567890").map(String.init), id: \.self) { digit in
                    Button { typeDigit(digit) } label: {
                        Text(digit).font(.custom("DoulosSIL", size: 23))
                            .frame(maxWidth: .infinity).frame(height: 36)
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
                VStack(spacing: 5) {
                ForEach(numbers ? ["-/:;()$&@", "[]{}#%^*+=", ".,?!'\\_~<>"] : ["qwertyuiop", "asdfghjkl", "zxcvbnm"], id: \.self) { row in
                    HStack(spacing: 4) {
                        if row == "zxcvbnm" { function("⇧", width: 43, height: 44) { uppercase.toggle() } }
                        ForEach(Array(row).map(String.init), id: \.self) { key in
                            Button {
                                if let digit = Int(key), (1...9).contains(digit), !model.raw.isEmpty && !model.raw.hasPrefix("tone:") && !model.raw.hasPrefix("code:") {
                                    model.choose(digit - 1, ipa: false)
                                } else if let digit = Int(key), (1...9).contains(digit), model.raw.isEmpty {
                                    model.onCommit(key)
                                } else { model.append(uppercase ? key.uppercased() : key) }
                            } label: {
                                VStack(spacing: 0) {
                                    let typedKey = uppercase ? key.uppercased() : key
                                    let output = model.mappings[typedKey]?.first ?? typedKey
                                    if model.pinyin || output == typedKey {
                                        Text(typedKey).font(.ipa(25)).frame(height: 40)
                                    } else {
                                        Text(output).font(.ipa(25)).frame(maxWidth: .infinity).frame(height: 40)
                                            .overlay(alignment: .topLeading) {
                                                Text(typedKey).font(.custom("PingFangSC-Regular", size: 9)).foregroundStyle(.secondary).padding(.leading, 4).padding(.top, 1)
                                            }
                                    }
                                }.frame(maxWidth: .infinity).frame(height: 44)
                            }.buttonStyle(KeyboardKeyStyle(fill: keyColor, ink: .primary)).accessibilityIdentifier("key-" + key).simultaneousGesture(LongPressGesture(minimumDuration: 0.45).onEnded { _ in preview = key })
                        }
                        if row == "zxcvbnm" { function("⌫", width: 43, height: 44) { if model.raw.isEmpty { delete() } else { model.backspace() } } }
                    }.padding(.horizontal, row == "asdfghjkl" ? 16 : 0)
                }
                }.frame(height: 144)
            }
            HStack(spacing: 5) {
                if showsGlobe { function("🌐", width: 32, action: nextKeyboard) }
                function(model.pinyin ? "中" : "IPA", width: 40) { model.toggleMode() }
                function("说明", width: 42) { help = true }
                if numbers { function("⇧", width: 32) { uppercase.toggle() } }
                function(numbers ? "ABC" : "符号", width: 36) { numbers.toggle() }
                Button { if model.raw.isEmpty { model.onCommit(" ") } else { model.choose(model.selectedIndex, ipa: false) } } label: {
                    Text("空格").frame(maxWidth: .infinity).frame(height: 40)
                }.buttonStyle(KeyboardKeyStyle(fill: keyColor, ink: .primary))
                if numbers { function("⌫", width: 38) { if model.raw.isEmpty { delete() } else { model.backspace() } } }
                Button { if model.raw.isEmpty { newline() } else if model.candidates.isEmpty { model.onCommit(model.raw); model.clear() } else { model.choose(model.selectedIndex, ipa: false) } } label: {
                    Image(systemName: "return").font(.system(size: 20)).frame(width: numbers ? 38 : 56, height: 40)
                }.buttonStyle(KeyboardKeyStyle(fill: accent, ink: .white))
                    .accessibilityLabel("↵")
            }.font(.custom("PingFangSC-Medium", size: 13))
        }
        .padding(.horizontal, 6).padding(.vertical, 7)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(surface).tint(accent).ignoresSafeArea(.container)
        .overlay(alignment: .top) {
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
            } else if let preview {
                VStack {
                    KeyPreview(key: preview, mappings: model.mappings)
                    Button("关闭预览") { self.preview = nil }.buttonStyle(.borderedProminent)
                }.padding(10).background(.regularMaterial)
            }
        }
    }
    private func typeDigit(_ digit: String) {
        if model.raw.hasPrefix("tone:") {
            model.tone(digit)
        } else if digit == "0" {
            if model.pinyin && model.raw.isEmpty { model.onCommit(digit) }
            else { model.append(digit) }
        } else if model.raw.hasPrefix("code:") {
            model.append(digit)
        } else if !model.raw.isEmpty {
            model.choose((Int(digit) ?? 1) - 1, ipa: false)
        } else { model.onCommit(digit) }
    }
    private func function(_ label: String, width: CGFloat, height: CGFloat = 40, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Group {
                if label == "⇧" || label == "⌫" {
                    Image(systemName: label == "⇧" ? (uppercase ? "shift.fill" : "shift") : "delete.left").font(.system(size: 18))
                } else { Text(label) }
            }.frame(width: width, height: height)
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
