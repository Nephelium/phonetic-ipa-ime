// Include this translation unit to exercise its private controls and Editor,
// rather than copying UI behavior into a second model just for tests.
#include "settings/native/native_page_ipa.cpp"
#include "utils/ipa_app_icon.h"
#include <iostream>
#include <cmath>
#include <set>

using namespace native_settings;
static void Require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

struct EditFixture
{
    Editor editor;
    KeyboardCanvas keyboard;
    TextField code{TextField::Options{}};
    IpaCandidateEditor output;
    Label status{L"", BodyStyle()}, detail{L"", BodyStyle()};
    Dropdown codes{std::vector<DropdownOption>{}}, layout{Options({{"104", L"104"}, {"60", L"60"}})},
        layer{Options({{"0", L"Base"}, {"6", L"Ctrl+Alt"}})};
    explicit EditFixture(const phonetic_ipa::CinTable &table)
    {
        editor.table = table;
        editor.keyboard = &keyboard;
        editor.code = &code;
        editor.output = &output;
        editor.status = &status;
        editor.detail = &detail;
        editor.codes = &codes;
        editor.layout = &layout;
        editor.layer = &layer;
        editor.SelectCode("a");
    }
    void Edit(const std::wstring &text)
    {
        output.SetText(text);
        editor.editing = true;
    }
};

// Only an invisible, never-activated STATIC HWND owned by this test process.
// Pointer methods below are ordinary C++ calls, not Windows input events.
struct TestSurface
{
    HWND hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"IPA control test", WS_POPUP, 0, 0,
                                1200, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    msimeui::Window window{L"unused", L"unused", 1200, 600};
    TestSurface()
    {
        Require(hwnd != nullptr, "Test surface creation");
        window.AdoptExistingHwnd(hwnd);
    }
    ~TestSurface()
    {
        DestroyWindow(hwnd);
    }
};

static POINT Center(const msimeui::RectF &rect, float dpi)
{
    return {static_cast<LONG>(std::lround((rect.x + rect.width / 2) * dpi / 96)),
            static_cast<LONG>(std::lround((rect.y + rect.height / 2) * dpi / 96))};
}

#include "native_preview_capture.h"
#include "native_font_checks.h"

int main(int argc, char **argv)
{
    std::cout << std::unitbuf;
    if (argc == 3)
    {
        try
        {
            return RenderPreviews(std::filesystem::u8path(argv[1]), std::filesystem::u8path(argv[2]));
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            return 1;
        }
    }
    int failures = 0, passed = 0;
    if (argc != 2)
        return 2;
    const auto root = std::filesystem::u8path(argv[1]);
    Require(msimeui::LoadPrivateIpaFont((root / "DoulosSIL-Regular.ttf").wstring()), "Load private Doulos font");
    Require(msimeui::LoadPrivateIpaFont((root / "PTBIPAPlus-Regular.ttf").wstring()), "Load private IPA Plus font");
    const auto table = PhoneticIpa::ImportProfile(root / "ipa.cin");
    const auto foreground = GetForegroundWindow();
    POINT pointerBefore{};
    GetCursorPos(&pointerBefore);
    auto run = [&](const char *name, auto action) {
        try
        {
            action();
            ++passed;
            std::cout << "PASS " << name << '\n';
        }
        catch (const std::exception &e)
        {
            ++failures;
            std::cout << "FAIL " << name << ": " << e.what() << '\n';
        }
    };
    run("select, edit, stash and Unicode candidate order", [&] {
        EditFixture f(table);
        f.Edit(L"a\u0303\n \u0250 \na\u0303");
        f.editor.SelectCode("r");
        Require(f.editor.table.Lookup("a") == std::vector<std::string>{u8"ã", u8" ɐ ", u8"ã"},
                "Stash lost candidate bytes/order");
        Require(f.code.GetText() == L"r" && f.editor.history.size() == 1, "Selection did not advance");
        f.Edit(L"\t");
        const auto before = f.editor.table.Serialize();
        f.editor.SelectCode("a");
        Require(f.code.GetText() == L"r" && f.editor.table.Serialize() == before, "Invalid edit changed selection/map");
    });
    run("numeric custom code is editable and preserves zero mapping", [&] {
        EditFixture f(table);
        f.code.SetText(L"a10");
        f.Edit(L"a¹⁰");
        Require(f.editor.Stash(), "Numeric custom code rejected");
        Require(phonetic_ipa::InputCandidates(f.editor.table, "code:a10")[0] == u8"a¹⁰", "Edited code unreachable");
        Require(f.editor.table.Lookup("0") == table.Lookup("0"), "Zero mapping changed");
    });
    run("undo restores original code and outputs", [&] {
        EditFixture f(table);
        const auto initial = f.output.GetText();
        f.code.SetText(L"newcode");
        f.Edit(L"\u0255");
        f.editor.Undo();
        Require(f.code.GetText() == L"a" && f.output.GetText() == initial,
                "Undo kept the edited code instead of restoring a");
    });
    run("dropdown selection survives rebuilding its options", [&] {
        EditFixture f(table);
        f.editor.RefreshCodes();
        const auto &options = f.codes.GetOptions();
        const auto found =
            std::find_if(options.begin(), options.end(), [](const auto &option) { return option.value == "r"; });
        Require(found != options.end(), "Missing real r code");
        f.Edit(L"\u0250");
        f.editor.SelectCode(found->value);
        Require(f.code.GetText() == L"r" && f.editor.selectedCode == "r",
                "Dropdown selection was invalidated by stashing");
    });
    run("function-key chord returns safely to base layer", [&] {
        EditFixture f(table);
        f.editor.ChangeLayer(6);
        f.editor.SelectKey(VK_F1);
        f.Edit(L"\u014b");
        f.editor.ChangeLayer(0);
        Require(f.editor.modifiers == 0 && f.editor.chordKey == 0 && !f.code.GetText().empty(),
                "Base layer retained stale Ctrl+Alt+F1 binding");
        f.Edit(L"\u0250");
        Require(f.editor.Stash(), "Base edit remained trapped in invalid chord state");
        Require(f.editor.table.Hotkey(6, VK_F1) == u8"ŋ", "Layer switch lost the prior chord output");
    });
    run("IPA Plus uses real insertText, including combining marks", [&] {
        std::ifstream stream(root / "plus-catalog.json", std::ios::binary);
        const auto catalog = nlohmann::json::parse(stream);
        Require(catalog.at("entries").size() == 625, "Catalog entry count drifted");
        EditFixture f(table);
        f.output.SetText(L"a");
        for (const auto &entry : catalog.at("entries"))
        {
            Symbol symbol{Widen(entry.at("display").get<std::string>()), Widen(entry.at("nameZh").get<std::string>()),
                          entry.at("insertText").get<std::string>(), entry.at("system").get<std::string>(),
                          entry.at("section").get<std::string>()};
            f.editor.append = false;
            f.editor.InsertSymbol(symbol);
            Require(Narrow(f.output.GetText()) == symbol.output, "Picker inserted display instead of insertText");
            f.output.SetText(L"a");
            f.editor.append = true;
            f.editor.InsertSymbol(symbol);
            Require(Narrow(f.output.GetText()) == "a" + symbol.output, "Append changed Unicode codepoints");
        }
    });
    run("keyboard pointer routing and DPI scale", [&] {
        TestSurface surface;
        KeyboardCanvas keyboard;
        keyboard.Attach(&surface.window);
        unsigned chosen = 0, hovered = 0;
        keyboard.choose = [&](unsigned key) { chosen = key; };
        keyboard.hover = [&](unsigned key) { hovered = key; };
        for (float dpi : {96.f, 120.f, 144.f, 192.f})
            for (const std::string size : {"104", "87", "75", "65", "60"})
            {
                surface.window.SetDpiOverride(dpi);
                keyboard.size = size;
                keyboard.Arrange({10, 20, 820, keyboard.Measure({820, 2000}).height});
                const auto keys = PhoneticIpa::KeyboardKeys(size);
                for (const auto &key : keys)
                {
                    const auto point = Center(keyboard.Rect(key), dpi);
                    chosen = 0;
                    keyboard.OnMouseMove(point, 0);
                    Require(hovered == key.vk, "Hover selected another key");
                    keyboard.OnMouseDown(point, 0);
                    keyboard.OnMouseUp(point, 0);
                    Require(chosen == key.vk, "Click selected another key");
                }
                chosen = 0;
                keyboard.OnMouseDown(Center(keyboard.Rect(keys[1]), dpi), 0);
                keyboard.OnMouseUp(Center(keyboard.Rect(keys[2]), dpi), 0);
                Require(chosen == 0, "Drag-release activated another key");
            }
        Require(!IsWindowVisible(surface.hwnd), "Test surface became visible");
    });
    run("candidate cards preserve punctuation, other candidates and ordering", [&] {
        EditFixture f(table);
        f.Edit(L"ã\n ,; \nŋ");
        Require(f.editor.Stash(), "Cannot stash initial card values");
        f.output.Select(1);
        Symbol symbol{L"◌́", L"高调", u8"́", "ipa", "diacritics"};
        f.editor.append = false;
        f.editor.InsertSymbol(symbol);
        Require(f.output.Values() == std::vector<std::wstring>{L"ã", L"́", L"ŋ"}, "Picker replaced other cards");
        f.editor.append = true;
        f.editor.InsertSymbol(symbol);
        Require(f.output.SelectedOutput() == L"́́", "Combining bytes changed");
        f.output.MoveCandidate(-1);
        Require(f.output.Selected() == 0 && f.output.Values()[1] == L"ã", "Candidate order did not move");
        f.output.AddCandidate();
        f.output.SetSelectedOutput(L" ,; ");
        Require(f.editor.Stash(), "Punctuation cannot be saved");
        Require(f.editor.table.Lookup("a").back() == " ,; ", "Punctuation parsed as delimiters");
        f.output.RemoveCandidate();
        Require(f.output.Values().size() == 3, "Remove did not target the selected card");
        f.editor.SelectCode("v");
        Require(f.detail.GetText() == CodePoints(Narrow(f.output.SelectedOutput())),
                "Changing keys retained the previous symbol description");
        f.editor.SelectKey('1');
        f.output.SetSelectedOutput(L"x");
        Require(f.output.Values().empty(), "Reserved digit accepted an output");
    });
    run("horizontal cards route clicks and cancel a press across pages", [&] {
        TestSurface surface;
        IpaCandidateCards cards;
        cards.Attach(&surface.window);
        cards.values = std::vector<std::wstring>(80, L"ã");
        for (float dpi : {96.f, 120.f, 144.f, 192.f})
            for (float width : {360.f, 820.f, 1320.f})
            {
                surface.window.SetDpiOverride(dpi);
                cards.selected = cards.page = 0;
                cards.Arrange({10, 20, width, cards.Measure({width, 1000}).height});
                size_t chosen = 999;
                cards.choose = [&](size_t i) { chosen = i; };
                const auto first = Center(cards.Card(0), dpi);
                cards.OnMouseDown(first, 0);
                cards.OnMouseWheel(first, -WHEEL_DELTA, 0);
                cards.OnMouseUp(first, 0);
                Require(chosen == 999, "Page change committed another candidate");
                const auto index = cards.page * cards.Capacity() + 1;
                const auto point = Center(cards.Card(index), dpi);
                cards.OnMouseDown(point, 0);
                cards.OnMouseUp(point, 0);
                Require(chosen == index, "Card click selected a neighbor");
            }
    });
    run("prefix previews include labeled sequences and reserve numeric keys", [&] {
        EditFixture f(table);
        const auto entries = f.editor.PreviewOutputs('V');
        size_t combinations = 0;
        for (const auto &[value, label] : entries)
            if (label == L"组合输入 · voqs")
                ++combinations;
        Require(combinations == table.Lookup("voqs").size() && combinations > 0, "V preview omitted voqs");
        for (unsigned key = '1'; key <= '9'; ++key)
            Require(f.editor.PreviewOutputs(key).empty(), "Numeric key advertises IPA mappings");
        const auto defaults = PhoneticIpa::ImportProfile(root / "default.cin");
        auto expected = table.Lookup("1");
        expected.insert(expected.end(), table.Lookup("\\").begin(), table.Lookup("\\").end());
        Require(defaults.Lookup("1").empty() && defaults.Lookup("\\") == expected, "1 to backslash move lost outputs");
        Require(defaults.Lookup("0") == table.Lookup("0") && defaults.Lookup("@") == std::vector<std::string>{u8"²"},
                "Move changed zero or Shift tone input");
        f.editor.ReserveCandidateDigits();
        const auto moved = f.editor.table.Serialize();
        Require(f.editor.table.Lookup("1").empty() && f.editor.table.Lookup("\\") == expected &&
                    f.editor.table.Lookup("0") == table.Lookup("0"),
                "Imported numeric mappings were lost");
        f.editor.ReserveCandidateDigits();
        Require(f.editor.table.Serialize() == moved, "Repeated import normalization duplicated candidates");
    });
    run("layout change invalidates prior hover index", [&] {
        TestSurface surface;
        KeyboardCanvas keyboard;
        keyboard.Attach(&surface.window);
        unsigned hovered = 0;
        keyboard.hover = [&](unsigned key) { hovered = key; };
        keyboard.size = "104";
        keyboard.Arrange({0, 0, 820, 310});
        auto keys = PhoneticIpa::KeyboardKeys(keyboard.size);
        keyboard.OnMouseMove(Center(keyboard.Rect(keys[2]), 96), 0);
        keyboard.size = "60";
        keyboard.Arrange({0, 0, 820, 250});
        keys = PhoneticIpa::KeyboardKeys(keyboard.size);
        keyboard.OnMouseMove(Center(keyboard.Rect(keys[2]), 96), 0);
        Require(hovered == keys[2].vk, "Old hover index hid the new key output");
    });
    run("large preview never steals neighboring keys and paginates", [&] {
        TestSurface surface;
        KeyboardCanvas keyboard;
        keyboard.Attach(&surface.window);
        keyboard.outputs = [](unsigned) { return std::vector<std::wstring>(128, L"ã"); };
        for (float dpi : {96.f, 120.f, 144.f, 192.f})
            for (float width : {360.f, 820.f, 1320.f})
                for (const std::string layout : {"104", "87", "75", "65", "60"})
                {
                    surface.window.SetDpiOverride(dpi);
                    keyboard.size = layout;
                    const auto size = keyboard.Measure({width, 2000});
                    keyboard.Arrange({10, 20, width, size.height});
                    const auto keys = PhoneticIpa::KeyboardKeys(layout);
                    const auto a = std::find_if(keys.begin(), keys.end(), [](const auto &k) { return k.vk == 'A'; });
                    const auto adjacent =
                        std::find_if(keys.begin(), keys.end(), [](const auto &k) { return k.vk == 'S'; });
                    keyboard.OnMouseMove(Center(keyboard.Rect(*a), dpi), 0);
                    const auto preview = keyboard.Preview();
                    Require(preview.box.x >= 10 && preview.box.y >= 20 &&
                                preview.box.x + preview.box.width <= width + 10 &&
                                preview.box.y + preview.box.height <= size.height + 20,
                            "Large preview left the keyboard viewport");
                    Require(keyboard.OnMouseWheel(Center(keyboard.Rect(*a), dpi), -WHEEL_DELTA, 0) &&
                                keyboard.PreviewPage() == 1,
                            "Large candidate list cannot be paged");
                    const auto neighborRect = keyboard.Rect(*adjacent);
                    const auto neighborX = neighborRect.x + neighborRect.width / 2;
                    const auto neighborY = neighborRect.y + neighborRect.height / 2;
                    Require(neighborX >= preview.box.x && neighborX <= preview.box.x + preview.box.width &&
                                neighborY >= preview.box.y && neighborY <= preview.box.y + preview.box.height,
                            "Fixture must cover the neighbor with the magnified preview");
                    keyboard.OnMouseMove(Center(keyboard.Rect(*adjacent), dpi), 0);
                    Require(keyboard.HoveredKey() == 'S' && keyboard.PreviewPage() == 0,
                            "Enlarged preview intercepted the adjacent key");
                    unsigned chosen = 0;
                    keyboard.choose = [&](unsigned key) { chosen = key; };
                    keyboard.OnMouseDown(Center(keyboard.Rect(*adjacent), dpi), 0);
                    keyboard.OnMouseUp(Center(keyboard.Rect(*adjacent), dpi), 0);
                    Require(chosen == 'S', "Covered neighbor cannot be selected");
                }
    });
    run("IPA skins load in both themes and candidate orientations", [&] {
        for (const std::string id : {"ipa-paper", "ipa-celadon", "ipa-night"})
            for (const std::string theme : {"light", "dark"})
                for (const std::string layout : {"horizontal", "vertical"})
                    Require(CandidateSkinCatalog::LoadSupported(root.parent_path() / "skins", id, layout, theme)
                                .has_value(),
                            "IPA skin failed validation or silently fell back");
    });
    run("symbol page/filter changes cancel a pending press", [&] {
        TestSurface surface;
        SymbolCanvas grid;
        grid.Attach(&surface.window);
        grid.Arrange({0, 0, 360, 230});
        for (int i = 0; i < 90; ++i)
            grid.symbols.push_back({L"a", L"name", std::to_string(i), "ipa", i < 45 ? "vowels" : "other"});
        grid.Filter();
        int count = 0;
        grid.choose = [&](const Symbol &) { ++count; };
        const auto point = Center(grid.Cell(0), 96);
        grid.OnMouseDown(point, 0);
        grid.Next(1);
        grid.OnMouseUp(point, 0);
        Require(count == 0, "Page change selected a symbol that was never pressed");
        grid.OnMouseDown(point, 0);
        grid.section = "other";
        grid.Filter();
        grid.OnMouseUp(point, 0);
        Require(count == 0, "Filter change selected a different symbol");
        grid.OnMouseDown(point, 0);
        grid.OnMouseUp(point, 0);
        Require(count == 1, "Normal symbol selection failed");
    });
    run("structured IPA charts retain every source symbol and hit the clicked cell", [&] {
        std::ifstream stream(root / "plus-catalog.json", std::ios::binary);
        const auto catalog = nlohmann::json::parse(stream);
        TestSurface surface;
        SymbolCanvas chart;
        chart.Attach(&surface.window);
        chart.Load(catalog);
        for (const std::string system : {"ipa", "extipa", "voqs"})
            for (const float dpi : {96.f, 120.f, 144.f, 192.f})
                for (const float scale : {.2f, .6f, .8f, 1.2f})
                {
                    surface.window.SetDpiOverride(dpi);
                    chart.system = system;
                    chart.SetZoom(scale);
                    chart.Arrange({0, 0, 1400, 650});
                    std::set<std::string> expected, actual;
                    for (const auto &symbol : chart.symbols)
                        if (symbol.system == system)
                            expected.insert(symbol.id);
                    for (const auto &tile : chart.Tiles())
                        actual.insert(chart.visible[tile.symbol].id);
                    Require(expected == actual, "Structured chart omitted a catalog entry");
                    for (size_t index : {size_t{0}, chart.Tiles().size() / 2, chart.Tiles().size() - 1})
                    {
                        const auto tile = chart.Tiles()[index];
                        chart.ScrollTo(std::max(0.f, tile.rect.y - 80));
                        std::string output;
                        chart.choose = [&](const Symbol &symbol) { output = symbol.output; };
                        const auto point = Center(chart.Cell(index), dpi);
                        chart.OnMouseDown(point, 0);
                        chart.OnMouseUp(point, 0);
                        Require(output == chart.visible[tile.symbol].output, "Chart DPI/scroll hit another symbol");
                    }
                    chart.choose = {};
                }
        chart.system = "ipa";
        chart.section = "vowels";
        chart.Filter();
        Require(chart.Tiles().size() == 28, "Vowel category lost source positions");
        chart.query = L"不存在的符号名称";
        chart.Filter();
        Require(chart.Tiles().empty(), "Search returned an unrelated symbol");
        Require(!IsWindowVisible(surface.hwnd), "Chart test displayed a desktop window");
    });
    run("IPA Plus fits width, balances tones and reaches the horizontal edge", [&] {
        std::ifstream stream(root / "plus-catalog.json", std::ios::binary);
        const auto catalog = nlohmann::json::parse(stream);
        TestSurface surface;
        SymbolCanvas chart;
        chart.Attach(&surface.window);
        chart.Load(catalog);
        for (const float dpi : {96.f, 120.f, 144.f, 192.f})
            for (const float width : {360.f, 640.f, 820.f, 1200.f})
            {
                surface.window.SetDpiOverride(dpi);
                chart.FitWidth();
                chart.Arrange({0, 0, width, 650});
                Require(chart.ContentWidth() <= width, "Default chart exceeds available width");
                Require(chart.zoom <= .7f, "Fit mode enlarged the default beyond 70%");
                for (const auto &tile : chart.Tiles())
                    if (chart.visible[tile.symbol].section == "tones")
                        Require(tile.rect.x > width / 2, "Tones must occupy the right column");
                chart.SetZoom(.1f);
                Require(std::abs(chart.zoom - .2f) < .001f, "Minimum zoom is not 20%");
                chart.SetZoom(1.2f);
                size_t edge = 0;
                for (size_t i = 1; i < chart.Tiles().size(); ++i)
                    if (chart.Tiles()[i].rect.x > chart.Tiles()[edge].rect.x)
                        edge = i;
                const auto tile = chart.Tiles()[edge];
                chart.ScrollHorizontal(chart.ContentWidth());
                chart.ScrollTo(tile.rect.y);
                std::string chosen;
                chart.choose = [&](const Symbol &symbol) { chosen = symbol.output; };
                const auto point = Center(chart.Cell(edge), dpi);
                chart.OnMouseDown(point, 0);
                chart.OnMouseUp(point, 0);
                Require(chosen == chart.visible[tile.symbol].output, "Rightmost symbol cannot be selected");
                chart.choose = {};
            }
    });
    run("Hanzi and IPA candidate rows activate distinct callbacks", [&] {
        phonetic_ipa::HanziTable hanzi;
        hanzi.Load(root / "beijing-toned.tsv");
        const auto ipa = hanzi.Convert(u8"汉字", "han'zi").text;
        Require(ipa == u8"xan˥˩ tsɹ̩˥˩", "Unexpected real Hanzi transcription");
        TestSurface surface;
        msimeui::CandidateList list;
        list.Attach(&surface.window);
        std::wstring committed;
        list.SetOnItemActivated([&](size_t index) { committed = list.GetItem(index)->text; });
        list.SetOnSecondaryActivated([&](size_t index) { committed = list.GetItem(index)->translation; });
        for (float dpi : {96.f, 120.f, 144.f, 192.f})
            for (float width : {180.f, 360.f})
            {
                surface.window.SetDpiOverride(dpi);
                list.SetItems({{L"1", L"汉字", L"", Widen(ipa), {}, true, L"", L"Doulos SIL"},
                               {L"2", L"\u014b", L"", L"", {}, false, L"Doulos SIL"}});
                const auto size = list.Measure({width, 1000});
                list.Arrange({0, 0, width, size.height});
                const auto first = list.GetItemBounds(0), second = list.GetItemBounds(1);
                Require(first.height > second.height, "IPA was not placed on its own lower line");
                auto top = Center({first.x, first.y, first.width, 8}, dpi);
                auto lower = Center({first.x, first.y + first.height - 8, first.width, 8}, dpi);
                committed.clear();
                list.OnMouseDown(top, 0);
                list.OnMouseUp(top, 0);
                Require(committed == L"汉字", "Hanzi row routed to IPA");
                committed.clear();
                list.OnMouseDown(lower, 0);
                list.OnMouseUp(lower, 0);
                Require(Narrow(committed) == ipa, "IPA row routed to Hanzi or changed Unicode");
                committed.clear();
                list.OnMouseDown(top, 0);
                list.OnMouseUp(lower, 0);
                Require(committed.empty(), "Cross-row release unexpectedly committed");
                committed.clear();
                auto ordinary = Center(second, dpi);
                list.OnMouseDown(ordinary, 0);
                list.OnMouseUp(ordinary, 0);
                Require(committed == L"\u014b", "Single-line CIN item was treated as secondary");
            }
    });
    run("13 enclosed letters shape as complete ligatures", [&] { CheckEnclosedGlyphs(); });
    run("Bundled Doulos primary and IPA Plus fallback map to their actual fonts", [&] {
        Require(msimeui::PrivateIpaFontCollection(L"Doulos SIL") != nullptr, "Doulos private collection missing");
        Require(MappedIpaFamily(L"ɕ", {L"PTB IPA Plus"}) == L"Doulos SIL",
                "Candidate IPA did not retain its Doulos primary font");
        Require(MappedIpaFamily(L"\u27c5", {L"PTB IPA Plus"}) == L"PTB IPA Plus",
                "Candidate extended glyph lost private fallback");
        Require(MappedIpaFamily(L"\u27c5", {}) == L"PTB IPA Plus", "Extended glyph lost private fallback");
        Require(MappedIpaFamily(L"汉", {}) != L"Doulos SIL", "Chinese text cannot fall back to a CJK font");
    });
    run("Long dual-line candidate pages stay inside a scrollable viewport", [&] {
        CandidateSkinScene scene;
        CandidateSkinScene::Settings settings;
        settings.skinId = "fluent";
        settings.fontSize = 24;
        settings.fontFamily = L"Segoe UI";
        settings.pageSize = 9;
        scene.Apply(settings);
        std::vector<msimeui::CandidateList::Item> items;
        for (int i = 1; i <= 9; ++i)
            items.push_back({std::to_wstring(i),
                             L"汉字",
                             L"",
                             L"xan\u02e5\u02e9 ts\u027f\u02e5\u02e9",
                             {},
                             true,
                             L"",
                             L"Doulos SIL"});
        scene.List()->SetItems(items);
        const auto size = scene.Root()->MeasureInLayout({400, 300});
        scene.Root()->ArrangeInLayout({0, 0, size.width, size.height});
        Require(size.height <= 300, "Candidate window exceeded its height limit");
        auto scroll = scene.BodyScroller();
        Require(scroll && scroll->GetMaxScrollOffset() > 0, "Last IPA candidates have no scrolling route");
        scroll->ScrollIntoView(scene.List()->GetItemBounds(8));
        const auto last = scene.List()->GetItemBounds(8), viewport = scroll->GetBounds();
        Require(last.y >= viewport.y - 1 && last.y + last.height <= viewport.y + viewport.height + 1,
                "Last candidate stays outside the visible viewport");
    });
    run("Custom application image is local, decodable and reset preserves the imported file", [&] {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        Require(SUCCEEDED(com), "Initialize image decoder COM");
        try
        {
            const auto directory =
                std::filesystem::temp_directory_path() /
                ("ipa-icon-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
            std::filesystem::create_directories(directory);
            const auto source = root.parent_path() / "skins/bigfish/assets/character.png";
            const auto originalSize = std::filesystem::file_size(source);
            PhoneticIpa::AppIcon::Import(source, directory);
            const auto icon = PhoneticIpa::AppIcon::Current(directory);
            Require(!icon.empty() && std::filesystem::is_regular_file(icon), "Imported icon was not selected");
            Require(std::filesystem::file_size(source) == originalSize, "Original picture was changed");
            const auto invalid = directory / "invalid.png";
            {
                std::ofstream file(invalid);
                file << "not an image";
            }
            bool rejected = false;
            try
            {
                PhoneticIpa::AppIcon::Import(invalid, directory);
            }
            catch (const std::exception &)
            {
                rejected = true;
            }
            Require(rejected && PhoneticIpa::AppIcon::Current(directory) == icon, "Invalid image changed the icon");
            PhoneticIpa::AppIcon::Reset(directory);
            Require(PhoneticIpa::AppIcon::Current(directory).empty(), "Default icon was not restored");
            Require(std::filesystem::is_regular_file(icon), "Reset deleted an imported image");
            PhoneticIpa::WriteProfileFile(directory / "custom-icon-name.txt", "../outside.ico");
            Require(PhoneticIpa::AppIcon::Current(directory).empty(), "Icon path escaped the private directory");
        }
        catch (...)
        {
            CoUninitialize();
            throw;
        }
        CoUninitialize();
    });
    run("All three bundled bigfish skins load through the production catalog", [&] {
        for (const auto *id : {"bigfish", "bigfish-peek", "bigfish-national-day"})
        {
            const auto skin = CandidateSkinCatalog::Load(root.parent_path() / "skins", id);
            Require(skin.has_value(), "Bundled bigfish skin is absent or invalid");
            Require(CandidateSkinCatalog::Supports(*skin, "vertical", "dark"), "Bigfish IPA layout unsupported");
        }
    });
    run("Detailed first-use guide scrolls and keeps all navigation visible at four DPIs", [&] {
        TestSurface surface;
        CheckFirstUsePages(surface);
    });
    POINT pointerAfter{};
    GetCursorPos(&pointerAfter);
    std::cout << "Desktop foreground unchanged: " << (foreground == GetForegroundWindow())
              << "; cursor unchanged: " << (pointerBefore.x == pointerAfter.x && pointerBefore.y == pointerAfter.y)
              << '\n';
    std::cout << passed << " passed, " << failures << " failed; no visible window or injected input\n";
    return failures ? 1 : 0;
}
