#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PhoneticIpaKey
{
constexpr bool IsReadingChoiceKey(unsigned key, unsigned modifiers)
{
    return key == 0x0d && (modifiers & 7u) == 3u; // Ctrl+Shift+Enter
}
// Prefixes are kept in the ordinary composition buffer, so synchronous and
// queued TSF classification observe the same state without another mode bit.
template <typename Char> constexpr bool HasPrefix(std::basic_string_view<Char> raw, std::string_view prefix)
{
    if (raw.size() < prefix.size())
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
        if (raw[i] != static_cast<Char>(prefix[i]))
            return false;
    return true;
}
template <typename Char> constexpr bool NumericEntry(std::basic_string_view<Char> raw)
{
    return HasPrefix(raw, "tone:") || HasPrefix(raw, "code:");
}
constexpr bool IsNumericEntry(std::string_view raw)
{
    return NumericEntry(raw);
}
constexpr bool IsNumericEntry(std::wstring_view raw)
{
    return NumericEntry(raw);
}
constexpr bool IsToneEntry(std::string_view raw)
{
    return HasPrefix(raw, "tone:");
}
constexpr bool EditableChord(unsigned modifiers, unsigned key)
{
    const bool printableKey = (key >= 0x30 && key <= 0x5a) || (key >= 0x60 && key <= 0x6f) ||
                              (key >= 0x70 && key <= 0x87) || (key >= 0xba && key <= 0xc0) ||
                              (key >= 0xdb && key <= 0xde);
    return modifiers >= 2 && modifiers <= 7 && printableKey && !(modifiers == 3 && (key == 'I' || key == 'F')) &&
           !(modifiers == 2 && ((key >= '1' && key <= '9') || (key >= 0x61 && key <= 0x69))) &&
           !(modifiers == 4 && key == 0x73); // Alt+F4 closes the foreground window.
}
// Ordinary CIN input keeps 1-9 for selection and the existing zero behavior.
// Explicit tone:/code: entries accept digits; Space confirms the chosen output.
constexpr bool IsInput(bool enabled, std::uint32_t character, std::size_t raw_length, bool numeric_entry = false)
{
    return enabled && character >= 0x21 && character <= 0x7e &&
           !(raw_length != 0 && !numeric_entry && character >= '0' && character <= '9');
}
} // namespace PhoneticIpaKey
