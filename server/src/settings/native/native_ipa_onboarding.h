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
        const std::array<const wchar_t *, 4> top = {L"h  →  1  →  2", L"1 好 · hao3     2 好 · hao4",
                                                    L"Shift + 2   1   4", L"IPA Plus  ·  音标表"};
        const std::array<const wchar_t *, 4> ipa = {L"xau˥˩", L"xau˨˩˧       xau˥˩", L"˨˩˦    ²¹⁴    214",
                                                    L"p   t   k   ʔ   ◌̃"};
        TextStyle style = BodyStyle();
        style.size = 17.0f;
        DrawTextInRect(dr, top[page], style, {bounds_.x + 16, bounds_.y + 20, bounds_.width - 104, 44}, Hex(0xE3EAFF));
        style.family = L"Doulos SIL";
        style.size = 27.0f;
        DrawTextInRect(dr, ipa[page], style, {bounds_.x + 16, bounds_.y + 80, bounds_.width - 32, 56}, Hex(0x5B9BFF));
    }
};

inline bool IpaFirstUsePending()
{
    std::error_code error;
    return !std::filesystem::exists(PhoneticIpa::ResourceRoot() / "onboarding-v1.done", error);
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
    const auto update = [index, title, figure, detail, weakBack = std::weak_ptr<Button>(back),
                         weakNext = std::weak_ptr<Button>(next)]() {
        const std::array<const wchar_t *, 4> titles = {L"拼音 → 音标／汉字", L"遇到多音字，再选读音", L"编码与五度调值",
                                                       L"按自己的习惯编辑键位"};
        const std::array<const wchar_t *, 4> texts = {
            L"默认输出音标。输入拼音，数字键或空格选择候选。Caps Lock "
            L"亮起时输出汉字。点击候选的汉字或下方音标也可分别输入。",
            L"例如输入 h，按 1 选择好，再按 1 或 2 选择 hao3／hao4。词语能确定读音时使用词语读音。Ctrl＋Shift＋Enter "
            L"返回选字。",
            L"单按 Shift 切换拼音与音标编码。先确认当前候选，再按住 Shift，依次按 2、1、4 "
            L"输入调值，空格确认。反斜杠键收录原数字键上的声调符号。",
            L"点击键位编辑候选，用 IPA Plus "
            L"替换或追加符号。悬浮工具栏可切换转写规则。升级后请保存草稿并重新打开仍在运行的应用，让它们加载新版输入法"
            L"。使用说明中可重新打开教学。"};
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
            PhoneticIpa::WriteProfileFile(PhoneticIpa::ResourceRoot() / "onboarding-v1.done", "1\n");
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
