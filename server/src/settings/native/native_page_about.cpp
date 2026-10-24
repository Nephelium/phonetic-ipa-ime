#include "settings/native/native_page_help_widgets.h"

#include "utils/ipa_update.h"
#include "utils/ipa_app_icon.h"
#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

// partials/about-settings.html + modules/about-settings.ts + modules/update-manifest.ts;
// styles in styles/modules/document-settings.css (.about-*).
namespace native_settings
{
namespace
{
constexpr char kWebsiteUrl[] = "https://www.phonetictoolbox.com/";
constexpr char kLicenseUrl[] = "https://github.com/Nephelium/phonetic-ipa-ime/blob/ipa-prototype/LICENSE";
constexpr char kPrivacyUrl[] = "https://github.com/Nephelium/phonetic-ipa-ime/blob/ipa-prototype/PRIVACY.md";

// `.about-link-row`: min-height 66px with a hairline under every row but the last.
class AboutRow : public msimeui::Panel
{
  public:
    AboutRow(VisualPtr left, VisualPtr right, bool last) : left_(std::move(left)), right_(std::move(right)), last_(last)
    {
        AddChild(left_);
        if (right_)
            AddChild(right_);
    }

    void SetOnClick(std::function<void()> handler, std::shared_ptr<Label> title)
    {
        onClick_ = std::move(handler);
        title_ = std::move(title);
    }

    SizeF Measure(const SizeF &availableSize) override
    {
        rightSize_ = right_ ? right_->MeasureInLayout(availableSize) : SizeF{};
        leftSize_ = left_->MeasureInLayout(
            {std::max(availableSize.width - (right_ ? rightSize_.width + 20.0f : 0.0f), 0.0f), availableSize.height});
        const float border = last_ ? 0.0f : CssBorder();
        return {availableSize.width, std::max(66.0f, std::max(leftSize_.height, rightSize_.height) + border)};
    }

    void Arrange(const RectF &finalRect) override
    {
        bounds_ = finalRect;
        const float border = last_ ? 0.0f : CssBorder();
        const float height = finalRect.height - border;
        left_->ArrangeInLayout(
            {finalRect.x, finalRect.y + (height - leftSize_.height) * 0.5f, leftSize_.width, leftSize_.height});
        if (right_)
            right_->ArrangeInLayout({finalRect.x + finalRect.width - rightSize_.width,
                                     finalRect.y + (height - rightSize_.height) * 0.5f, rightSize_.width,
                                     rightSize_.height});
    }

    void Render(msimeui::DeviceResources &dr) override
    {
        if (title_)
            title_->SetColor(hovered_ ? CurrentPalette().accentStrong : CurrentPalette().text);
        left_->Render(dr);
        if (right_)
            right_->Render(dr);
        if (!last_)
        {
            const float border = CssBorder();
            FillRect(dr, {bounds_.x, bounds_.y + bounds_.height - border, bounds_.width, border},
                     CurrentPalette().border);
        }
    }

    msimeui::Visual *FindVisualAt(const PointF &point) override
    {
        if (onClick_)
            return HitTest(point) ? this : nullptr;
        return msimeui::Panel::FindVisualAt(point);
    }

    bool OnMouseDown(const POINT &point, WPARAM) override
    {
        pressed_ = onClick_ && window_ && HitTest(window_->ClientPixelsToDips(point));
        return pressed_;
    }

    bool OnMouseUp(const POINT &point, WPARAM) override
    {
        if (!pressed_)
            return false;
        pressed_ = false;
        if (window_ && HitTest(window_->ClientPixelsToDips(point)) && onClick_)
            onClick_();
        return true;
    }

    void OnMouseEnter() override
    {
        hovered_ = true;
        InvalidateVisual();
    }

    void OnMouseLeave() override
    {
        hovered_ = false;
        pressed_ = false;
        InvalidateVisual();
    }

    HCURSOR GetCursor() const override
    {
        return LoadCursor(nullptr, onClick_ ? IDC_HAND : IDC_ARROW);
    }

  private:
    VisualPtr left_;
    VisualPtr right_;
    bool last_ = false;
    SizeF leftSize_ = {};
    SizeF rightSize_ = {};
    std::function<void()> onClick_;
    std::shared_ptr<Label> title_;
    bool hovered_ = false;
    bool pressed_ = false;
};

// `.about-mark`: 52px tile with the 32px logo.
class AboutMark : public msimeui::Visual
{
  public:
    SizeF Measure(const SizeF &) override
    {
        return {52.0f, 52.0f};
    }
    void Arrange(const RectF &finalRect) override
    {
        bounds_ = finalRect;
    }
    void Render(msimeui::DeviceResources &dr) override
    {
        const Palette &p = CurrentPalette();
        FillRounded(dr, bounds_, 11.0f, p.surfaceSunken);
        StrokeRounded(dr, bounds_, 11.0f, p.border, 1.0f);
        logo_.SetPath(PhoneticIpa::AppIcon::AssetOr(AssetPath(L"ipa-bigfish.png")));
        logo_.Arrange({bounds_.x + 10.0f, bounds_.y + 10.0f, 32.0f, 32.0f});
        logo_.Render(dr);
    }

  private:
    BitmapImage logo_{AssetPath(L"ipa-bigfish.png"), 32.0f, 32.0f};
};

CssButton::Style DialogButtonStyle(bool primary)
{
    // .about-dialog-button: min-width 76px, padding 7px 13px, radius 6px, 12px.
    CssButton::Style style;
    style.look = primary ? CssButton::Look::Accent : CssButton::Look::Secondary;
    style.minWidth = 76.0f;
    style.paddingX = 13.0f;
    style.paddingY = 7.0f;
    style.fontSize = 12.0f;
    return style;
}

struct AboutState
{
    std::shared_ptr<CssButton> check;
    std::shared_ptr<Label> status;
    std::shared_ptr<Hideable> statusBox;
};

void ShowUpdateDialog(PageContext &context, const PhoneticIpa::Updates::Feed &feed)
{
    auto column = std::make_shared<Column>(12.0f);
    column->Add(MakeLabel(L"国际音标输入法 " + Widen(feed.version), DocStyle(16.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD),
                          TextRole::Primary));
    column->Add(MakeLabel(Widen(feed.notes), DocStyle(13.0f, DWRITE_FONT_WEIGHT_NORMAL, 1.65f, true), TextRole::Muted));
    auto actions = std::make_shared<Row>(9.0f);
    actions->SetJustify(Row::Justify::End);
    HostServices *host = &context.host;
    auto later = std::make_shared<CssButton>(L"稍后", DialogButtonStyle(false));
    later->SetOnClick([host] { host->CloseModal(); });
    actions->Add(later);
    for (const auto &entry :
         std::vector<std::pair<std::wstring, std::string>>{{L"GitHub 下载", feed.github_url}, {L"官网下载", feed.url}})
    {
        auto button = std::make_shared<CssButton>(entry.first, DialogButtonStyle(entry.first == L"官网下载"));
        button->SetOnClick([host, url = entry.second] {
            host->CloseModal();
            ShellExecuteW(nullptr, L"open", Widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        });
        actions->Add(button);
    }
    column->Add(actions);
    context.host.ShowModal(column);
}

void CheckForUpdate(PageContext &context, const std::shared_ptr<AboutState> &state)
{
    state->check->SetDisabled(true);
    state->check->SetText(L"正在检查…");
    state->status->SetText(L"正在检查官网与 GitHub…");
    state->statusBox->SetVisible(true);
    auto result = std::make_shared<PhoneticIpa::Updates::Result>();
    PageContext *ctx = &context;
    RunInBackground(
        context.host.Hwnd(),
        [result] {
            *result = PhoneticIpa::Updates::Check(true);
            return std::string{};
        },
        [ctx, state, result](const std::string &) {
            state->check->SetDisabled(false);
            state->check->SetText(L"检查更新");
            state->status->SetText(result->status);
            if (result->latest)
                ShowUpdateDialog(*ctx, *result->latest);
        });
}

std::weak_ptr<Button> g_restartButton;
void CALLBACK RestoreRestartButton(HWND hwnd, UINT, UINT_PTR id, DWORD)
{
    KillTimer(hwnd, id);
    if (auto button = g_restartButton.lock())
    {
        button->SetDisabled(false);
        button->SetText(L"重启");
    }
}
} // namespace

VisualPtr BuildAboutPage(PageContext &context)
{
    PageBuilder page(L"关于");
    page.SetTitleRole(TextRole::Secondary);
    SettingsModel *model = &context.model;
    const auto openUrl = [model](const std::string &url) { model->Post("openExternalUrl", Json(url)); };

    // .about-hero: mark, then eyebrow / title / text, gap 20px. The eyebrow is uppercased.
    auto heroRow = std::make_shared<Row>(20.0f);
    heroRow->Add(std::make_shared<AboutMark>());
    heroRow->Add(MakeDocumentHero(L"PHONETIC IPA", L"国际音标输入法",
                                  L"输入汉字与国际音标，按自己的习惯编辑键位。基于水杉输入法开发。"),
                 1.0f);
    page.AddRaw(std::make_shared<PaddedSection>(heroRow, Thickness{28.0f, 26.0f, 28.0f, 26.0f}));

    // .about-links: no vertical padding, rows of 66px.
    auto state = std::make_shared<AboutState>();
    auto links = std::make_shared<Column>(0.0f);
    {
        auto left = std::make_shared<Column>(0.0f);
        left->Add(MakeLabel(L"当前版本", DocStyle(13.0f, DWRITE_FONT_WEIGHT_MEDIUM), TextRole::Primary));
        auto version = MakeLabel(Widen(PhoneticIpa::Updates::kVersion), DocStyle(12.0f), TextRole::Muted);
        version->SetMargin({0.0f, 5.0f, 0.0f, 0.0f});
        left->Add(version);
        state->status = MakeLabel(L"", DocStyle(11.0f), TextRole::Muted);
        state->status->SetMinHeight(16.0f);
        state->statusBox = std::make_shared<Hideable>(state->status, false);
        state->statusBox->SetMargin({0.0f, 3.0f, 0.0f, 0.0f});
        left->Add(state->statusBox);
        // .about-update-button: padding 7px 13px, fully rounded, 12px; border-strong on
        // hover and pressed down 1px, like .default-ime-button.
        CssButton::Style style;
        style.paddingX = 13.0f;
        style.paddingY = 7.0f;
        style.radius = 99.0f;
        style.fontSize = 12.0f;
        style.transitionMs = 150.0;
        style.pressShift = true;
        style.disabledOpacity = 0.65f;
        state->check = std::make_shared<CssButton>(L"检查更新", style);
        PageContext *ctx = &context;
        std::weak_ptr<AboutState> weak = state;
        state->check->SetOnClick([ctx, weak]() {
            if (auto locked = weak.lock())
                CheckForUpdate(*ctx, locked);
        });
        links->Add(std::make_shared<AboutRow>(left, state->check, false));
    }
    auto iconActions = std::make_shared<Row>(9.0f);
    auto customIcon = std::make_shared<Button>(L"选择图片更换图标");
    customIcon->SetOnClick([ctx = &context]() {
        WCHAR path[32768]{};
        OPENFILENAMEW picker{sizeof(picker)};
        picker.hwndOwner = ctx->host.Hwnd();
        picker.lpstrFile = path;
        picker.nMaxFile = 32768;
        picker.lpstrFilter = L"图片\0*.png;*.jpg;*.jpeg;*.bmp\0\0";
        picker.Flags = OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&picker))
            return;
        try
        {
            PhoneticIpa::AppIcon::Import(path);
            ctx->host.ShowToast(L"图标已更换。", true);
            ctx->host.Window()->Invalidate();
        }
        catch (const std::exception &error)
        {
            ctx->host.ShowToast(Widen(error.what()), false);
        }
    });
    iconActions->Add(customIcon);
    auto defaultIcon = std::make_shared<Button>(L"恢复大肥鱼图标");
    defaultIcon->SetOnClick([ctx = &context]() {
        try
        {
            PhoneticIpa::AppIcon::Reset();
            ctx->host.ShowToast(L"已恢复默认图标。", true);
            ctx->host.Window()->Invalidate();
        }
        catch (const std::exception &error)
        {
            ctx->host.ShowToast(Widen(error.what()), false);
        }
    });
    iconActions->Add(defaultIcon);
    links->Add(MakeRow(L"应用显示图标", L"PNG、JPEG、BMP，最大 10 MB。用于设置窗口与任务栏，程序文件保留默认图标。",
                       iconActions));
    auto automatic = std::make_shared<ToggleSwitch>(PhoneticIpa::Updates::Automatic());
    automatic->SetOnChanged([](bool enabled) { PhoneticIpa::Updates::SetAutomatic(enabled); });
    links->Add(MakeRow(L"自动检查新版本", L"每天检查官网与 GitHub，发现新版时提醒下载。", automatic));
    struct Link
    {
        const wchar_t *title;
        const wchar_t *detail;
        const char *url;
    };
    const Link documents[] = {
        // The space before the detail <span> belongs to the title text.
        {L"PhoneticToolbox 语音工具箱官网 ", L"phonetictoolbox.com", kWebsiteUrl},
        {L"致谢水杉输入法", L"原始 Windows 项目与贡献者", "https://github.com/metasequoiaime/MSIME-Windows"},
        {L"开源许可协议", nullptr, kLicenseUrl},
        {L"隐私政策", nullptr, kPrivacyUrl},
    };
    for (size_t i = 0; i < std::size(documents); ++i)
    {
        auto title = MakeLabel(documents[i].title, DocStyle(13.0f, DWRITE_FONT_WEIGHT_MEDIUM), TextRole::Primary);
        auto titleRow = std::make_shared<Row>(8.0f);
        titleRow->SetAlign(Row::Align::End);
        titleRow->Add(title);
        if (documents[i].detail)
            titleRow->Add(MakeLabel(documents[i].detail, DocStyle(12.0f), TextRole::Muted));
        auto arrow = MakeLabel(L"↗", DocStyle(16.0f), TextRole::Muted);
        auto row = std::make_shared<AboutRow>(titleRow, arrow, i + 1 == std::size(documents));
        const std::string url = documents[i].url;
        row->SetOnClick([openUrl, url]() { openUrl(url); }, title);
        links->Add(row);
    }
    auto linksSection = std::make_shared<PaddedSection>(links, Thickness{24.0f, 0.0f, 24.0f, 0.0f});
    page.AddRaw(linksSection);

    // 输入法进程与设置窗口
    {
        Dropdown::Options linger;
        linger.minWidth = 150.0f;
        linger.matchToggleWidth = true;
        HostServices *host = &context.host;
        auto restart = std::make_shared<Button>(L"重启", ButtonKind::Secondary);
        g_restartButton = restart;
        std::weak_ptr<Button> weakRestart = restart;
        restart->SetOnClick([model, host, weakRestart]() {
            // The Watchdog owns the relaunch and never answers; hold the button for a moment
            // so repeated clicks do not queue several exits.
            auto button = weakRestart.lock();
            if (!button || button->IsDisabled())
                return;
            button->SetDisabled(true);
            button->SetText(L"重启中…");
            model->Post("restartServer");
            SetTimer(host->Hwnd(), 0x4E60, 3000, &RestoreRestartButton);
        });
        page.AddSection(MakeRows({
            MakeRow(L"重启输入法进程", L"立即重启输入法后台进程，使需要重启的设置生效。", restart),
            MakeRow(L"设置窗口后台驻留",
                    L"关闭设置窗口后在后台保留一段时间，期间重新打开无需等待加载；驻留越久占用内存越久。",
                    BindDropdown(context, "appearance.settings_window_linger",
                                 Options({{"off", L"关闭后立即退出"},
                                          {"1m", L"1 分钟"},
                                          {"5m", L"5 分钟"},
                                          {"10m", L"10 分钟"},
                                          {"30m", L"30 分钟"},
                                          {"60m", L"1 小时"},
                                          {"forever", L"一直驻留"}}),
                                 linger)),
        }));
    }

    page.AddSection(MakeRow(L"启动动画",
                            L"打开设置窗口时先在窗口中央显示齿轮图标，再淡入主界面。关闭后只短暂显示纯色背景。",
                            BindToggle(context, "appearance.settings_launch_animation")));
    page.AddSection(
        MakeRow(L"Server 端日志",
                L"排查 Server "
                L"通信和输入延迟时开启。记录慢请求阶段、候选窗、悬浮工具栏、菜单、焦点会话和通信状态，不记录按键、输入"
                L"内容或候选文本。日志默认保存到桌面的“水杉IME诊断日志.log”，复现后可直接发送该文件。",
                BindToggle(context, "general.diagnostic_log")));
    page.AddSection(MakeRow(L"TSF 端日志",
                            L"排查应用内预编辑和输入延迟时开启。TSF "
                            L"日志会在内存中限量缓冲，并通过独立管道批量汇总到桌面的“水杉IME诊断日志."
                            L"log”；不记录按键、输入内容或候选文本。",
                            BindToggle(context, "general.tsf_diagnostic_log")));
    // The button's handler only holds the state weakly (the state holds the button), so
    // the page has to own it or the first click finds it gone.
    return std::make_shared<OwnerBox>(page.Build(), state);
}
} // namespace native_settings
