#include "settings/native/native_settings_pages.h"

namespace native_settings
{
const std::vector<PageInfo> &Pages()
{
    static const std::vector<PageInfo> pages = {
        {"ipa-keyboard", L"音标键盘", L"sidebar/screen_keyboard.svg", &BuildIpaKeyboardPage},
        {"input", L"输入与转写", L"sidebar/input.svg", &BuildIpaInputPage},
        {"appearance", L"外观", L"sidebar/appearance.svg", &BuildAppearancePage},
        {"skin", L"皮肤", L"sidebar/skin.svg", &BuildSkinPage},
        {"floating-toolbar", L"悬浮工具栏", L"sidebar/floating_toolbar.svg", &BuildFloatingToolbarPage},
        {"help-settings", L"使用说明", L"sidebar/help.svg", &BuildIpaHelpPage},
        {"about-settings", L"关于", L"sidebar/about.svg", &BuildAboutPage},
    };
    return pages;
}
} // namespace native_settings
