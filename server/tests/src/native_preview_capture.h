#pragma once

// Test-only offscreen rendering of production controls. No desktop capture,
// shown window, OS input dispatch, or settings persistence is involved.
#include "msimeui/Fonts.h"
#include "msimeui/Theme.h"
#include "window/candidate_skin_scene.h"

static void CaptureControls(msimeui::DeviceResources &dr, const std::filesystem::path &file, UINT width, UINT height,
                            std::function<void()> paint);

static void RenderTypingSkin(msimeui::Window &window, const std::filesystem::path &resources,
                             const std::filesystem::path &destination, const std::string &id, const std::wstring &name,
                             bool light, bool horizontal, bool directIpa, bool toneEntry = false)
{
    const auto skinsRoot = resources.parent_path() / "skins";
    std::string error;
    Require(CandidateSkinCatalog::Load(skinsRoot, id, &error).has_value(), error.c_str());
    Require(CandidateSkinCatalog::LoadSupported(skinsRoot, id, horizontal ? "horizontal" : "vertical",
                                                light ? "light" : "dark")
                .has_value(),
            "Unsupported skin variant");
    CandidateSkinScene scene;
    CandidateSkinScene::Settings settings;
    settings.skinsRoot = skinsRoot;
    settings.skinId = id;
    settings.light = light;
    settings.horizontal = horizontal;
    settings.fontSize = 24;
    settings.preeditFontSize = 18;
    settings.fontFamily = L"Microsoft YaHei UI";
    settings.fallbackFontFamilies = {L"Doulos SIL", L"PTB IPA Plus"};
    scene.Apply(settings);
    scene.Preedit()->SetText(toneEntry ? L"tone:214\n" + Widen(phonetic_ipa::EntryHint("tone:214"))
                                       : (directIpa ? L"a" : L"hanzi"));
    std::vector<msimeui::CandidateList::Item> items;
    phonetic_ipa::HanziTable hanzi;
    hanzi.Load(resources / "beijing-toned.tsv");
    if (directIpa)
    {
        const auto table = PhoneticIpa::ImportProfile(resources / "ipa.cin");
        for (const auto &value : phonetic_ipa::InputCandidates(table, toneEntry ? "tone:214" : "a"))
        {
            if (items.size() == 9)
                break;
            items.push_back({toneEntry ? L"" : std::to_wstring(items.size() + 1), Widen(value), L"", L"", {}, false});
        }
    }
    else
    {
        for (const auto &word : {u8"汉字", u8"汉子", u8"憨子"})
            items.push_back({std::to_wstring(items.size() + 1),
                             Widen(word),
                             L"",
                             Widen(hanzi.Convert(word, "han'zi").text),
                             {},
                             true});
    }
    for (auto &item : items)
        if (directIpa)
            item.textFontFamily = L"Doulos SIL";
        else if (!item.translation.empty())
            item.secondaryFontFamily = L"Doulos SIL";
    scene.List()->SetItems(items);
    scene.Root()->Attach(&window);
    const auto size = scene.Root()->Measure({1060, 1600});
    scene.Root()->Arrange({70, 302, size.width, size.height});
    const auto bg = light ? (id == "ipa-paper" ? Hex(0xEEE9DF) : Hex(0xE7F0EC)) : Hex(0x0E192A);
    const auto paper = light ? Hex(0xFFFEFB) : Hex(0x132238);
    const auto ink = light ? Hex(0x273B36) : Hex(0xDDEAF9);
    const auto muted = light ? Hex(0x78847C) : Hex(0x8CA4C1);
    auto &dr = window.GetDeviceResources();
    const float footerY = std::max(643.0f, 302 + size.height + 24);
    const auto imageHeight = static_cast<UINT>(std::ceil(footerY + 95));
    CaptureControls(dr, destination / ("skin-" + id + (toneEntry ? "-tone" : "") + ".png"), 1240, imageHeight, [&] {
        dr.GetRenderTarget()->Clear(bg);
        auto style = BodyStyle();
        style.size = 30;
        style.weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
        DrawTextInRect(dr, name, style, {42, 26, 600, 46}, ink);
        style.size = 14;
        style.weight = DWRITE_FONT_WEIGHT_NORMAL;
        DrawTextInRect(dr, L"输入场景预览 · 24 DIP", style, {924, 39, 278, 24}, muted);
        FillRounded(dr, {40, 102, 1160, static_cast<float>(imageHeight) - 148}, 14, paper);
        StrokeRounded(dr, {40, 102, 1160, static_cast<float>(imageHeight) - 148}, 14,
                      light ? Hex(0xD5DDD7) : Hex(0x2C405A));
        DrawTextInRect(dr, L"语音笔记", style, {74, 124, 400, 30}, muted);
        DrawLine(dr, {74, 170}, {1164, 170}, light ? Hex(0xE7E9E3) : Hex(0x293D55), 1);
        style.size = 25;
        DrawTextInRect(dr, toneEntry ? L"声调记录" : (directIpa ? L"音标记录" : L"词语记录"), style, {76, 199, 800, 42},
                       ink);
        style.size = 21;
        DrawTextInRect(dr, toneEntry ? L"五度调值：" : (directIpa ? L"当前编码：" : L"本次标注的词语："), style,
                       {76, 264, 500, 36}, muted);
        DrawTextInRect(dr, toneEntry ? L"tone:214" : (directIpa ? L"a" : L"hanzi"), style,
                       {directIpa ? 192.0f : 255.0f, 264, 160, 36}, ink);
        DrawLine(dr, {directIpa ? 192.0f : 255.0f, 296}, {directIpa ? 242.0f : 311.0f, 296}, ink, 1);
        scene.Root()->Render(dr);
        style.size = 15;
        DrawTextInRect(dr,
                       toneEntry   ? L"调值输入 · 1–5 继续输入调值 · ↑↓ 选写法 · 空格上屏"
                       : directIpa ? L"IPA 模式 · a 编码 · 每页 9 项 · 1–9 选择音标"
                                   : L"中文模式 · 上行选汉字，下行选 IPA · Ctrl+Enter 输入音标",
                       style, {76, footerY, 1060, 28}, muted);
    });
}

static void CheckHr(HRESULT result, const char *operation)
{
    if (FAILED(result))
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}

class PreviewHost : public HostServices
{
  public:
    explicit PreviewHost(msimeui::Window &window) : window_(window)
    {
    }
    HWND Hwnd() const override
    {
        return window_.GetHandle();
    }
    msimeui::Window *Window() const override
    {
        return &window_;
    }
    void ShowToast(const std::wstring &, bool) override
    {
    }
    void Confirm(const std::wstring &, std::function<void(bool)>, const std::wstring &, const std::wstring &,
                 const std::wstring &) override
    {
        throw std::runtime_error("Preview cannot confirm changes");
    }
    void ShowModal(VisualPtr, std::function<void()>) override
    {
    }
    void CloseModal() override
    {
    }
    void NavigateTo(const std::string &) override
    {
    }
    void ScrollIntoView(const msimeui::Visual &) override
    {
    }

  private:
    msimeui::Window &window_;
};

static void CaptureControls(msimeui::DeviceResources &dr, const std::filesystem::path &file, UINT width, UINT height,
                            std::function<void()> paint)
{
    auto dc = dr.GetDeviceContext();
    Require(dc != nullptr, "Offscreen D2D context");
    const auto format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    D2D1_BITMAP_PROPERTIES1 properties{};
    properties.pixelFormat = format;
    properties.dpiX = properties.dpiY = 96;
    properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> target, pixels;
    CheckHr(dc->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0, properties, &target), "Create target");
    dc->SetTarget(target.Get());
    dc->SetDpi(96, 96);
    dc->SetTransform(D2D1::Matrix3x2F::Identity());
    dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    dr.BeginFrame();
    Require(dr.BeginDraw(), "Begin offscreen frame");
    dc->Clear(CurrentPalette().chromeBg);
    paint();
    CheckHr(dr.EndDraw(), "Render production controls");
    dc->SetTarget(nullptr);
    properties.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    CheckHr(dc->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0, properties, &pixels), "Create readback");
    CheckHr(pixels->CopyFromBitmap(nullptr, target.Get(), nullptr), "Read rendered pixels");
    D2D1_MAPPED_RECT mapped{};
    CheckHr(pixels->Map(D2D1_MAP_OPTIONS_READ, &mapped), "Map pixels");
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
    Microsoft::WRL::ComPtr<IWICStream> stream;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    CheckHr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)), "WIC");
    CheckHr(wic->CreateStream(&stream), "PNG stream");
    CheckHr(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE), "PNG file");
    CheckHr(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "PNG encoder");
    CheckHr(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Initialize PNG");
    CheckHr(encoder->CreateNewFrame(&frame, nullptr), "PNG frame");
    CheckHr(frame->Initialize(nullptr), "Initialize frame");
    CheckHr(frame->SetSize(width, height), "PNG size");
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    CheckHr(frame->SetPixelFormat(&pixelFormat), "PNG format");
    CheckHr(frame->WritePixels(height, mapped.pitch, mapped.pitch * height, mapped.bits), "PNG pixels");
    CheckHr(frame->Commit(), "Commit frame");
    CheckHr(encoder->Commit(), "Commit PNG");
    pixels->Unmap();
    std::cout << "Rendered " << file.u8string() << '\n';
}

static int RenderPreviews(const std::filesystem::path &resources, const std::filesystem::path &destination)
{
    CheckHr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "COM");
    SetEnvironmentVariableW(L"METASEQUOIA_IME_DATA_DIR", resources.parent_path().c_str());
    std::filesystem::create_directories(destination);
    msimeui::LoadPrivateIpaFont((resources / "DoulosSIL-Regular.ttf").wstring());
    msimeui::LoadPrivateIpaFont((resources / "PTBIPAPlus-Regular.ttf").wstring());
    SetCurrentPalette(true);
    TestSurface surface;
    surface.window.SetDpiOverride(96);
    auto &dr = surface.window.GetDeviceResources();
    Require(dr.EnsureForComposition(surface.hwnd), "Initialize hidden renderer");
    dr.SetAnimationsEnabled(false); // Still image records the settled hover state.
    PreviewHost host(surface.window);
    SettingsModel model([](const std::string &) { throw std::runtime_error("Preview cannot send settings"); });
    model.Receive(R"({"type":"configSnapshot","data":{"appearance":{"page_size":9}}})");
    PageContext context(model, host);
    auto page = BuildIpaKeyboardPage(context);
    page->Attach(&surface.window);
    const auto size = page->Measure({1400, 4096});
    page->Arrange({0, 0, 1400, size.height});
    CaptureControls(dr, destination / "01-settings.png", 1400, static_cast<UINT>(std::ceil(size.height)),
                    [&] { page->Render(dr); });

    KeyboardCanvas *keyboard = nullptr;
    SymbolCanvas *symbols = nullptr;
    IpaCandidateCards *cards = nullptr;
    for (float y = 0; y < size.height && (!keyboard || !symbols || !cards); y += 10)
        for (float x = 0; x < 1400; x += 15)
        {
            auto visual = page->FindVisualAt({x, y});
            if (auto found = dynamic_cast<KeyboardCanvas *>(visual))
                keyboard = found;
            if (auto found = dynamic_cast<SymbolCanvas *>(visual))
                symbols = found;
            if (auto found = dynamic_cast<IpaCandidateCards *>(visual))
                cards = found;
        }
    Require(keyboard && symbols && cards, "Production keyboard, candidate cards and Plus panel");
    for (const auto &key : PhoneticIpa::KeyboardKeys(keyboard->size))
        if (key.vk == 'L')
            keyboard->OnMouseMove(Center(keyboard->Rect(key), 96), 0);
    for (const auto &symbol : symbols->symbols)
        if (symbol.output == u8"ɕ")
        {
            symbols->choose(symbol);
            break;
        }
    const auto editedSize = page->Measure({1400, 4096});
    page->Arrange({0, 0, 1400, editedSize.height});
    const float lower = keyboard->GetBounds().y + keyboard->GetBounds().height + 70;
    CaptureControls(dr, destination / "02-hover-edit-plus.png", 1400,
                    static_cast<UINT>(std::ceil(editedSize.height - lower)), [&] {
                        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -lower));
                        page->Render(dr);
                    });
    const float chartTop = symbols->GetBounds().y - 210;
    const auto captureChart = [&](const char *name) {
        CaptureControls(dr, destination / name, 1400, 960, [&] {
            dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -chartTop));
            page->Render(dr);
        });
    };
    captureChart("05-plus-consonants.png");
    symbols->ScrollTo(symbols->SectionTop("vowels"));
    captureChart("06-plus-vowels.png");
    const auto selectChartSystem = [&](size_t tabIndex, const char *system) {
        // The fixed 1400-DIP preview uses the production 72-DIP tabs, 8-DIP gaps.
        // Call only the invisible control's methods; never dispatch desktop input.
        const PointF point{symbols->GetBounds().x + 36 + tabIndex * 80.f, chartTop + 64};
        auto tab = dynamic_cast<Button *>(page->FindVisualAt(point));
        Require(tab != nullptr, "Production IPA system tab");
        const auto center = Center(tab->GetBounds(), 96);
        Require(tab->OnMouseDown(center, 0) && tab->OnMouseUp(center, 0), "Switch IPA system tab");
        Require(symbols->system == system && symbols->section == "all", "IPA tab did not reset its chart");
    };
    selectChartSystem(1, "extipa");
    captureChart("07-extipa-consonants.png");
    symbols->ScrollTo(symbols->SectionTop("diacritics"));
    captureChart("08-extipa-diacritics-voicing.png");
    symbols->ScrollTo(symbols->SectionTop("rhythm"));
    captureChart("09-extipa-rhythm-uncertainty.png");
    selectChartSystem(2, "voqs");
    captureChart("10-voqs.png");
    selectChartSystem(0, "ipa");
    symbols->ScrollTo(0);
    const float keyboardTop = keyboard->GetBounds().y - 24;
    CaptureControls(dr, destination / "03-keyboard-hover.png", 1400, 480, [&] {
        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -keyboardTop));
        page->Render(dr);
    });
    for (const auto &key : PhoneticIpa::KeyboardKeys(keyboard->size))
        if (key.vk == 'A')
            keyboard->OnMouseMove(Center(keyboard->Rect(key), 96), 0);
    CaptureControls(dr, destination / "03b-keyboard-hover-a.png", 1400, 480, [&] {
        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -keyboardTop));
        page->Render(dr);
    });
    keyboard->OnMouseLeave();
    const float editorTop = cards->GetBounds().y - 152;
    CaptureControls(dr, destination / "11-editor-cards.png", 1400, 464, [&] {
        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -editorTop));
        page->Render(dr);
    });
    SetCurrentPalette(false);
    // Construct the dark page as at launch, avoiding a still image of the
    // controls' live 200-ms palette transition from the preceding light frame.
    auto darkPage = BuildIpaKeyboardPage(context);
    darkPage->Attach(&surface.window);
    const auto darkSize = darkPage->Measure({1400, 4096});
    darkPage->Arrange({0, 0, 1400, darkSize.height});
    CaptureControls(dr, destination / "11b-editor-cards-dark.png", 1400, 464, [&] {
        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -editorTop));
        darkPage->Render(dr);
    });
    SetCurrentPalette(true);
    keyboard->choose('V');
    const auto comboSize = page->Measure({1400, 4096});
    page->Arrange({0, 0, 1400, comboSize.height});
    for (const auto &key : PhoneticIpa::KeyboardKeys(keyboard->size))
        if (key.vk == 'V')
            keyboard->OnMouseMove(Center(keyboard->Rect(key), 96), 0);
    CaptureControls(dr, destination / "12-v-combination-preview.png", 1400, 480, [&] {
        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -keyboardTop));
        page->Render(dr);
    });
    for (const auto &key : PhoneticIpa::KeyboardKeys(keyboard->size))
        if (key.vk == VK_OEM_5)
            keyboard->OnMouseMove(Center(keyboard->Rect(key), 96), 0);
    CaptureControls(dr, destination / "13-backslash-tone-preview.png", 1400, 480, [&] {
        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -keyboardTop));
        page->Render(dr);
    });
    keyboard->OnMouseLeave();
    const auto narrowSize = page->Measure({820, 4096});
    page->Arrange({0, 0, 820, narrowSize.height});
    const float narrowEditorTop = cards->GetBounds().y - 220;
    CaptureControls(dr, destination / "14-editor-narrow.png", 820, 670, [&] {
        dr.GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Translation(0, -narrowEditorTop));
        page->Render(dr);
    });
    RenderTypingSkin(surface.window, resources, destination, "ipa-paper", L"暖纸 / Warm Paper", true, true, false);
    RenderTypingSkin(surface.window, resources, destination, "ipa-celadon", L"青瓷 / Celadon", true, false, false);
    RenderTypingSkin(surface.window, resources, destination, "ipa-night", L"夜航 / Night Flight", false, true, true);

    RenderTypingSkin(surface.window, resources, destination, "ipa-night", L"夜航 / 五度调值输入", false, true, true,
                     true);

    phonetic_ipa::HanziTable hanzi;
    hanzi.Load(resources / "beijing-toned.tsv");
    const auto ipa = Widen(hanzi.Convert(u8"汉字", "han'zi").text);
    msimeui::CandidateList chinese, cin;
    chinese.Attach(&surface.window);
    cin.Attach(&surface.window);
    chinese.SetItems({{L"1", L"汉字", L"", ipa, {}, true, L"", L"Doulos SIL"}});
    std::vector<msimeui::CandidateList::Item> entries;
    const auto table = PhoneticIpa::ImportProfile(resources / "ipa.cin");
    for (const auto &value : table.Lookup("a"))
        entries.push_back({std::to_wstring(entries.size() + 1), Widen(value), L"", L"", {}, false, L"Doulos SIL"});
    cin.SetItems(entries);
    auto chineseSize = chinese.Measure({560, 2000});
    chinese.Arrange({30, 70, 560, chineseSize.height});
    auto cinSize = cin.Measure({560, 2000});
    cin.Arrange({630, 70, 560, cinSize.height});
    auto title = BodyStyle();
    title.size = 20;
    CaptureControls(dr, destination / "04-candidate-controls.png", 1220,
                    static_cast<UINT>(std::ceil(std::max(chineseSize.height, cinSize.height) + 110)), [&] {
                        dr.GetRenderTarget()->Clear(Hex(0x252525));
                        DrawTextInRect(dr, L"汉字候选 · hanzi", title, {30, 22, 560, 35}, Hex(0xF5F5F5));
                        DrawTextInRect(dr, L"IPA 码表候选 · a", title, {630, 22, 560, 35}, Hex(0xF5F5F5));
                        chinese.Render(dr);
                        cin.Render(dr);
                    });
    CandidateSkinScene readingScene;
    CandidateSkinScene::Settings readingSettings;
    readingSettings.skinsRoot = resources.parent_path() / "skins";
    readingSettings.skinId = "ipa-paper";
    readingSettings.light = true;
    readingSettings.horizontal = false;
    readingSettings.fontSize = 24;
    readingSettings.preeditFontSize = 17;
    readingSettings.fontFamily = L"Microsoft YaHei UI";
    readingSettings.fallbackFontFamilies = {L"Doulos SIL", L"PTB IPA Plus"};
    readingScene.Apply(readingSettings);
    readingScene.Preedit()->SetText(L"haoxue\n选择读音 · 数字/空格选定 · Ctrl+Shift+Enter 返回");
    std::vector<msimeui::CandidateList::Item> readingItems;
    const auto studyReadings = hanzi.Convert(u8"好学", "hao'xue");
    Require(studyReadings.ambiguous && studyReadings.text.empty(), "Preview word must need a choice");
    for (const auto &reading : studyReadings.parts[0].options)
        readingItems.push_back({std::to_wstring(readingItems.size() + 1),
                                L"好学 · " + Widen(reading.pinyin),
                                L"",
                                Widen(reading.ipa),
                                {},
                                true});
    readingItems.push_back({L"3", L"返回汉字候选", L"", L"", {}, false});
    for (auto &item : readingItems)
        if (!item.translation.empty())
            item.secondaryFontFamily = L"Doulos SIL";
    readingScene.List()->SetItems(readingItems);
    readingScene.Root()->Attach(&surface.window);
    const auto readingSize = readingScene.Root()->Measure({760, 1200});
    readingScene.Root()->Arrange({24, 75, readingSize.width, readingSize.height});
    const auto readingHeight = static_cast<UINT>(std::ceil(readingSize.height + 155));
    CaptureControls(dr, destination / "15-reading-choice.png", 820, readingHeight, [&] {
        dr.GetRenderTarget()->Clear(Hex(0xEEE9DF));
        DrawTextInRect(dr, L"好学 · 两种词语读音", title, {48, 22, 710, 36}, Hex(0x273B36));
        readingScene.Root()->Render(dr);
        auto note = BodyStyle();
        note.size = 15;
        DrawTextInRect(dr, L"选定读音后返回汉字候选，再按 Ctrl+Enter 输入音标。", note,
                       {48, static_cast<float>(readingHeight) - 50, 720, 30}, Hex(0x526259));
    });
    Require(!IsWindowVisible(surface.hwnd), "Preview window must remain invisible");
    return 0;
}
