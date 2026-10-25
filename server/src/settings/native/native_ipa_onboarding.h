#pragma once

#include "settings/native/native_settings_pages.h"
#include "ipa/ipa_profile.h"
#include "ipa/ipa_runtime.h"
#include "msimeui/DeviceResources.h"
#include <array>

namespace native_settings
{
// A native diagram using the existing bigfish skin assets and their original attribution.
class IpaFirstUseFigure : public msimeui::Visual
{
  public:
    int page = 0;
    SizeF Measure(const SizeF &available) override
    {
        return {available.width, 160.0f};
    }
    void Arrange(const RectF &rect) override
    {
        bounds_ = rect;
    }
    void Render(msimeui::DeviceResources &dr) override
    {
        FillRounded(dr, bounds_, 12.0f, Hex(0x141B33));
        auto target = dr.GetRenderTarget();
        const auto skin = PhoneticIpa::ResourceRoot().parent_path() / "skins" / "bigfish" / "assets";
        if (auto background = dr.GetBitmapFromFile((skin / "background.png").wstring()))
            target->DrawBitmap(background, ToD2D(bounds_), 0.22f);
        if (auto character = dr.GetBitmapFromFile((skin / "character.png").wstring()))
            target->DrawBitmap(character, D2D1::RectF(bounds_.x + bounds_.width - 86.0f, bounds_.y + 12.0f,
                                                      bounds_.x + bounds_.width - 10.0f, bounds_.y + 94.0f));
        const std::array<const wchar_t *, 4> top = {L"拼音 ⇄ IPA · Caps Lock 切换输出", L"hao → 选好 → 选 hao4",
                                                    L"单按 Shift → IPA · a → 1", L"按住 Shift + 2、1、4 → 空格"};
        const std::array<const wchar_t *, 4> ipa = {L"拼音查字     直接音标", L"xau˥˩     /     好", L"a",
                                                    L"˨˩˦    ²¹⁴    214"};
        TextStyle style = BodyStyle();
        style.size = 17.0f;
        DrawTextInRect(dr, top[page], style, {bounds_.x + 16, bounds_.y + 20, bounds_.width - 104, 44}, Hex(0xE3EAFF));
        style.family = L"PTB IPA Plus";
        style.size = 27.0f;
        DrawTextInRect(dr, ipa[page], style, {bounds_.x + 16, bounds_.y + 80, bounds_.width - 32, 56}, Hex(0x5B9BFF));
    }
};

inline const std::array<const wchar_t *, 4> &IpaGuideTitles()
{
    static const std::array<const wchar_t *, 4> values = {L"先认清当前输入状态", L"用拼音输入音标或汉字",
                                                          L"切换到直接音标键盘", L"输入声调，查找更多符号"};
    return values;
}

inline const std::array<const wchar_t *, 4> &IpaGuideDetails()
{
    static const std::array<const wchar_t *, 4> values = {
        L"① 安装后按 Win＋空格，从系统输入法列表选择国际音标输入法。先点进要打字的输入框，再输入。\n\n② "
        L"悬浮工具栏左侧显示当前状态：默认输入拼音，选择候选后输出音标。按 Caps Lock "
        L"切到汉字输出，继续输入拼音即可选择汉字。单按并松开 Shift，进入 IPA 模式，用字母或符号查找音"
        L"标。\n\n③ 轻按并松开 Shift，切换拼音查字与直接音标。按 Caps "
        L"Lock，在拼音查字时切换 "
        L"IPA／汉字输出。两个开关分工不同：先决定怎么输入，再决定拼音候选输出什么。新安装默认拼音查字、输出 "
        L"IPA，已有偏好会保留。",
        L"① 单按 Shift 返回拼音查字，Caps Lock 关闭时工具栏显示 IPA。输入 hao，候选上行是汉字，下行是音标。\n\n② "
        L"找到“好”，按它前面的数字；若“好”标为 1，就按 1。出现 hao3、hao4 读音页后，再按所需读音前的数字。选 hao4 "
        L"后输入 xau˥˩。单一读音直接输入，有多个读音才再选一次，编号以当前页为准。\n\n③ 想输入汉字：在拼音模式按 Caps "
        L"Lock，让工具栏显示“汉字”，输入 hao，再选“好”，结果就是“好”。再按 Caps Lock 返回 IPA。\n\n④ "
        L"点上行汉字或下行音标也能分别输入。空格选当前高亮项，上下键移动，Page Up／Page Down 翻页，Esc "
        L"取消当前组合。读音页用 Ctrl＋Shift＋Enter 返回选字。完整词语 yanwu 可直接选择厌恶的词语读音。",
        L"① 轻按并松开 Shift，直到工具栏显示 IPA，即直接音标键盘。此时字母用作音标编码。按 a 会出现 a 的音标候选，再按 "
        L"1 或空格确认首项 a。\n\n② "
        L"候选窗口还在、编码仍在组合时，表示尚未完成输入。按数字选所需音标后才确认上屏。每个键的备选可以在设置的“音标键"
        L"盘”页查看和修改。\n\n③ 再单按 Shift，返回拼音查字。随后用 Caps Lock 选择默认输出 IPA 或汉字。单按 Shift "
        L"是切换模式；按住 Shift 配合数字是输入声调。\n\n④ /、\\ 等标点也能作为音标编码。\\ 收录声调符号，/ "
        L"默认输出斜杠。含数字的连续编码可用 code: 前缀。Esc 可取消未确认的编码。",
        L"① 先让当前汉字或音标上屏。按住 Shift，依次按数字 2、1、4，再松开 Shift。数字 1 最低、5 "
        L"最高，顺序组成调型。\n\n② 候选栏显示 tone:214 时，表示调值尚待确认。按空格选首项 ˨˩˦，也可按候选前的数字选择 "
        L"²¹⁴ 或 214。这里的空格用于确认，再按一次才输入空格。\n\n③ 工具栏的规则名称可切换十套拼音转写方案。设置里的 "
        L"IPA Plus 可浏览 IPA、extIPA、VoQS 全部符号并编辑键位。\n\n④ "
        L"设置→使用说明可重新打开教学。升级后请保存草稿，完全退出并重开仍在运行的微信、ChatGPT "
        L"等应用，让它们加载新版输入法。"};
    return values;
}

inline bool IpaFirstUsePending()
{
    std::error_code error;
    return !std::filesystem::exists(PhoneticIpa::ResourceRoot() / "onboarding-v2.done", error);
}

inline void ShowIpaFirstUse(HostServices &host)
{
    const auto index = std::make_shared<int>(0);
    auto column = std::make_shared<Column>(12.0f);
    auto title = MakeTitle(L"国际音标输入法 · 开始使用");
    column->Add(title);
    auto figure = std::make_shared<IpaFirstUseFigure>();
    auto detail = MakeDescription(L"");
    auto content = std::make_shared<Column>(14.0f);
    content->Add(figure);
    content->Add(detail);
    auto scroll = std::make_shared<msimeui::ScrollViewer>(content);
    scroll->SetHeight(280.0f);
    column->Add(scroll);
    auto actions = std::make_shared<Row>(10.0f);
    auto back = std::make_shared<Button>(L"上一步");
    auto next = std::make_shared<Button>(L"下一步", ButtonKind::Primary);
    auto later = std::make_shared<Button>(L"稍后");
    actions->Add(back);
    actions->Add(later);
    actions->Add(next);
    column->Add(actions);
    const auto update = [index, title, figure, detail, scroll, weakBack = std::weak_ptr<Button>(back),
                         weakNext = std::weak_ptr<Button>(next)]() {
        const auto &titles = IpaGuideTitles();
        const auto &texts = IpaGuideDetails();
        scroll->SetScrollOffset(0.0f);
        title->SetText(std::wstring(titles[*index]) + L"   " + std::to_wstring(*index + 1) + L" / 4");
        figure->page = *index;
        figure->InvalidateVisual();
        detail->SetText(texts[*index]);
        if (auto button = weakBack.lock())
            button->SetDisabled(*index == 0);
        if (auto button = weakNext.lock())
            button->SetText(*index == 3 ? L"开始使用" : L"下一步");
    };
    const auto close = [&host]() {
        try
        {
            PhoneticIpa::WriteProfileFile(PhoneticIpa::ResourceRoot() / "onboarding-v2.done", "1\n");
        }
        catch (const std::exception &)
        {
            host.ShowToast(L"教学完成记录未保存，可在使用说明中重新打开。", false);
        }
        host.CloseModal();
    };
    back->SetOnClick([index, update]() {
        if (*index > 0)
        {
            --*index;
            update();
        }
    });
    next->SetOnClick([index, update, close]() {
        if (*index < 3)
        {
            ++*index;
            update();
        }
        else
            close();
    });
    later->SetOnClick(close);
    update();
    host.ShowModal(column);
}
} // namespace native_settings
