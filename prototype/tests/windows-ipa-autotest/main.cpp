// Automated real-TSF check for the 2026-11-01 review fixes.
//
// Safety boundary: the tool creates its own EDIT window, activates the IPA input
// method for this process only (TF_IPPMF_FORPROCESS), and synthesizes keys only
// while its own window is the foreground window. If focus moves to any other
// window it stops immediately, so keys never reach another application. Caps Lock
// is restored to its starting state on exit. Results go to stdout and a UTF-8 file.
#include <windows.h>
#include <msctf.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static HWND g_window, g_edit;
static bool g_initial_caps = false;

static std::string Utf8(const std::wstring &text)
{
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                          nullptr, nullptr);
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr,
                        nullptr);
    return result;
}

static void Pump(DWORD milliseconds)
{
    const ULONGLONG until = GetTickCount64() + milliseconds;
    MSG message;
    while (GetTickCount64() < until)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(5);
    }
}

static void RequireForeground()
{
    if (GetForegroundWindow() != g_window)
        throw std::runtime_error("test window lost focus; stopped before sending more keys");
}

static void Key(WORD vk, bool up)
{
    RequireForeground();
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    if (SendInput(1, &input, sizeof(input)) != 1)
        throw std::runtime_error("SendInput failed");
    Pump(45);
}

static void Tap(WORD vk)
{
    Key(vk, false);
    Key(vk, true);
}

static void ShiftTap(WORD vk)
{
    Key(VK_LSHIFT, false);
    Tap(vk);
    Key(VK_LSHIFT, true);
}

static void Type(const std::string &keys)
{
    for (const char ch : keys)
        Tap(ch == '\'' ? VK_OEM_7 : static_cast<WORD>(ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch));
}

static bool CapsOn()
{
    return (GetKeyState(VK_CAPITAL) & 1) != 0;
}

static void SetCaps(bool on)
{
    for (int attempt = 0; attempt < 3 && CapsOn() != on; ++attempt)
    {
        Tap(VK_CAPITAL);
        Pump(250);
    }
}

static std::wstring Text()
{
    std::wstring text(static_cast<size_t>(GetWindowTextLengthW(g_edit)) + 1, L'\0');
    text.resize(static_cast<size_t>(GetWindowTextW(g_edit, text.data(), static_cast<int>(text.size()))));
    return text;
}

static void Clear()
{
    Tap(VK_ESCAPE); // ends any pending composition
    Pump(200);
    SetWindowTextW(g_edit, L"");
    Pump(100);
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_DESTROY)
        PostQuitMessage(0);
    return DefWindowProcW(window, message, w, l);
}

int wmain(int argc, wchar_t **argv)
{
    const std::wstring evidence = argc > 1 ? argv[1] : L"ipa-autotest.txt";
    const bool show_only = argc > 2 && std::wstring(argv[2]) == L"--show";
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW cls{};
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"PhoneticIPA.AutoTest";
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&cls);
    g_window = CreateWindowW(cls.lpszClassName, L"国际音标输入法 · 自动实测（请勿操作键盘）", WS_OVERLAPPEDWINDOW,
                             120, 120, 900, 420, nullptr, nullptr, cls.hInstance, nullptr);
    g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
                             16, 16, 850, 340, g_window, nullptr, cls.hInstance, nullptr);
    HFONT font = CreateFontW(-28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SendMessageW(g_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    ShowWindow(g_window, SW_SHOW);
    // Bring our own window forward; the foreground check guards every key.
    const DWORD foreground = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    AttachThreadInput(GetCurrentThreadId(), foreground, TRUE);
    SetForegroundWindow(g_window);
    AttachThreadInput(GetCurrentThreadId(), foreground, FALSE);
    SetFocus(g_edit);
    Pump(500);

    std::vector<std::pair<std::string, bool>> results;
    std::ofstream log(evidence, std::ios::binary);
    auto record = [&](const std::string &name, const std::wstring &got, bool ok) {
        const std::string line = (ok ? "PASS " : "FAIL ") + name + " => [" + Utf8(got) + "]";
        std::cout << line << std::endl;
        log << line << "\n";
        results.push_back({name, ok});
    };
    int exit_code = 0;
    try
    {
        ITfInputProcessorProfileMgr *manager = nullptr;
        if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ITfInputProcessorProfileMgr, reinterpret_cast<void **>(&manager))))
            throw std::runtime_error("TSF profile manager unavailable");
        CLSID clsid{};
        GUID profile{};
        CLSIDFromString(L"{D95EFABF-8AEC-5396-AEA4-A4E47D5F21BF}", &clsid);
        CLSIDFromString(L"{881BB948-8042-590E-AAC7-77D38BAFB1E6}", &profile);
        const HRESULT activated = manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0804, clsid, profile,
                                                           nullptr, TF_IPPMF_FORPROCESS |
                                                                        TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
        manager->Release();
        if (FAILED(activated))
            throw std::runtime_error("could not activate the IPA input method for this process");
        Pump(1500); // first activation starts the Server
        RequireForeground();
        g_initial_caps = CapsOn();
        SetCaps(false);
        Clear();
        if (show_only)
        {
            // Holds three states for screenshots of the toolbar and candidate hint.
            Type("hao");
            Pump(500);
            ShiftTap('2');
            Pump(4000);
            Clear();
            SetCaps(true);
            Type("hao");
            Pump(4000);
            Clear();
            SetCaps(false);
            Tap(VK_LSHIFT);
            Pump(300);
            Type("a");
            Pump(4000);
            Clear();
            Tap(VK_LSHIFT);
            Pump(300);
            throw std::runtime_error("show mode finished");
        }

        Type("kuaile");
        Pump(600);
        Tap(VK_SPACE);
        Pump(700);
        auto text = Text();
        // The active rule may be broad or narrow; both start with an aspirated k and use falling tones.
        record("pinyin kuaile + Space commits the word IPA", text,
               text.rfind(L"kʰ", 0) == 0 && text.find(L"˥˩") != std::wstring::npos);
        Clear();

        for (const WORD digit : {WORD('2'), WORD('1'), WORD('4')})
            ShiftTap(digit);
        Pump(500);
        text = Text();
        record("Shift+2,1,4 with empty composition types tone letters", text, text == L"˨˩˦");
        Clear();

        SetCaps(true);
        Type("hao");
        Pump(600);
        Tap(VK_SPACE);
        Pump(700);
        text = Text();
        record("Caps Lock pinyin hao + Space commits Hanzi", text, text == L"好");
        Clear();
        ShiftTap('1');
        Pump(500);
        text = Text();
        record("Caps Lock Shift+1 types punctuation (M3)", text, text == L"！" || text == L"!");
        Clear();
        Type("hao");
        Pump(600);
        ShiftTap('1');
        Pump(800);
        text = Text();
        record("Caps Lock pinyin + Shift+1 keeps punctuation behaviour", text,
               !text.empty() && (text.back() == L'！' || text.back() == L'!'));
        Clear();
        SetCaps(false);
        Pump(300);

        Type("hao");
        Pump(600);
        ShiftTap('2');
        Pump(600);
        text = Text();
        record("IPA output pinyin + Shift+2 keeps the composition (hint shown)", text, text.empty());
        Clear();

        Tap(VK_LSHIFT); // lone Shift: direct IPA keyboard
        Pump(500);
        Type("a");
        Pump(500);
        ShiftTap('2');
        Pump(700);
        text = Text();
        record("direct IPA a + Shift+2 commits symbol and tone (M3)", text, text == L"a˨");
        Clear();
        Tap(VK_LSHIFT); // back to pinyin
        Pump(500);

        Type("wo'men'xi'an");
        Pump(700);
        Tap(VK_ESCAPE);
        Pump(300);
        Type("kuaile");
        Pump(600);
        Tap(VK_SPACE);
        Pump(700);
        text = Text();
        record("pinyin still works after the mode round trip", text, text.rfind(L"kʰ", 0) == 0);
        Clear();
    }
    catch (const std::exception &e)
    {
        std::cout << "STOPPED " << e.what() << std::endl;
        log << "STOPPED " << e.what() << "\n";
        exit_code = 2;
    }
    try
    {
        if (GetForegroundWindow() == g_window)
            SetCaps(g_initial_caps);
    }
    catch (...)
    {
    }
    for (const auto &result : results)
        if (!result.second && exit_code == 0)
            exit_code = 1;
    DestroyWindow(g_window);
    DeleteObject(font);
    CoUninitialize();
    return exit_code;
}
