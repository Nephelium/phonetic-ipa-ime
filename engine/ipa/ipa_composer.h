#pragma once
#include "ipa_tables.h"
#include "../contracts/ipa_input.h"

namespace phonetic_ipa
{
inline std::vector<std::string> ToneCandidates(std::string_view digits)
{
    if (digits.empty() || digits.size() > 32)
        return {};
    static constexpr const char *bars[] = {u8"˩", u8"˨", u8"˧", u8"˦", u8"˥"};
    static constexpr const char *superscripts[] = {u8"¹", u8"²", u8"³", u8"⁴", u8"⁵"};
    std::string tone, superscript;
    for (const char digit : digits)
    {
        if (digit < '1' || digit > '5')
            return {};
        tone += bars[digit - '1'];
        superscript += superscripts[digit - '1'];
    }
    return {tone, superscript, std::string(digits)};
}
inline std::vector<std::string> InputCandidates(const CinTable &table, const std::string &raw)
{
    if (PhoneticIpaKey::IsToneEntry(raw))
        return ToneCandidates(std::string_view(raw).substr(5));
    // Escaping also makes original table codes named tone:... reachable.
    return table.Lookup(PhoneticIpaKey::HasPrefix(std::string_view(raw), "code:") ? raw.substr(5) : raw);
}
inline bool MayAutoCommit(const CinTable &table, const std::string &raw)
{
    if (PhoneticIpaKey::IsNumericEntry(raw))
        return false;
    for (const std::string_view prefix : {"tone:", "code:"})
        if (raw.size() < prefix.size() && prefix.substr(0, raw.size()) == raw)
            return false;
    return table.Lookup(raw).size() == 1 && !table.HasLongerPrefix(raw);
}
inline std::string EntryHint(const std::string &raw)
{
    if (PhoneticIpaKey::IsToneEntry(raw))
        return InputCandidates(CinTable{}, raw).empty() ? u8"五度调值 · 输入 1–5，如 55、214 · Backspace 修改"
                                                        : u8"五度调值 · ↑↓ 选写法 · 空格确认";
    if (PhoneticIpaKey::IsNumericEntry(raw))
        return u8"完整编码 · 数字参与编码 · ↑↓ 选择 · 空格确认";
    return {};
}
} // namespace phonetic_ipa
