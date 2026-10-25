#pragma once

#include "ipa/ipa_keyboard_layout.h"
#include "settings/native/native_settings_controls.h"
#include "msimeui/Window.h"
#include <map>

namespace native_settings
{
inline std::wstring IpaPreviewText(const std::wstring &text)
{
    WORD kind = 0;
    if (!text.empty() && GetStringTypeW(CT_CTYPE3, text.data(), 1, &kind) && (kind & C3_NONSPACING))
        return L"◌" + text;
    return text;
}
class IpaPageHero : public msimeui::Visual
{
  public:
    SizeF Measure(const SizeF &available) override
    {
        return {available.width, 96};
    }
    void Arrange(const RectF &rect) override
    {
        bounds_ = rect;
    }
    void Render(msimeui::DeviceResources &dr) override
    {
        const auto &p = CurrentPalette();
        FillRounded(dr, bounds_, 18, p.light ? Hex(0xE9EEF7) : Hex(0x202D42));
        auto style = BodyStyle();
        style.size = 13;
        style.weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
        style.size = 25;
        DrawTextInRect(dr, L"音标键盘工作台", style, {bounds_.x + 24, bounds_.y + 14, bounds_.width - 48, 36}, p.text);
        style.size = 15;
        style.weight = DWRITE_FONT_WEIGHT_NORMAL;
        DrawTextInRect(dr, L"选一个键，编辑音标；从 IPA Plus 点选，也能保存和分享整套键位。", style,
                       {bounds_.x + 26, bounds_.y + 59, bounds_.width - 52, 24}, p.textSecondary);
        if (bounds_.width > 1080)
        {
            style.family = L"PTB IPA Plus";
            style.size = 50;
            DrawTextInRect(dr, L"a ɐ ɑ", style, {bounds_.x + bounds_.width - 240, bounds_.y + 12, 215, 70},
                           p.light ? Hex(0x667DA6) : Hex(0x99B5E0), true);
        }
    }
};

// The floating card is paint only. It never changes hit testing: every pointer
// coordinate still refers to the original keyboard, including covered neighbors.
class IpaKeyboardCanvas : public msimeui::Visual
{
  public:
    std::string size = "104";
    unsigned selected = 'A', modifiers = 0;
    std::function<std::wstring(unsigned)> summary;
    std::function<std::vector<std::wstring>(unsigned)> outputs;
    std::function<std::vector<std::wstring>(unsigned)> outputLabels;
    std::function<void(unsigned)> hover, choose;
    bool IsFocusable() const override
    {
        return true;
    }
    SizeF Measure(const SizeF &available) override
    {
        return {available.width, size == "60" || size == "65" ? 326.0f : 410.0f};
    }
    void Arrange(const RectF &rect) override
    {
        if (arrangedSize_ != size || bounds_.x != rect.x || bounds_.y != rect.y || bounds_.width != rect.width)
        {
            hovered_ = -1;
            pressed_ = 0;
            previewPage_ = 0;
        }
        bounds_ = rect;
        arrangedSize_ = size;
        keys_ = PhoneticIpa::KeyboardKeys(size);
    }
    RectF Rect(const PhoneticIpa::KeyCap &key) const
    {
        const auto r = PhoneticIpa::KeyBounds(key, size, bounds_.width);
        return {bounds_.x + r.x, bounds_.y + r.y, r.width, r.height};
    }
    unsigned HoveredKey() const
    {
        return hovered_ < 0 ? 0 : keys_[hovered_].vk;
    }
    size_t PreviewPage() const
    {
        return previewPage_;
    }
    struct PreviewGeometry
    {
        RectF box{};
        size_t columns = 1, capacity = 1;
        float rowHeight = 58;
    };
    PreviewGeometry Preview() const
    {
        PreviewGeometry g;
        if (hovered_ < 0 || !outputs)
            return g;
        const auto values = outputs(HoveredKey());
        if (values.empty())
            return g;
        size_t longest = 1;
        for (const auto &v : values)
            longest = std::max(longest, v.size());
        const auto labels = outputLabels ? outputLabels(HoveredKey()) : std::vector<std::wstring>{};
        const bool combined =
            std::any_of(labels.begin(), labels.end(), [](const auto &label) { return !label.empty(); });
        g.rowHeight = combined ? 76.f : 58.f;
        const float cell = std::clamp(26.0f + longest * 22.0f, combined ? 132.0f : 76.0f, 240.0f);
        const float maxWidth = std::min(700.0f, bounds_.width - 16);
        g.columns =
            std::max(size_t{1}, std::min(values.size(), static_cast<size_t>(std::max(1.0f, (maxWidth - 32) / cell))));
        const size_t rows =
            std::max(size_t{1}, static_cast<size_t>(std::max(1.0f, (bounds_.height - 82) / g.rowHeight)));
        g.capacity = rows * g.columns;
        const size_t usedRows = std::min(rows, (values.size() + g.columns - 1) / g.columns);
        const float w = std::min(maxWidth, std::max(300.0f, cell * g.columns + 32));
        const float h = usedRows * g.rowHeight + 68;
        const auto key = Rect(keys_[hovered_]);
        g.box = {std::clamp(key.x - 28, bounds_.x + 8, bounds_.x + bounds_.width - w - 8),
                 std::clamp(key.y - 42, bounds_.y + 6, bounds_.y + bounds_.height - h - 6), w, h};
        return g;
    }
    void Render(msimeui::DeviceResources &dr) override
    {
        const auto &p = CurrentPalette();
        for (size_t i = 0; i < keys_.size(); ++i)
        {
            const auto &key = keys_[i];
            auto rect = Rect(key);
            const bool active = key.vk == selected;
            FillRounded(dr, {rect.x, rect.y + 3, rect.width, rect.height}, 8, p.light ? Hex(0xDCE0E8) : Hex(0x151A22));
            FillRounded(dr, rect, 8, active ? p.accentSoft : p.light ? Hex(0xFFFFFF) : p.controlBg);
            StrokeRounded(dr, rect, 8, active ? p.accent : p.border);
            auto label = key.label;
            if (modifiers == 1)
            {
                const auto code = PhoneticIpa::KeyCode(key.vk, true);
                if (!code.empty())
                    label = PhoneticIpa::KeyLegend(code);
            }
            const auto values = outputs ? outputs(key.vk) : std::vector<std::wstring>{};
            auto style = CaptionStyle();
            style.size = 12;
            DrawTextInRect(dr, label, style, {rect.x + 6, rect.y + 4, rect.width - 12, 18}, p.textSecondary);
            if (values.size() > 1 && rect.width > 40)
            {
                style.size = 10;
                DrawTextInRect(dr, std::to_wstring(values.size()), style,
                               {rect.x + rect.width - 23, rect.y + 5, 18, 16}, p.textMuted, true);
            }
            style = BodyStyle();
            style.family = L"PTB IPA Plus";
            style.size = 28;
            DrawTextInRect(dr, summary ? summary(key.vk) : L"", style,
                           {rect.x + 3, rect.y + 22, rect.width - 6, rect.height - 23}, p.text, true);
        }
        RenderPreview(dr);
    }
    bool OnMouseMove(const POINT &point, WPARAM) override
    {
        const int next = Hit(point);
        if (next != hovered_)
        {
            hovered_ = next;
            previewPage_ = 0;
            entrance_ = msimeui::Tween(0.0f);
            entrance_.AnimateTo(1, 145, msimeui::Easing::EaseOut(), msimeui::NowMs());
            if (next >= 0 && hover)
                hover(keys_[next].vk);
            RefreshVisual();
        }
        return next >= 0;
    }
    void OnMouseLeave() override
    {
        hovered_ = -1;
        RefreshVisual();
    }
    bool OnMouseDown(const POINT &point, WPARAM) override
    {
        const auto hit = Hit(point);
        pressed_ = hit >= 0 ? keys_[hit].vk : 0;
        return hit >= 0;
    }
    bool OnMouseUp(const POINT &point, WPARAM) override
    {
        const auto hit = Hit(point);
        const auto pressed = pressed_;
        pressed_ = 0;
        if (hit >= 0 && keys_[hit].vk == pressed && choose)
            choose(pressed);
        return hit >= 0;
    }
    bool OnMouseWheel(const POINT &, short delta, WPARAM) override
    {
        if (!HoveredKey() || !outputs || delta == 0)
            return false;
        const auto count = outputs(HoveredKey()).size(), capacity = Preview().capacity;
        if (count <= capacity)
            return false;
        const size_t pages = (count + capacity - 1) / capacity;
        previewPage_ = delta < 0 ? (previewPage_ + 1) % pages : (previewPage_ + pages - 1) % pages;
        RefreshVisual();
        return true;
    }
    HCURSOR GetCursor() const override
    {
        return LoadCursor(nullptr, IDC_HAND);
    }
    void Refresh()
    {
        InvalidateMeasure();
        RefreshVisual();
    }

  private:
    std::string arrangedSize_;
    std::vector<PhoneticIpa::KeyCap> keys_;
    unsigned pressed_ = 0;
    int hovered_ = -1;
    size_t previewPage_ = 0;
    msimeui::Tween entrance_{1.0f};
    msimeui::ShadowCache shadow_;
    void RefreshVisual()
    {
        InvalidateVisual();
        if (window_)
            window_->RequestAnimationFrame();
    }
    int Hit(const POINT &point) const
    {
        const auto dip = window_->ClientPixelsToDips(point);
        for (size_t i = 0; i < keys_.size(); ++i)
            if (Contains(Rect(keys_[i]), dip))
                return static_cast<int>(i);
        return -1;
    }
    void RenderPreview(msimeui::DeviceResources &dr)
    {
        if (hovered_ < 0 || !outputs)
            return;
        const auto values = outputs(HoveredKey());
        const auto labels = outputLabels ? outputLabels(HoveredKey()) : std::vector<std::wstring>{};
        if (values.empty())
            return;
        const auto g = Preview();
        const auto &p = CurrentPalette();
        auto target = dr.GetRenderTarget();
        const double now = window_ ? window_->GetFrameTime() : msimeui::NowMs();
        const float progress = dr.AreAnimationsEnabled() ? entrance_.Value(now) : 1;
        if (progress < 1 && window_)
            window_->RequestAnimationFrame();
        D2D1_MATRIX_3X2_F old{};
        target->GetTransform(&old);
        const float scale = 0.92f + 0.08f * progress;
        target->SetTransform(
            D2D1::Matrix3x2F::Scale(scale, scale, {g.box.x + g.box.width / 2, g.box.y + g.box.height / 2}) * old);
        msimeui::ShadowPass pass{12, p.light ? 0.18f : 0.4f, 0, 7};
        shadow_.Render(target, g.box, 14, &pass, 1);
        FillRounded(dr, g.box, 14, p.light ? Hex(0xF9FBFF) : Hex(0x26334A));
        StrokeRounded(dr, g.box, 14, p.accent, 1.4f);
        auto style = BodyStyle();
        style.size = 16;
        style.weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
        const size_t start = std::min(previewPage_ * g.capacity, values.size() - 1);
        const size_t end = std::min(start + g.capacity, values.size());
        std::wstring heading = keys_[hovered_].label + L"  ·  " + std::to_wstring(values.size()) + L" 个音标";
        if (values.size() > g.capacity)
            heading += L"   " + std::to_wstring(start + 1) + L"–" + std::to_wstring(end) + L"  滚轮翻页";
        DrawTextInRect(dr, heading, style, {g.box.x + 16, g.box.y + 12, g.box.width - 32, 27}, p.text);
        style.family = L"PTB IPA Plus";
        style.size = 34;
        style.weight = DWRITE_FONT_WEIGHT_NORMAL;
        const float cell = (g.box.width - 32) / g.columns;
        for (size_t i = start; i < end; ++i)
        {
            const auto index = i - start;
            const RectF r{g.box.x + 16 + (index % g.columns) * cell, g.box.y + 51 + (index / g.columns) * g.rowHeight,
                          cell - 6, g.rowHeight - 7};
            FillRounded(dr, r, 7, p.light ? Hex(0xFFFFFF) : Hex(0x1D283B));
            StrokeRounded(dr, r, 7, p.border);
            const bool combined = i < labels.size() && !labels[i].empty();
            DrawTextInRect(dr, IpaPreviewText(values[i]), style,
                           {r.x, r.y, r.width, combined ? r.height - 18 : r.height}, p.text, true);
            if (combined)
            {
                auto caption = CaptionStyle();
                caption.size = 11;
                DrawTextInRect(dr, labels[i], caption, {r.x + 2, r.y + r.height - 18, r.width - 4, 17}, p.textSecondary,
                               true);
            }
        }
        target->SetTransform(old);
    }
};
} // namespace native_settings
