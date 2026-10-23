#pragma once
#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>

namespace PhoneticIpa
{
inline std::string LayoutForReportedKeyCount(unsigned count)
{
    if (count >= 101 && count <= 109)
        return "104";
    if (count == 87 || count == 88)
        return "87";
    if (count >= 75 && count <= 84)
        return "75";
    if (count >= 64 && count <= 70)
        return "65";
    if (count >= 60 && count <= 62)
        return "60";
    return {};
}
inline std::vector<unsigned> ReportedKeyboardSizes()
{
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0 || count > 256)
        return {};
    std::vector<RAWINPUTDEVICELIST> devices(count);
    const auto result = GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (result == static_cast<UINT>(-1))
        return {};
    std::vector<unsigned> sizes;
    for (UINT i = 0; i < result; ++i)
    {
        if (devices[i].dwType != RIM_TYPEKEYBOARD)
            continue;
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof(info);
        UINT bytes = sizeof(info);
        if (GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICEINFO, &info, &bytes) != static_cast<UINT>(-1) &&
            !LayoutForReportedKeyCount(info.keyboard.dwNumberOfKeysTotal).empty() &&
            std::find(sizes.begin(), sizes.end(), info.keyboard.dwNumberOfKeysTotal) == sizes.end())
            sizes.push_back(info.keyboard.dwNumberOfKeysTotal);
    }
    return sizes;
}
inline std::wstring KeyLegend(const std::string &s)
{
    return std::wstring(s.begin(), s.end());
}
struct KeyCap
{
    unsigned vk;
    std::wstring label;
    float x, y, w = 1, h = 1;
};

inline std::string KeyCode(unsigned vk, bool shift)
{
    BYTE state[256]{};
    if (shift)
        state[VK_SHIFT] = 0x80;
    WCHAR buffer[8]{};
    const HKL layout = GetKeyboardLayout(0);
    const int length = ToUnicodeEx(vk, MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC, layout), state, buffer, 8, 4, layout);
    if (length == 1 && buffer[0] >= 33 && buffer[0] <= 126)
        return std::string(1, static_cast<char>(buffer[0]));
    return {};
}

inline std::vector<KeyCap> KeyboardKeys(const std::string &size)
{
    std::vector<KeyCap> keys;
    const bool function = size == "104" || size == "87" || size == "75";
    const bool compact = size == "75" || size == "65";
    const float top = function ? 1.3f : 0.0f;
    auto add = [&](unsigned key, std::wstring label, float x, float y, float w = 1, float h = 1) {
        keys.push_back({key, std::move(label), x, y, w, h});
    };
    if (function)
    {
        add(VK_ESCAPE, L"Esc", 0, 0);
        for (int i = 0; i < 12; ++i)
            add(VK_F1 + i, L"F" + std::to_wstring(i + 1), 2.0f + i, 0);
    }
    const unsigned numberKeys[] = {VK_OEM_3, '1', '2', '3', '4',          '5',        '6',
                                   '7',      '8', '9', '0', VK_OEM_MINUS, VK_OEM_PLUS};
    for (int i = 0; i < 13; ++i)
        add(numberKeys[i], KeyLegend(KeyCode(numberKeys[i], false)), static_cast<float>(i), top);
    add(VK_BACK, L"Back", 13, top, 2);
    add(VK_TAB, L"Tab", 0, top + 1, 1.5f);
    const unsigned row1[] = {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', VK_OEM_4, VK_OEM_6, VK_OEM_5};
    for (int i = 0; i < 13; ++i)
        add(row1[i], KeyLegend(KeyCode(row1[i], false)), 1.5f + i, top + 1, i == 12 ? 1.5f : 1);
    add(VK_CAPITAL, L"Caps", 0, top + 2, 1.75f);
    const unsigned row2[] = {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', VK_OEM_1, VK_OEM_7};
    for (int i = 0; i < 11; ++i)
        add(row2[i], KeyLegend(KeyCode(row2[i], false)), 1.75f + i, top + 2);
    add(VK_RETURN, L"Enter", 12.75f, top + 2, 2.25f);
    add(VK_SHIFT, L"Shift", 0, top + 3, 2.25f);
    const unsigned row3[] = {'Z', 'X', 'C', 'V', 'B', 'N', 'M', VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2};
    for (int i = 0; i < 10; ++i)
        add(row3[i], KeyLegend(KeyCode(row3[i], false)), 2.25f + i, top + 3);
    add(VK_SHIFT, L"Shift", 12.25f, top + 3, compact ? 1.75f : 2.75f);
    add(VK_CONTROL, L"Ctrl", 0, top + 4, 1.25f);
    add(VK_LWIN, L"Win", 1.25f, top + 4, 1.25f);
    add(VK_MENU, L"Alt", 2.5f, top + 4, 1.25f);
    add(VK_SPACE, L"Space", 3.75f, top + 4, 6.25f);
    if (compact)
    {
        add(VK_MENU, L"Alt", 10, top + 4);
        add(VK_APPS, L"Fn", 11, top + 4);
        add(VK_CONTROL, L"Ctrl", 12, top + 4);
        add(VK_LEFT, L"←", 13, top + 4);
        add(VK_DOWN, L"↓", 14, top + 4);
        add(VK_RIGHT, L"→", 15, top + 4);
        add(VK_UP, L"↑", 14, top + 3);
        add(VK_DELETE, L"Del", 15, top);
        add(VK_PRIOR, L"PgUp", 15, top + 1);
        add(VK_NEXT, L"PgDn", 15, top + 2);
        add(VK_END, L"End", 15, top + 3);
    }
    else
    {
        add(VK_MENU, L"Alt", 10, top + 4, 1.25f);
        add(VK_RWIN, L"Win", 11.25f, top + 4, 1.25f);
        add(VK_APPS, L"Menu", 12.5f, top + 4, 1.25f);
        add(VK_CONTROL, L"Ctrl", 13.75f, top + 4, 1.25f);
    }
    if (size == "104" || size == "87")
    {
        const float x = 15.4f;
        add(VK_UP, L"↑", x + 1, top + 3);
        add(VK_LEFT, L"←", x, top + 4);
        add(VK_DOWN, L"↓", x + 1, top + 4);
        add(VK_RIGHT, L"→", x + 2, top + 4);
        add(VK_DELETE, L"Del", x, top + 1);
        add(VK_PRIOR, L"PgUp", x + 1, top + 1);
        add(VK_NEXT, L"PgDn", x + 2, top + 1);
        if (size == "104" || size == "87")
        {
            add(VK_INSERT, L"Ins", x, top);
            add(VK_HOME, L"Home", x + 1, top);
            add(VK_END, L"End", x + 2, top);
            add(VK_SNAPSHOT, L"Prt", x, 0);
            add(VK_SCROLL, L"Scr", x + 1, 0);
            add(VK_PAUSE, L"Pause", x + 2, 0);
        }
    }
    if (size == "104")
    {
        const float x = 19;
        add(VK_NUMLOCK, L"Num", x, top);
        add(VK_DIVIDE, L"/", x + 1, top);
        add(VK_MULTIPLY, L"*", x + 2, top);
        add(VK_SUBTRACT, L"−", x + 3, top);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
            {
                unsigned digit = 7 - row * 3 + col;
                add(VK_NUMPAD0 + digit, std::to_wstring(digit), x + col, top + 1 + row);
            }
        add(VK_ADD, L"+", x + 3, top + 1, 1, 2);
        add(VK_RETURN, L"Enter", x + 3, top + 3, 1, 2);
        add(VK_NUMPAD0, L"0", x, top + 4, 2);
        add(VK_DECIMAL, L".", x + 2, top + 4);
    }
    return keys;
}

inline float KeyboardUnits(const std::string &size)
{
    return size == "104" ? 23.0f : size == "60" ? 15.0f : (size == "75" || size == "65") ? 16.0f : 18.4f;
}
struct KeyRectangle
{
    float x, y, width, height;
};
inline KeyRectangle KeyBounds(const KeyCap &key, const std::string &size, float width, float progress = 0)
{
    const float u = width / KeyboardUnits(size), grow = 2 * progress;
    return {key.x * u + 4 - grow, key.y * 64 + 4 - grow, key.w * u - 8 + 2 * grow, key.h * 64 - 8 + 2 * grow};
}
} // namespace PhoneticIpa
