#include "settings/native/native_settings_pages.h"
#include "settings/native/native_ipa_keyboard.h"
#include "settings/native/native_ipa_plus.h"
#include "settings/native/native_ipa_candidates.h"
#include "settings/native/native_ipa_onboarding.h"
#include "ipa/ipa_profile.h"
#include "ipa/ipa_keyboard_layout.h"
#include "ipa/ipa_runtime.h"
#include "msimeui/Window.h"
#include "defines/defines.h"
#include <commdlg.h>
#include <iomanip>
#include <cmath>
#include <sstream>
#pragma comment(lib, "comdlg32.lib")

namespace native_settings
{
namespace
{
using PhoneticIpa::KeyboardKeys;
using PhoneticIpa::KeyCap;
using PhoneticIpa::KeyCode;

using KeyboardCanvas = IpaKeyboardCanvas;

std::wstring CodePoints(const std::string &text)
{
    const auto wide = Widen(text);
    std::wostringstream stream;
    stream << std::hex << std::uppercase;
    for (size_t i = 0; i < wide.size(); ++i)
    {
        unsigned cp = wide[i];
        if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < wide.size())
            cp = 0x10000 + ((cp - 0xd800) << 10) + (wide[++i] - 0xdc00);
        stream << L"U+" << std::setw(4) << std::setfill(L'0') << cp << L" ";
    }
    return stream.str();
}
std::filesystem::path ChooseFile(HWND owner, bool save, bool cin = false)
{
    WCHAR path[32768]{};
    if (save)
        wcscpy_s(path, cin ? L"国际音标键位.cin" : L"国际音标键位.json");
    OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner = owner;
    dialog.lpstrFile = path;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter = save ? (cin ? L"IPA 码表\0*.cin\0\0" : L"完整键位方案\0*.json\0\0")
                              : L"键位方案与 IPA 码表\0*.json;*.cin\0所有文件\0*.*\0\0";
    dialog.lpstrDefExt = cin ? L"cin" : L"json";
    dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))
        return path;
    return {};
}

struct Editor
{
    phonetic_ipa::CinTable table;
    std::vector<phonetic_ipa::CinTable> history;
    HostServices *host;
    KeyboardCanvas *keyboard = nullptr;
    TextField *code = nullptr;
    IpaCandidateEditor *output = nullptr;
    Label *status = nullptr, *detail = nullptr, *pickerTarget = nullptr;
    Dropdown *codes = nullptr, *layout = nullptr, *layer = nullptr;
    unsigned chordKey = 0, modifiers = 0;
    std::string selectedCode;
    bool editing = false, append = false;
    void ReserveCandidateDigits()
    {
        std::vector<std::string> moved;
        auto &entries = table.EntriesForEdit();
        for (char digit = '1'; digit <= '9'; ++digit)
        {
            const auto found = entries.find(std::string(1, digit));
            if (found == entries.end())
                continue;
            moved.insert(moved.end(), found->second.begin(), found->second.end());
            entries.erase(found);
        }
        if (!moved.empty())
        {
            const auto &existing = table.Lookup("\\");
            moved.insert(moved.end(), existing.begin(), existing.end());
            entries["\\"] = std::move(moved);
        }
    }
    void UpdatePickerTarget()
    {
        if (pickerTarget)
            pickerTarget->SetText(L"当前编码 " + code->GetText() + L" · 第 " + std::to_wstring(output->Selected() + 1) +
                                  L" 项 → " + output->SelectedOutput().substr(0, 48));
    }
    void Status(const std::wstring &text)
    {
        status->SetText(text);
    }
    std::vector<std::string> Outputs()
    {
        std::vector<std::string> result;
        for (const auto &value : output->Values())
            result.push_back(Narrow(value));
        return result;
    }
    bool Stash()
    {
        if (!editing)
            return true;
        try
        {
            auto next = table;
            const auto values = Outputs();
            if (chordKey)
            {
                if (values.size() != 1)
                    throw std::runtime_error("组合键对应一项输出；连续编码可设置多个候选");
                next.HotkeysForEdit()[{modifiers, chordKey}] = values.front();
            }
            else
            {
                auto key = Narrow(code->GetText());
                if (key.empty())
                    throw std::runtime_error("请填写编码");
                if (key.size() == 1 && key[0] >= '1' && key[0] <= '9')
                    throw std::runtime_error("数字 1–9 用于选择候选，请使用其他编码");
                if (table.AllEntries().find(key) == table.AllEntries().end() &&
                    (key.size() > 32 ||
                     !std::all_of(key.begin(), key.end(), [](unsigned char c) { return c >= 33 && c <= 126; })))
                    throw std::runtime_error("新编码请使用 1–32 个英文字母、数字或键盘符号");
                next.EntriesForEdit()[key] = values;
            }
            PhoneticIpa::ValidateProfile(next);
            history.push_back(table);
            table = std::move(next);
            if (!chordKey)
                selectedCode = Narrow(code->GetText());
            editing = false;
            RefreshCodes();
            keyboard->Refresh();
            return true;
        }
        catch (const std::exception &e)
        {
            Status(Widen(e.what()));
            return false;
        }
    }
    void RefreshCodes()
    {
        std::vector<DropdownOption> options;
        for (const auto &[key, values] : table.AllEntries())
            options.push_back({Widen(key) + (key.size() > 1 ? L" · 组合输入" : L" · 单键") + L" · " +
                                   std::to_wstring(values.size()) + L" 项",
                               key});
        codes->SetOptions(std::move(options));
    }
    // Stash rebuilds the dropdown's options. Own the requested key before that
    // rebuild, because dropdown callbacks pass a reference into those options.
    void SelectCode(std::string key)
    {
        if (!Stash())
            return;
        chordKey = 0;
        selectedCode = key;
        codes->SetValue(key);
        code->SetDisabled(false);
        code->SetText(Widen(key));
        output->SetSingle(false);
        const bool reserved = key.size() == 1 && key[0] >= '1' && key[0] <= '9';
        output->SetDisabled(reserved);
        std::wstring values;
        for (const auto &item : table.Lookup(key))
        {
            if (reserved)
                break;
            if (!values.empty())
                values += L"\n";
            values += Widen(item);
        }
        output->SetText(values);
        detail->SetText(CodePoints(Narrow(output->SelectedOutput())));
        UpdatePickerTarget();
        Status(key.size() == 1 && key[0] >= '1' && key[0] <= '9'
                   ? L"数字 1–9 保留用于选候选。原 1 键的声调音标已移到反斜杠键 \\。"
                   : L"点击候选卡片，在下方修改这一项；可新增、删除、调整顺序。修改后点击应用方案。");
        editing = false;
    }
    void SelectKey(unsigned vk)
    {
        if (!Stash())
            return;
        if (modifiers < 2 && !PhoneticIpaKey::IsToneShortcut(vk, modifiers))
        {
            const auto key = KeyCode(vk, modifiers == 1);
            if (key.empty())
            {
                Status(L"此键保留用于控制输入。请点击字母、数字或符号键。");
                return;
            }
            SelectCode(key);
        }
        else
        {
            if (!PhoneticIpaKey::EditableChord(modifiers, vk) && !PhoneticIpaKey::IsToneShortcut(vk, modifiers))
            {
                Status(L"此组合保留给输入法或系统，请换一个键。");
                return;
            }
            chordKey = vk;
            code->SetDisabled(true);
            code->SetText(PhoneticIpaKey::IsToneShortcut(vk, modifiers) ? L"Shift + " + std::to_wstring(vk - '0')
                                                                        : L"组合键 · VK " + std::to_wstring(vk));
            output->SetSingle(true);
            output->SetDisabled(false);
            output->SetText(Widen(PhoneticIpaKey::IsToneShortcut(vk, modifiers)
                                      ? phonetic_ipa::ToneShortcutOutput(table, vk)
                                      : table.Hotkey(modifiers, vk)));
            detail->SetText(CodePoints(Narrow(output->SelectedOutput())));
            UpdatePickerTarget();
            editing = false;
            Status(L"组合键在 IPA 模式且编码为空时生效。请保留常用编辑快捷键。");
        }
        keyboard->selected = vk;
        keyboard->Refresh();
    }
    void ChangeLayer(unsigned value)
    {
        if (!Stash())
        {
            layer->SetValue(std::to_string(modifiers));
            return;
        }
        modifiers = value;
        keyboard->modifiers = modifiers;
        // A key reserved in the new layer must not keep an editable stale chord.
        if ((modifiers >= 2 && !PhoneticIpaKey::EditableChord(modifiers, keyboard->selected)) ||
            (modifiers < 2 && KeyCode(keyboard->selected, modifiers == 1).empty()))
            keyboard->selected = 'A';
        SelectKey(keyboard->selected);
    }
    void Undo()
    {
        if (editing)
        {
            editing = false;
            if (chordKey)
                SelectKey(chordKey);
            else
                SelectCode(selectedCode);
            Status(L"已撤销当前编辑框的修改。");
            return;
        }
        if (history.empty())
        {
            Status(L"暂无已暂存的修改。");
            return;
        }
        table = history.back();
        history.pop_back();
        editing = false;
        layout->SetValue(table.layout);
        keyboard->size = table.layout;
        RefreshCodes();
        if (chordKey)
            SelectKey(chordKey);
        else
            SelectCode(selectedCode);
        keyboard->Refresh();
        Status(L"已撤销，点击应用使输入法使用此方案。");
    }
    void InsertSymbol(const Symbol &s)
    {
        output->SetSelectedOutput((append ? output->SelectedOutput() : L"") + Widen(s.output));
        editing = true;
        detail->SetText(s.name + L"\n" + CodePoints(Narrow(output->SelectedOutput())));
        UpdatePickerTarget();
    }
    std::vector<std::pair<std::wstring, std::wstring>> PreviewOutputs(unsigned vk) const
    {
        std::vector<std::pair<std::wstring, std::wstring>> result;
        if (PhoneticIpaKey::IsToneShortcut(vk, modifiers))
            return {{Widen(phonetic_ipa::ToneShortcutOutput(table, vk)), L"Shift 声调"}};
        if (modifiers >= 2)
        {
            const auto value = table.Hotkey(modifiers, vk);
            if (!value.empty())
                result.push_back({Widen(value), L""});
            return result;
        }
        const auto prefix = KeyCode(vk, modifiers == 1);
        if (prefix.empty() || (modifiers == 0 && prefix.size() == 1 && prefix[0] >= '1' && prefix[0] <= '9'))
            return result;
        for (const auto &value : table.Lookup(prefix))
            result.push_back({Widen(value), L""});
        std::vector<std::pair<std::string, const std::vector<std::string> *>> combinations;
        for (const auto &[key, values] : table.AllEntries())
            if (key.size() > prefix.size() && key.compare(0, prefix.size(), prefix) == 0)
                combinations.push_back({key, &values});
        // Named groups such as voqs/click appear before short two-key variants.
        std::stable_sort(combinations.begin(), combinations.end(),
                         [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
        for (const auto &[key, values] : combinations)
            for (const auto &value : *values)
                result.push_back({Widen(value), L"组合输入 · " + Widen(key)});
        return result;
    }
    void Apply()
    {
        if (!Stash())
            return;
        try
        {
            PhoneticIpa::ValidateProfile(table);
            PhoneticIpa::WriteProfileFile(PhoneticIpa::ResourceRoot() / "active.cin", table.Serialize());
            Status(L"方案已应用。下一次输入会使用新键位；原方案备份保留在同一目录。");
        }
        catch (const std::exception &e)
        {
            Status(Widen(e.what()));
        }
    }
};
} // namespace

VisualPtr BuildIpaKeyboardPage(PageContext &context)
{
    PageBuilder page(L"音标键盘");
    page.AddRaw(std::make_shared<IpaPageHero>(), 22);
    auto state = std::make_shared<Editor>();
    state->host = &context.host;
    try
    {
        state->table = PhoneticIpa::Cin();
        state->ReserveCandidateDigits();
        if (!std::filesystem::exists(PhoneticIpa::ResourceRoot() / "active.cin"))
        {
            const auto sizes = PhoneticIpa::ReportedKeyboardSizes();
            if (sizes.size() == 1)
                state->table.layout = PhoneticIpa::LayoutForReportedKeyCount(sizes.front());
        }
    }
    catch (const std::exception &e)
    {
        page.AddRaw(MakeDescription(Widen(e.what())));
        return page.Build();
    }
    auto top = std::make_shared<Column>(12.0f);
    top->Add(MakeDescription(L"悬停放大查看音标，点击原键位编辑。移到相邻键即可切换预览；候选较多时可用滚轮翻页。"));
    auto controls = std::make_shared<Row>(12.0f);
    controls->SetWrap(true, 8);
    auto layout = std::make_shared<Dropdown>(Options(
        {{"104", L"全尺寸 104"}, {"87", L"TKL 87"}, {"75", L"75% 紧凑"}, {"65", L"65% 紧凑"}, {"60", L"60% 紧凑"}}));
    state->layout = layout.get();
    layout->SetValue(state->table.layout);
    auto layer = std::make_shared<Dropdown>(Options({{"0", L"基本层"},
                                                     {"1", L"Shift 层"},
                                                     {"2", L"Ctrl 层"},
                                                     {"3", L"Ctrl+Shift"},
                                                     {"4", L"Alt 层"},
                                                     {"5", L"Alt+Shift"},
                                                     {"6", L"Ctrl+Alt"},
                                                     {"7", L"Ctrl+Alt+Shift"}}));
    state->layer = layer.get();
    layer->SetValue("0");
    controls->Add(layout);
    controls->Add(layer);
    auto detect = std::make_shared<Button>(L"检测键盘尺寸");
    controls->Add(detect);
    top->Add(controls);
    top->Add(MakeDescription(L"键位总览"));
    auto keyboard = std::make_shared<KeyboardCanvas>();
    state->keyboard = keyboard.get();
    keyboard->size = state->table.layout;
    keyboard->choose = [state](unsigned vk) { state->SelectKey(vk); };
    keyboard->summary = [state](unsigned vk) {
        if (state->modifiers == 0 && vk >= '1' && vk <= '9')
            return std::wstring{};
        if (PhoneticIpaKey::IsToneShortcut(vk, state->modifiers))
            return Widen(phonetic_ipa::ToneShortcutOutput(state->table, vk));
        if (state->modifiers >= 2)
            return Widen(state->table.Hotkey(state->modifiers, vk));
        const auto &values = state->table.Lookup(KeyCode(vk, state->modifiers == 1));
        return values.empty() ? L"" : Widen(values.front());
    };
    top->Add(keyboard);
    auto hoverInfo = MakeDescription(L"单按 Shift 切换拼音／IPA；Caps Lock 切换音标／汉字输出。");
    hoverInfo->SetMinHeight(36);
    hoverInfo->SetMaxHeight(36);
    keyboard->outputs = [state](unsigned vk) {
        std::vector<std::wstring> result;
        for (const auto &item : state->PreviewOutputs(vk))
            result.push_back(item.first);
        return result;
    };
    keyboard->outputLabels = [state](unsigned vk) {
        std::vector<std::wstring> result;
        for (const auto &item : state->PreviewOutputs(vk))
            result.push_back(item.second);
        return result;
    };
    std::weak_ptr<Label> weakHover = hoverInfo;
    keyboard->hover = [weakHover, state](unsigned vk) {
        if (auto label = weakHover.lock())
        {
            const auto count = state->PreviewOutputs(vk).size();
            label->SetText(count ? std::to_wstring(count) +
                                       L" 个音标 · 点击原键位编辑；移动到相邻键即可切换，滚轮查看更多。"
                           : state->modifiers == 0 && vk >= '1' && vk <= '9'
                               ? L"数字 1–9 用于选择候选，不分配音标；声调音标在反斜杠键 \\。"
                               : L"此键尚无音标输出，点击可设置。");
        }
    };
    top->Add(hoverInfo);
    top->Add(MakeDescription(L"按住 Shift 输入 1–5，默认得到 ˩、˨、˧、˦、˥，可连续输入调型。Shift "
                             L"层可编辑这些快捷键；普通 1–9 选候选，0 保留原映射。"));
    top->Add(MakeRow(L"每页候选数", L"3–9 项，选择后自动保存",
                     BindDropdown(context, "appearance.page_size", NumberOptions(3, 9, L" 项"), Dropdown::Options{},
                                  ValueKind::Integer)));
    page.AddSection(top);
    auto edit = std::make_shared<Column>(10.0f);
    Dropdown::Options codeOptions;
    codeOptions.searchable = true;
    auto codes = std::make_shared<Dropdown>(std::vector<DropdownOption>{}, codeOptions);
    state->codes = codes.get();
    state->RefreshCodes();
    edit->Add(MakeTitle(L"编辑按键输出"));
    auto codeRow = std::make_shared<IpaResponsiveRow>(18.0f, 600.0f);
    auto existingCode = std::make_shared<Column>(6.0f);
    existingCode->Add(MakeDescription(L"切换已有编码 · 可搜索组合输入，如 voqs"));
    existingCode->Add(codes);
    codeRow->Add(existingCode, 1);
    TextField::Options codeField;
    codeField.placeholder = L"连续编码，例如 a、tone、click";
    auto code = std::make_shared<TextField>(codeField);
    state->code = code.get();
    auto currentCode = std::make_shared<Column>(6.0f);
    currentCode->Add(MakeDescription(L"输入编码 · 一个按键或连续键序列"));
    currentCode->Add(code);
    codeRow->Add(currentCode, 1);
    edit->Add(codeRow);
    edit->Add(MakeDescription(L"候选输出 · 从左到右排列，顺序与打字时一致"));
    auto output = std::make_shared<IpaCandidateEditor>();
    state->output = output.get();
    edit->Add(output);
    auto detail = MakeDescription(L"选中一项后可直接修改，也可打开 IPA Plus 点选符号；追加模式把附加符号加到当前项。");
    state->detail = detail.get();
    edit->Add(detail);
    auto status = MakeDescription(L"尚未修改方案。");
    state->status = status.get();
    code->SetOnChanged([state](const std::wstring &) { state->editing = true; });
    output->SetOnChanged([state](const std::wstring &) {
        state->editing = true;
        state->detail->SetText(CodePoints(Narrow(state->output->SelectedOutput())));
        state->UpdatePickerTarget();
    });
    output->SetOnSelected([state] {
        state->detail->SetText(CodePoints(Narrow(state->output->SelectedOutput())));
        state->UpdatePickerTarget();
    });
    codes->SetOnChanged([state](size_t, const DropdownOption &option) { state->SelectCode(option.value); });
    layout->SetOnChanged([state](size_t, const DropdownOption &option) {
        if (!state->Stash())
        {
            state->layout->SetValue(state->table.layout);
            return;
        }
        state->history.push_back(state->table);
        state->table.layout = option.value;
        state->keyboard->size = option.value;
        state->keyboard->Refresh();
        state->Status(L"键盘尺寸已更改，点击应用保存。");
    });
    layer->SetOnChanged([state](size_t, const DropdownOption &option) {
        state->ChangeLayer(static_cast<unsigned>(std::stoul(option.value)));
    });
    detect->SetOnClick([state] {
        if (!state->Stash())
            return;
        const auto sizes = PhoneticIpa::ReportedKeyboardSizes();
        if (sizes.size() != 1)
        {
            state->Status(sizes.empty() ? L"Windows 未提供可用的按键数，请选择与键盘相符的尺寸。"
                                        : L"检测到多种键盘尺寸，请手动选择当前使用的键盘。");
            return;
        }
        state->history.push_back(state->table);
        state->table.layout = PhoneticIpa::LayoutForReportedKeyCount(sizes.front());
        state->layout->SetValue(state->table.layout);
        state->keyboard->size = state->table.layout;
        state->keyboard->Refresh();
        state->Status(L"Windows 报告 " + std::to_wstring(sizes.front()) +
                      L" 键，已匹配相近布局。若设备报告与实物不同，可手动选择；点击应用保存。");
    });
    auto actions = std::make_shared<Row>(8.0f);
    actions->SetWrap(true, 8);
    auto addAction = [actions](const wchar_t *title, std::function<void()> click,
                               ButtonKind kind = ButtonKind::Secondary) {
        auto button = std::make_shared<Button>(title, kind);
        button->SetOnClick(std::move(click));
        actions->Add(button);
    };
    addAction(L"应用方案", [state] { state->Apply(); }, ButtonKind::Primary);
    addAction(L"撤销修改", [state] { state->Undo(); });
    addAction(L"移除当前映射", [state] {
        state->host->Confirm(
            L"从当前编辑方案移除此映射？应用之前仍可撤销。",
            [state](bool confirmed) {
                if (!confirmed)
                    return;
                state->history.push_back(state->table);
                if (state->chordKey)
                    state->table.HotkeysForEdit().erase({state->modifiers, state->chordKey});
                else
                    state->table.EntriesForEdit().erase(Narrow(state->code->GetText()));
                state->output->SetText(L"");
                state->editing = false;
                state->RefreshCodes();
                state->keyboard->Refresh();
                state->Status(L"已移除，点击应用后生效。");
            },
            L"移除映射", L"移除", L"取消");
    });
    addAction(L"导入 IPA / JSON", [state] {
        if (!state->Stash())
            return;
        const auto file = ChooseFile(state->host->Hwnd(), false);
        if (file.empty())
            return;
        try
        {
            auto incoming = PhoneticIpa::ImportProfile(file);
            state->history.push_back(state->table);
            state->table = std::move(incoming);
            state->ReserveCandidateDigits();
            state->editing = false;
            state->layout->SetValue(state->table.layout);
            state->keyboard->size = state->table.layout;
            state->RefreshCodes();
            state->SelectKey(state->keyboard->selected);
            state->Status(L"已载入预览。数字 1–9 保留选候选，原有输出移到反斜杠键 \\；点击应用方案保存。");
        }
        catch (const std::exception &e)
        {
            state->Status(Widen(e.what()));
        }
    });
    addAction(L"导出完整 JSON", [state] {
        if (!state->Stash())
            return;
        auto file = ChooseFile(state->host->Hwnd(), true);
        if (file.empty())
            return;
        try
        {
            PhoneticIpa::WriteProfileFile(file, PhoneticIpa::ProfileJson(state->table));
            state->Status(L"完整方案已导出，含键盘尺寸与组合键。");
        }
        catch (const std::exception &e)
        {
            state->Status(Widen(e.what()));
        }
    });
    addAction(L"导出 IPA", [state] {
        if (!state->Stash())
            return;
        auto file = ChooseFile(state->host->Hwnd(), true, true);
        if (file.empty())
            return;
        try
        {
            PhoneticIpa::WriteProfileFile(file, state->table.Serialize(false));
            state->Status(L"IPA 已导出，包含连续编码与候选。组合键请使用 JSON 保存。");
        }
        catch (const std::exception &e)
        {
            state->Status(Widen(e.what()));
        }
    });
    edit->Add(actions);
    edit->Add(status);
    auto picker = std::make_shared<Column>(10.0f);
    picker->Add(MakeTitle(L"IPA Plus · 点选音标"));
    auto pickerRow = std::make_shared<Row>(8.0f);
    auto insertion = std::make_shared<Dropdown>(Options({{"replace", L"替换当前输出"}, {"append", L"追加到输出末尾"}}));
    insertion->SetValue("replace");
    TextField::Options searchField;
    searchField.placeholder = L"搜索符号或中文名称";
    auto search = std::make_shared<TextField>(searchField);
    std::vector<std::pair<std::shared_ptr<Button>, std::string>> systemTabs;
    for (const auto &[value, title] :
         std::vector<std::pair<std::string, std::wstring>>{{"ipa", L"IPA"}, {"extipa", L"extIPA"}, {"voqs", L"VoQS"}})
    {
        auto tab = std::make_shared<Button>(title, ButtonKind::Soft);
        tab->SetMinWidth(72);
        tab->SetFontSize(14);
        tab->SetActive(value == "ipa");
        systemTabs.push_back({tab, value});
        pickerRow->Add(tab);
    }
    pickerRow->Add(insertion);
    pickerRow->SetWrap(true, 8.0f);
    picker->Add(pickerRow);
    picker->Add(search);
    auto symbols = std::make_shared<SymbolCanvas>();
    auto targetStyle = BodyStyle();
    targetStyle.size = 20;
    auto targetPreview = std::make_shared<Label>(L"", targetStyle);
    targetPreview->SetMinHeight(32);
    targetPreview->SetMaxHeight(58);
    state->pickerTarget = targetPreview.get();
    picker->Add(targetPreview);
    auto openChart = std::make_shared<Button>(L"打开 IPA Plus 音标表");
    openChart->SetOnClick([state, symbols] { state->host->ScrollIntoView(*symbols); });
    edit->Add(openChart);
    auto backToEditor = std::make_shared<Button>(L"返回按键编辑");
    backToEditor->SetOnClick([state] { state->host->ScrollIntoView(*state->output); });
    pickerRow->Add(backToEditor);
    auto category = std::make_shared<Dropdown>(Options({{"all", L"全部分类"}}));
    picker->Add(category);
    try
    {
        std::ifstream input(PhoneticIpa::ResourceRoot() / "plus-catalog.json", std::ios::binary);
        auto catalog = nlohmann::json::parse(input);
        symbols->Load(catalog);
    }
    catch (const std::exception &e)
    {
        picker->Add(MakeDescription(L"音标表未加载：" + Widen(e.what())));
    }
    symbols->choose = [state](const Symbol &s) { state->InsertSymbol(s); };
    std::weak_ptr<SymbolCanvas> weakSymbols = symbols;
    std::weak_ptr<Dropdown> weakCategory = category;
    auto updateCategories = [weakSymbols, weakCategory] {
        auto s = weakSymbols.lock();
        auto dropdown = weakCategory.lock();
        if (!s || !dropdown)
            return;
        std::vector<DropdownOption> choices{{L"显示全部分区", "all"}};
        if (s->charts.contains(s->system))
            for (const auto &chart : s->charts.at(s->system))
                choices.push_back({Widen(chart.at("title")), chart.at("id")});
        dropdown->SetOptions(choices);
        dropdown->SetValue("all");
        s->section = "all";
        s->Filter();
    };
    std::vector<std::pair<std::weak_ptr<Button>, std::string>> weakTabs;
    for (const auto &[tab, value] : systemTabs)
        weakTabs.push_back({tab, value});
    for (const auto &[tab, value] : systemTabs)
        tab->SetOnClick([weakSymbols, updateCategories, weakTabs, value] {
            if (auto s = weakSymbols.lock())
            {
                s->system = value;
                updateCategories();
            }
            for (const auto &[weak, key] : weakTabs)
                if (auto button = weak.lock())
                    button->SetActive(key == value);
        });
    auto zoomLabel = MakeDescription(L"适合宽度");
    symbols->zoomChanged = [zoomLabel](float zoom, bool fit) {
        zoomLabel->SetText((fit ? L"适合宽度 · " : L"") + std::to_wstring(static_cast<int>(std::lround(zoom * 100))) +
                           L"%");
    };
    auto fitButton = std::make_shared<Button>(L"适合宽度");
    fitButton->SetOnClick([weakSymbols] {
        if (auto s = weakSymbols.lock())
            s->FitWidth();
    });
    pickerRow->Add(fitButton);
    for (const auto &[label, delta] : std::vector<std::pair<std::wstring, float>>{{L"字号 −", -.1f}, {L"字号 +", .1f}})
    {
        auto button = std::make_shared<Button>(label);
        button->SetOnClick([weakSymbols, zoomLabel, delta] {
            if (auto s = weakSymbols.lock())
            {
                s->SetZoom(s->zoom + delta);
                zoomLabel->SetText(std::to_wstring(static_cast<int>(std::lround(s->zoom * 100))) + L"%");
            }
        });
        pickerRow->Add(button);
    }
    pickerRow->Add(zoomLabel);
    category->SetOnChanged([weakSymbols](size_t, const DropdownOption &o) {
        if (auto s = weakSymbols.lock())
        {
            s->section = o.value;
            s->Filter();
        }
    });
    updateCategories();
    auto symbolInfo = MakeDescription(L"悬停查看名称与实际输出。滚轮浏览，Shift+滚轮横向移动；点击符号写入当前按键。");
    symbolInfo->SetMinHeight(48);
    std::weak_ptr<Label> weakSymbolInfo = symbolInfo;
    symbols->hover = [weakSymbolInfo](const Symbol &s) {
        if (auto label = weakSymbolInfo.lock())
            label->SetText(s.name + L"  " + Widen(s.output) + L"\n" + CodePoints(s.output));
    };
    insertion->SetOnChanged([state](size_t, const DropdownOption &o) { state->append = o.value == "append"; });
    search->SetOnChanged([weakSymbols](const std::wstring &q) {
        if (auto s = weakSymbols.lock())
        {
            s->query = q;
            s->Filter();
        }
    });
    picker->Add(symbols);
    auto paging = std::make_shared<Row>(8.0f);
    for (const auto &[label, direction] :
         std::vector<std::pair<std::wstring, int>>{{L"向左移动", -1}, {L"向右移动", 1}})
    {
        auto button = std::make_shared<Button>(label);
        button->SetOnClick([weakSymbols, direction] {
            if (auto s = weakSymbols.lock())
                s->NextHorizontal(direction);
        });
        paging->Add(button);
    }
    for (const auto &[label, direction] :
         std::vector<std::pair<std::wstring, int>>{{L"向上翻页", -1}, {L"向下翻页", 1}})
    {
        auto button = std::make_shared<Button>(label);
        button->SetOnClick([weakSymbols, direction] {
            if (auto s = weakSymbols.lock())
                s->Next(direction);
        });
        paging->Add(button);
    }
    picker->Add(paging);
    picker->Add(symbolInfo);
    picker->Add(MakeDescription(L"沿用 PhoneticToolbox M17 的 IPA、extIPA、VoQS "
                                L"目录。圆圈等显示辅助符号不会写入输出。字体及来源许可随本地资源提供。"));
    page.AddSection(edit);
    page.AddSection(picker);
    state->SelectCode("a");
    return page.Build();
}
VisualPtr BuildIpaInputPage(PageContext &context)
{
    PageBuilder page(L"输入与转写");
    auto input = std::make_shared<Column>(12.0f);
    input->Add(MakeTitle(L"默认用拼音输入国际音标"));
    input->Add(
        MakeDescription(L"输入拼音后，按数字键或空格上屏对应音标及声调。遇到多音字，先选字，再按数字选择带调读音。"));
    input->Add(MakeDescription(
        L"Caps Lock：切换音标／汉字输出。单按 Shift：切换拼音／IPA 音标编码。Shift＋1–5：输入五度调值。"));
    input->Add(MakeRow(L"每页候选", L"可选 3–9 项。",
                       BindDropdown(context, "appearance.page_size", NumberOptions(3, 9, L" 项"), Dropdown::Options{},
                                    ValueKind::Integer)));
    page.AddSection(input);
    auto rules = std::make_shared<Column>(12.0f);
    rules->Add(MakeTitle(L"国际音标转写规则"));
    try
    {
        std::vector<DropdownOption> options;
        for (const auto &rule : phonetic_ipa::LoadTranscriptionRules(PhoneticIpa::ResourceRoot()))
            options.push_back({Widen(rule.name), rule.id});
        auto selector = std::make_shared<Dropdown>(options);
        selector->SetValue(PhoneticIpa::RuleId());
        auto status = MakeDescription(L"也可点击悬浮工具栏上的规则名称快捷切换。切换后重新计算当前候选。");
        selector->SetOnChanged([status](size_t, const DropdownOption &option) {
            try
            {
                PhoneticIpa::SetRuleId(option.value);
                PostMessageW(HWND_BROADCAST, WM_IPA_RULE_CHANGED, 0, 0);
                status->SetText(L"已选择：" + option.label);
            }
            catch (const std::exception &e)
            {
                status->SetText(Widen(e.what()));
            }
        });
        rules->Add(selector);
        rules->Add(status);
    }
    catch (const std::exception &e)
    {
        rules->Add(MakeDescription(Widen(e.what())));
    }
    rules->Add(MakeDescription(L"各规则保留原字表的符号与声调。来源和列名随资源包保存于 Rules-SOURCES.json。"));
    page.AddSection(rules);
    return page.Build();
}

VisualPtr BuildIpaHelpPage(PageContext &context)
{
    PageBuilder page(L"使用说明");
    auto guide = std::make_shared<Column>(14.0f);
    auto intro = std::make_shared<Button>(L"重新查看上手教学");
    intro->SetOnClick([host = &context.host]() { ShowIpaFirstUse(*host); });
    guide->Add(intro);
    auto webGuide = std::make_shared<Button>(L"打开完整图文教程（网页）");
    webGuide->SetOnClick([model = &context.model]() {
        model->Post("openExternalUrl", Json("https://www.phonetictoolbox.com/ipa-ime-guide/?platform=windows"));
    });
    guide->Add(webGuide);
    for (size_t i = 0; i < IpaGuideTitles().size(); ++i)
    {
        guide->Add(MakeTitle(IpaGuideTitles()[i]));
        guide->Add(MakeDescription(IpaGuideDetails()[i]));
    }
    page.AddSection(guide);
    return page.Build();
}
} // namespace native_settings
