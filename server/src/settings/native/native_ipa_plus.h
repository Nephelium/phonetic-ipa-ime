#pragma once
#include "settings/native/native_settings_pages.h"
#include "msimeui/Window.h"
#include <map>

namespace native_settings
{
struct Symbol
{
    std::wstring display, name;
    std::string output, system, section, id;
};

// The catalog owns the chart positions and symbol identities. This canvas only
// lays them out and dispatches insertText from the exact clicked entry.
class SymbolCanvas : public msimeui::Visual
{
  public:
    struct Tile
    {
        size_t symbol;
        RectF rect;
    };
    std::vector<Symbol> symbols, visible;
    std::function<void(const Symbol &)> choose, hover;
    std::string system = "ipa", section = "all";
    std::wstring query;
    nlohmann::json charts;
    float zoom = .7f;
    std::function<void(float, bool)> zoomChanged;
    bool IsFocusable() const override
    {
        return true;
    }
    void Load(const nlohmann::json &catalog)
    {
        symbols.clear();
        symbolWidths_.clear();
        charts = catalog.at("charts");
        for (const auto &e : catalog.at("entries"))
            symbols.push_back({Widen(e.at("display")), Widen(e.at("nameZh")), e.at("insertText"), e.at("system"),
                               e.at("section"), e.at("id")});
        Filter();
    }
    void Filter()
    {
        pressed_ = hovered_ = -1;
        scrollX_ = scrollY_ = 0;
        visible.clear();
        for (const auto &s : symbols)
            if (s.system == system && (section == "all" || charts.contains(system) || s.section == section) &&
                (query.empty() || s.name.find(query) != std::wstring::npos ||
                 s.display.find(query) != std::wstring::npos))
                visible.push_back(s);
        Layout();
        InvalidateVisual();
    }
    void SetZoom(float value)
    {
        fitWidth_ = false;
        zoom = std::clamp(value, .2f, 1.2f);
        Filter();
    }
    void FitWidth()
    {
        fitWidth_ = true;
        Filter();
    }
    float ContentWidth() const
    {
        return contentWidth_;
    }
    void ScrollHorizontal(float x)
    {
        pressed_ = hovered_ = -1;
        scrollX_ = std::clamp(x, 0.f, std::max(0.f, contentWidth_ - bounds_.width));
        InvalidateVisual();
    }
    void NextHorizontal(int direction)
    {
        ScrollHorizontal(scrollX_ + direction * bounds_.width * .7f);
    }
    SizeF Measure(const SizeF &available) override
    {
        return {available.width, 650};
    }
    void Arrange(const RectF &rect) override
    {
        if (bounds_.width != rect.width || bounds_.height != rect.height)
            pressed_ = hovered_ = -1;
        bounds_ = rect;
        Layout();
    }
    const std::vector<Tile> &Tiles() const
    {
        return tiles_;
    }
    float SectionTop(const std::string &id) const
    {
        const auto i = chartOffsets_.find(id);
        return i == chartOffsets_.end() ? 0.f : i->second;
    }
    float ContentHeight() const
    {
        return contentHeight_;
    }
    RectF Cell(size_t i) const
    {
        if (i >= tiles_.size())
            return {};
        const auto r = tiles_[i].rect;
        return {bounds_.x + r.x - scrollX_, bounds_.y + r.y - scrollY_, r.width, r.height};
    }
    void ScrollTo(float y)
    {
        pressed_ = hovered_ = -1;
        scrollY_ = std::clamp(y, 0.0f, std::max(0.0f, contentHeight_ - bounds_.height));
        InvalidateVisual();
    }
    void Next(int direction)
    {
        ScrollTo(scrollY_ + direction * (bounds_.height - 48));
    }
    bool OnMouseWheel(const POINT &, short delta, WPARAM keys) override
    {
        pressed_ = hovered_ = -1;
        if (keys & MK_SHIFT)
            scrollX_ = std::clamp(scrollX_ - delta / 120.f * 100, 0.f, std::max(0.f, contentWidth_ - bounds_.width));
        else
            ScrollTo(scrollY_ - delta / 120.f * 100);
        InvalidateVisual();
        return true;
    }
    bool OnMouseDown(const POINT &p, WPARAM) override
    {
        pressed_ = Hit(p);
        return pressed_ >= 0;
    }
    bool OnMouseUp(const POINT &p, WPARAM) override
    {
        const int index = Hit(p);
        if (index >= 0 && index == pressed_ && choose)
            choose(visible[tiles_[index].symbol]);
        pressed_ = -1;
        return index >= 0;
    }
    bool OnMouseMove(const POINT &p, WPARAM) override
    {
        const int index = Hit(p);
        if (hovered_ != index)
        {
            hovered_ = index;
            if (index >= 0 && hover)
                hover(visible[tiles_[index].symbol]);
            InvalidateVisual();
        }
        return index >= 0;
    }
    void OnMouseLeave() override
    {
        hovered_ = -1;
        InvalidateVisual();
    }
    HCURSOR GetCursor() const override
    {
        return LoadCursor(nullptr, IDC_HAND);
    }
    void Render(msimeui::DeviceResources &dr) override
    {
        const auto &p = CurrentPalette();
        auto target = dr.GetRenderTarget();
        target->PushAxisAlignedClip(
            D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height),
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const auto offset = [&](RectF r) {
            return RectF{bounds_.x + r.x - scrollX_, bounds_.y + r.y - scrollY_, r.width, r.height};
        };
        for (const auto &d : decorations_)
        {
            auto r = offset(d.rect);
            FillRounded(dr, r, 0, d.header ? p.accentSoft : p.controlBg);
            StrokeRounded(dr, r, 0, p.border);
            if (!d.text.empty())
            {
                auto style = BodyStyle();
                style.size = d.header ? 14.f : 12.f;
                style.weight = d.header ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
                DrawTextInRect(dr, d.text, style, {r.x + 6, r.y + 2, r.width - 12, r.height - 4}, p.textSecondary,
                               d.center);
            }
        }
        for (const auto &line : lines_)
            DrawLine(dr, {bounds_.x + line.first.x - scrollX_, bounds_.y + line.first.y - scrollY_},
                     {bounds_.x + line.second.x - scrollX_, bounds_.y + line.second.y - scrollY_}, p.textMuted, 1);
        const auto style = SymbolStyle();
        for (size_t i = 0; i < tiles_.size(); ++i)
        {
            const auto r = Cell(i);
            if (r.y + r.height < bounds_.y || r.y > bounds_.y + bounds_.height)
                continue;
            FillRounded(dr, r, 4, static_cast<int>(i) == hovered_ ? p.accentSoft : p.controlBg);
            DrawTextInRect(dr, visible[tiles_[i].symbol].display, style, r, p.text, true);
        }
        if (tiles_.empty())
            DrawTextInRect(dr, L"没有匹配的符号", BodyStyle(), {bounds_.x + 20, bounds_.y + 55, 300, 30}, p.textMuted);
        target->PopAxisAlignedClip();
        if (contentHeight_ > bounds_.height)
        {
            const float h = std::max(24.f, bounds_.height * bounds_.height / contentHeight_);
            FillRounded(dr,
                        {bounds_.x + bounds_.width - 5,
                         bounds_.y + scrollY_ / (contentHeight_ - bounds_.height) * (bounds_.height - h), 4, h},
                        2, p.textMuted);
        }
        if (contentWidth_ > bounds_.width)
        {
            const float w = std::max(24.f, bounds_.width * bounds_.width / contentWidth_);
            FillRounded(dr,
                        {bounds_.x + scrollX_ / (contentWidth_ - bounds_.width) * (bounds_.width - w),
                         bounds_.y + bounds_.height - 5, w, 4},
                        2, p.textMuted);
        }
    }

  private:
    struct Decoration
    {
        RectF rect;
        std::wstring text;
        bool header = false, center = false;
    };
    std::vector<Decoration> decorations_;
    std::vector<std::pair<PointF, PointF>> lines_;
    std::vector<Tile> tiles_;
    std::map<std::string, size_t> byId_;
    std::map<std::string, float> chartOffsets_;
    mutable std::map<std::pair<std::wstring, float>, float> symbolWidths_;
    float contentWidth_ = 1000, contentHeight_ = 0, scrollX_ = 0, scrollY_ = 0;
    int pressed_ = -1, hovered_ = -1;
    bool fitWidth_ = true;
    int Hit(const POINT &point) const
    {
        const auto p = window_->ClientPixelsToDips(point);
        if (!Contains(bounds_, p))
            return -1;
        for (size_t i = 0; i < tiles_.size(); ++i)
            if (Contains(Cell(i), p))
                return static_cast<int>(i);
        return -1;
    }
    std::vector<size_t> Ids(const nlohmann::json &ids) const
    {
        std::vector<size_t> result;
        for (const auto &id : ids)
            if (const auto found = byId_.find(id.get<std::string>()); found != byId_.end())
                result.push_back(found->second);
        return result;
    }
    TextStyle SymbolStyle() const
    {
        auto style = BodyStyle();
        style.family = L"PTB IPA Plus";
        style.size = 28 * zoom;
        return style;
    }
    float SymbolWidth(size_t index) const
    {
        const auto key = std::make_pair(visible[index].display, zoom);
        if (const auto found = symbolWidths_.find(key); found != symbolWidths_.end())
            return found->second;
        // Shape combining marks with their base. Centering each glyph in its
        // advance plus one space leaves a single space between adjacent entries.
        const float width = MeasureTextWidth(key.first + L" ", SymbolStyle());
        symbolWidths_.emplace(key, width);
        return width;
    }
    float Pack(const std::vector<size_t> &ids, RectF box, bool emit)
    {
        const float h = 42 * zoom;
        float x = box.x + 4, y = box.y + 3;
        for (size_t i : ids)
        {
            const float w = std::min(SymbolWidth(i), box.width - 8);
            if (x + w > box.x + box.width - 4 && x > box.x + 4)
            {
                x = box.x + 4;
                y += h;
            }
            if (emit)
                tiles_.push_back({i, {x, y, w, h}});
            x += w;
        }
        return std::max(h + 6, y - box.y + h + 3);
    }
    float Chart(const nlohmann::json &chart, RectF box)
    {
        chartOffsets_[chart.at("id").get<std::string>()] = box.y;
        const float head = 32;
        decorations_.push_back({{box.x, box.y, box.width, head}, Widen(chart.at("title")), true});
        float y = box.y + head;
        const auto kind = chart.at("kind").get<std::string>();
        auto labelStyle = BodyStyle();
        labelStyle.size = 12;
        labelStyle.weight = DWRITE_FONT_WEIGHT_NORMAL;
        if (kind == "matrix")
        {
            const auto &columns = chart.at("columns");
            float label = 100 * zoom;
            for (const auto &row : chart.at("rows"))
                label = std::max(label, MeasureTextWidth(Widen(row.at("label")), labelStyle) + 12);
            const float cell = (box.width - label) / columns.size();
            decorations_.push_back({{box.x, y, label, 28}, L"方式 / 部位", false, true});
            for (size_t c = 0; c < columns.size(); ++c)
                decorations_.push_back({{box.x + label + c * cell, y, cell, 28}, Widen(columns[c]), false, true});
            y += 28;
            for (const auto &row : chart.at("rows"))
            {
                float height = 42 * zoom + 6;
                for (const auto &item : row.at("cells"))
                    height = std::max(height, Pack(Ids(item.at("ids")), {0, 0, cell, 0}, false));
                decorations_.push_back({{box.x, y, label, height}, Widen(row.at("label")), false, true});
                size_t c = 0;
                for (const auto &item : row.at("cells"))
                {
                    RectF cellBox{box.x + label + c++ * cell, y, cell, height};
                    decorations_.push_back({cellBox, L""});
                    Pack(Ids(item.at("ids")), cellBox, true);
                }
                y += height;
            }
        }
        else if (kind == "vowels")
        {
            const float height = 360 * zoom + 42;
            decorations_.push_back({{box.x, y, box.width, height}, L""});
            const auto point = [&](float x, float v) {
                return PointF{box.x + x / 100 * box.width, y + 25 + v / 100 * (height - 40)};
            };
            for (const auto &segment : std::vector<std::pair<PointF, PointF>>{{{20, 9}, {42, 91}},
                                                                              {{50, 9}, {63, 91}},
                                                                              {{83, 9}, {83, 91}},
                                                                              {{20, 9}, {83, 9}},
                                                                              {{27, 37}, {83, 37}},
                                                                              {{34, 65}, {83, 65}},
                                                                              {{42, 91}, {83, 91}}})
                lines_.push_back({point(segment.first.x, segment.first.y), point(segment.second.x, segment.second.y)});
            for (const auto &[x, label] :
                 std::vector<std::pair<float, std::wstring>>{{20.f, L"前"}, {50.f, L"央"}, {83.f, L"后"}})
                decorations_.push_back({{box.x + x / 100 * box.width - 20, y, 40, 26}, label, false, true});
            for (const auto &p : chart.at("points"))
            {
                const auto ids = Ids(p.at("ids"));
                const auto center = point(p.at("x"), p.at("y"));
                float total = 0;
                for (auto i : ids)
                    total += SymbolWidth(i);
                float left = center.x - total / 2;
                for (auto i : ids)
                {
                    RectF r{left, center.y - 20 * zoom, SymbolWidth(i), 40 * zoom};
                    decorations_.push_back({r, L""}); // mask chart lines behind glyphs
                    tiles_.push_back({i, r});
                    left += r.width;
                }
            }
            y += height;
        }
        else
        {
            // Groups retain the source's semantic boundaries. VoQS charts without
            // groups are still separate titled sections.
            if (chart.contains("groups"))
            {
                float label = 145 * zoom;
                for (const auto &group : chart.at("groups"))
                    label = std::max(label, MeasureTextWidth(Widen(group.at("label")), labelStyle) + 12);
                label = std::min(label, box.width * .35f);
                for (const auto &group : chart.at("groups"))
                {
                    const auto ids = Ids(group.at("ids"));
                    if (ids.empty())
                        continue;
                    const float h = Pack(ids, {box.x + label, y, box.width - label, 0}, true);
                    decorations_.push_back({{box.x, y, label, h}, Widen(group.at("label")), false});
                    decorations_.push_back({{box.x + label, y, box.width - label, h}, L""});
                    y += h;
                }
            }
            else
            {
                const float h = Pack(Ids(chart.at("ids")), {box.x + 8, y + 6, box.width - 16, 0}, true) + 12;
                decorations_.push_back({{box.x, y, box.width, h}, L""});
                y += h;
            }
        }
        return y - box.y;
    }
    void Layout()
    {
        if (fitWidth_ && bounds_.width > 12)
            zoom = std::clamp((bounds_.width - 12) / 1080.f, .2f, .7f);
        if (zoomChanged)
            zoomChanged(zoom, fitWidth_);
        tiles_.clear();
        decorations_.clear();
        lines_.clear();
        byId_.clear();
        chartOffsets_.clear();
        for (size_t i = 0; i < visible.size(); ++i)
            byId_[visible[i].id] = i;
        contentWidth_ = std::max(bounds_.width - 12, 1080 * zoom);
        float y = 0;
        if (charts.contains(system))
        {
            std::vector<nlohmann::json> selected;
            for (const auto &chart : charts.at(system))
                if (section == "all" || chart.at("id") == section)
                    selected.push_back(chart);
            float columns[2] = {y, y};
            for (size_t i = 0; i < selected.size(); ++i)
            {
                if (selected[i].at("kind") == "matrix" || section != "all")
                {
                    y = std::max(columns[0], columns[1]);
                    y += Chart(selected[i], {0, y, contentWidth_, 0}) + 16;
                    columns[0] = columns[1] = y;
                }
                else
                {
                    const float width = (contentWidth_ - 16) / 2;
                    const int column =
                        system == "ipa" && selected[i].at("id") == "tones" ? 1 : (columns[0] <= columns[1] ? 0 : 1);
                    columns[column] += Chart(selected[i], {(width + 16) * column, columns[column], width, 0}) + 16;
                }
            }
            y = std::max(columns[0], columns[1]);
        }
        else
        {
            std::vector<size_t> ids;
            for (size_t i = 0; i < visible.size(); ++i)
                ids.push_back(i);
            y = Pack(ids, {0, 0, contentWidth_, 0}, true);
        }
        contentHeight_ = y;
        scrollY_ = std::clamp(scrollY_, 0.f, std::max(0.f, y - bounds_.height));
        scrollX_ = std::clamp(scrollX_, 0.f, std::max(0.f, contentWidth_ - bounds_.width));
    }
};
} // namespace native_settings
