#pragma once
#include "settings/native/native_ipa_keyboard.h"
#include <sstream>

namespace native_settings
{
class IpaResponsiveRow : public Row
{
  public:
    explicit IpaResponsiveRow(float gap, float breakpoint) : Row(gap), breakpoint_(breakpoint)
    {
    }
    SizeF Measure(const SizeF &available) override
    {
        const bool wrap = available.width < breakpoint_;
        if (wrap != wrapping_)
        {
            wrapping_ = wrap;
            SetWrap(wrap, 8);
        }
        return Row::Measure(available);
    }

  private:
    float breakpoint_;
    bool wrapping_ = false;
};
// Horizontal candidate cards keep punctuation and combining marks as data:
// there is no comma/semicolon delimiter for users to accidentally overwrite.
class IpaCandidateCards : public msimeui::Visual
{
  public:
    std::vector<std::wstring> values;
    size_t selected = 0, page = 0;
    std::function<void(size_t)> choose;
    size_t Columns(float width) const
    {
        return std::max(size_t{1}, static_cast<size_t>(std::max(1.f, width / 112)));
    }
    size_t Capacity() const
    {
        return Columns(bounds_.width) * 2;
    }
    SizeF Measure(const SizeF &available) override
    {
        const auto rows = std::min(size_t{2}, (std::max(size_t{1}, values.size()) + Columns(available.width) - 1) /
                                                  Columns(available.width));
        return {available.width, rows * 66.f + 24};
    }
    void Arrange(const RectF &rect) override
    {
        if (bounds_.width != rect.width)
        {
            page = selected / (Columns(rect.width) * 2);
            pressed_ = -1;
        }
        bounds_ = rect;
        page = std::min(page, values.empty() ? size_t{0} : (values.size() - 1) / Capacity());
    }
    RectF Card(size_t index) const
    {
        const size_t local = index - page * Capacity(), columns = Columns(bounds_.width);
        const float width = bounds_.width / columns;
        return {bounds_.x + (local % columns) * width, bounds_.y + (local / columns) * 66.f, width - 8, 58};
    }
    void Render(msimeui::DeviceResources &dr) override
    {
        const auto &p = CurrentPalette();
        const size_t start = page * Capacity(), end = std::min(values.size(), start + Capacity());
        for (size_t i = start; i < end; ++i)
        {
            const auto r = Card(i);
            FillRounded(dr, r, 8, i == selected ? p.accentSoft : p.controlBg);
            StrokeRounded(dr, r, 8, i == selected ? p.accent : p.border);
            auto style = CaptionStyle();
            DrawTextInRect(dr, std::to_wstring(i + 1), style, {r.x + 7, r.y + 3, 32, 15}, p.textMuted);
            style = BodyStyle();
            style.family = L"Doulos SIL";
            style.size = 30;
            const auto text = values[i].empty() ? L"…" : IpaPreviewText(values[i]);
            const float width = MeasureTextWidth(text, style);
            if (width > r.width - 12)
                style.size = std::max(16.f, style.size * (r.width - 12) / width);
            DrawTextInRect(dr, text, style, {r.x + 5, r.y + 17, r.width - 10, 38}, p.text, true);
        }
        const auto hint = values.empty() ? L"暂无输出，点击新增候选或在下方输入。"
                                         : L"第 " + std::to_wstring(start + 1) + L"–" + std::to_wstring(end) +
                                               L" 项，共 " + std::to_wstring(values.size()) + L" 项 · 点击卡片编辑" +
                                               (values.size() > Capacity() ? L" · 滚轮翻页" : L"");
        DrawTextInRect(dr, hint, CaptionStyle(), {bounds_.x, bounds_.y + bounds_.height - 22, bounds_.width, 22},
                       p.textSecondary);
    }
    bool OnMouseDown(const POINT &p, WPARAM) override
    {
        pressed_ = Hit(p);
        return pressed_ >= 0;
    }
    bool OnMouseUp(const POINT &p, WPARAM) override
    {
        const int hit = Hit(p), pressed = pressed_;
        pressed_ = -1;
        if (hit >= 0 && hit == pressed && choose)
            choose(static_cast<size_t>(hit));
        return hit >= 0;
    }
    bool OnMouseWheel(const POINT &, short delta, WPARAM) override
    {
        if (delta == 0 || values.size() <= Capacity())
            return false;
        const auto pages = (values.size() + Capacity() - 1) / Capacity();
        page = delta < 0 ? (page + 1) % pages : (page + pages - 1) % pages;
        pressed_ = -1;
        InvalidateVisual();
        return true;
    }
    void Refresh()
    {
        pressed_ = -1;
        InvalidateMeasure();
        InvalidateVisual();
    }

  private:
    int pressed_ = -1;
    int Hit(const POINT &point) const
    {
        if (!window_)
            return -1;
        const auto p = window_->ClientPixelsToDips(point);
        for (size_t i = page * Capacity(); i < std::min(values.size(), (page + 1) * Capacity()); ++i)
            if (Contains(Card(i), p))
                return static_cast<int>(i);
        return -1;
    }
};

class IpaCandidateEditor : public Column
{
  public:
    IpaCandidateEditor() : Column(8)
    {
        cards_ = std::make_shared<IpaCandidateCards>();
        Add(cards_);
        auto row = std::make_shared<IpaResponsiveRow>(8.0f, 900.0f);
        selectedLabel_ = MakeDescription(L"编辑第 1 项");
        selectedLabel_->SetMaxWidth(88);
        row->Add(selectedLabel_);
        TextField::Options options;
        options.height = 44;
        options.fontSize = 28;
        options.fontFamily = L"Doulos SIL";
        options.placeholder = L"输入音标，或从 IPA Plus 点选";
        field_ = std::make_shared<TextField>(options);
        field_->SetMinWidth(240);
        row->Add(field_, 1);
        auto action = [&](const wchar_t *text, std::function<void()> fn) {
            auto button = std::make_shared<Button>(text);
            button->SetOnClick(std::move(fn));
            row->Add(button);
        };
        action(L"＋ 新增", [this] { AddCandidate(); });
        action(L"删除此项", [this] { RemoveCandidate(); });
        action(L"← 前移", [this] { MoveCandidate(-1); });
        action(L"后移 →", [this] { MoveCandidate(1); });
        Add(row);
        cards_->choose = [this](size_t i) { Select(i); };
        field_->SetOnChanged([this](const std::wstring &value) { SetSelectedOutput(value); });
    }
    const std::vector<std::wstring> &Values() const
    {
        return cards_->values;
    }
    size_t Selected() const
    {
        return cards_->selected;
    }
    std::wstring SelectedOutput() const
    {
        return Values().empty() ? L"" : Values()[Selected()];
    }
    void SetOnChanged(std::function<void(const std::wstring &)> fn)
    {
        onChanged_ = std::move(fn);
    }
    void SetOnSelected(std::function<void()> fn)
    {
        onSelected_ = std::move(fn);
    }
    void SetText(const std::wstring &text)
    {
        cards_->values.clear();
        std::wistringstream stream(text);
        std::wstring line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == L'\r')
                line.pop_back();
            cards_->values.push_back(line);
        }
        cards_->selected = cards_->page = 0;
        Sync();
    }
    std::wstring GetText() const
    {
        std::wstring result;
        for (size_t i = 0; i < Values().size(); ++i)
        {
            if (i)
                result += L'\n';
            result += Values()[i];
        }
        return result;
    }
    void Select(size_t i)
    {
        if (i >= Values().size())
            return;
        cards_->selected = i;
        cards_->page = i / cards_->Capacity();
        Sync();
        if (onSelected_)
            onSelected_();
    }
    void SetSelectedOutput(const std::wstring &value)
    {
        if (disabled_)
            return;
        if (Values().empty())
        {
            cards_->values.push_back(value);
            cards_->selected = 0;
        }
        else
            cards_->values[Selected()] = value;
        if (field_->GetText() != value)
            field_->SetText(value);
        Changed();
    }
    void AddCandidate()
    {
        if (disabled_ || (single_ && !Values().empty()) || Values().size() >= 200)
            return;
        cards_->values.push_back(L"");
        Select(Values().size() - 1);
        Changed();
    }
    void RemoveCandidate()
    {
        if (disabled_)
            return;
        if (Values().empty())
            return;
        cards_->values.erase(cards_->values.begin() + Selected());
        cards_->selected = std::min(Selected(), Values().empty() ? size_t{0} : Values().size() - 1);
        cards_->page = Selected() / cards_->Capacity();
        Sync();
        Changed();
    }
    void MoveCandidate(int direction)
    {
        if (disabled_)
            return;
        if (Values().empty() || (direction < 0 && Selected() == 0) ||
            (direction > 0 && Selected() + 1 == Values().size()))
            return;
        const auto next = static_cast<size_t>(static_cast<int>(Selected()) + direction);
        std::swap(cards_->values[Selected()], cards_->values[next]);
        Select(next);
        Changed();
    }
    void SetSingle(bool single)
    {
        single_ = single;
    }
    void SetDisabled(bool disabled)
    {
        disabled_ = disabled;
        field_->SetDisabled(disabled);
    }

  private:
    std::shared_ptr<IpaCandidateCards> cards_;
    std::shared_ptr<TextField> field_;
    std::shared_ptr<Label> selectedLabel_;
    std::function<void(const std::wstring &)> onChanged_;
    std::function<void()> onSelected_;
    bool single_ = false;
    bool disabled_ = false;
    void Sync()
    {
        selectedLabel_->SetText(L"编辑第 " + std::to_wstring(Selected() + 1) + L" 项");
        field_->SetText(SelectedOutput());
        cards_->Refresh();
    }
    void Changed()
    {
        selectedLabel_->SetText(L"编辑第 " + std::to_wstring(Selected() + 1) + L" 项");
        cards_->Refresh();
        if (onChanged_)
            onChanged_(GetText());
    }
};
} // namespace native_settings
