#include "settings/native/native_settings_pages.h"

namespace native_settings
{
const std::vector<PageInfo> &Pages()
{
    static const std::vector<PageInfo> pages = {
        {"ipa-keyboard", L"音标键盘", L"sidebar/screen_keyboard.svg", &BuildIpaKeyboardPage},
        // 默认打开音标键盘；常规输入设置仍排在外观之前。
        {"input", L"输入", L"sidebar/input.svg", &BuildInputPage},
        {"appearance", L"外观", L"sidebar/appearance.svg", &BuildAppearancePage},
        {"helpcode", L"辅助码", L"sidebar/helpcode.svg", &BuildHelpcodePage},
        {"shortcut", L"快捷键", L"sidebar/shortcut.svg", &BuildShortcutPage},
        {"dict", L"词库", L"sidebar/dictionary.svg", &BuildDictionaryPage},
        {"skin", L"皮肤", L"sidebar/skin.svg", &BuildSkinPage},
        {"voice", L"语音输入", L"sidebar/voice_input.svg", &BuildVoicePage},
        {"screenkb-settings", L"屏幕键盘", L"sidebar/screen_keyboard.svg", &BuildScreenKeyboardPage},
        {"handwriting-settings", L"手写识别板", L"sidebar/handwriting.svg", &BuildHandwritingPage},
        {"tools-settings", L"实用功能", L"sidebar/utilities.svg", &BuildToolsPage},
        {"ai-settings", L"AI 与云联想", L"sidebar/ai.svg", &BuildAiPage},
        {"floating-toolbar", L"悬浮工具栏", L"sidebar/floating_toolbar.svg", &BuildFloatingToolbarPage},
        {"stats", L"统计", L"sidebar/stats.svg", &BuildStatsPage},
        {"help-settings", L"帮助", L"sidebar/help.svg", &BuildHelpPage},
        {"about-settings", L"关于", L"sidebar/about.svg", &BuildAboutPage},
        {"feedback-settings", L"反馈", L"sidebar/feedback.svg", &BuildFeedbackPage},
    };
    return pages;
}
} // namespace native_settings
