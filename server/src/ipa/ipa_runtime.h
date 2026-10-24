#pragma once

#include "engine/ipa/ipa_composer.h"
#include "engine/ipa/ipa_rules.h"
#include "utils/common_utils.h"
#include <atomic>

namespace PhoneticIpa
{
inline std::filesystem::path ResourceRoot()
{
    return std::filesystem::u8path(CommonUtils::get_ime_data_path()) / "ipa";
}
inline const phonetic_ipa::CinTable &Cin()
{
    static thread_local phonetic_ipa::LiveCinTable table;
    return table.Get(ResourceRoot());
}
inline std::string RuleId()
{
    const auto path = ResourceRoot() / "rule-selection.txt";
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
        return "invalid";
    if (!exists)
        return "beijing";
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > 64)
        return "invalid";
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return "invalid";
    std::string id;
    std::getline(file, id);
    if (!id.empty() && id.back() == '\r')
        id.pop_back();
    return id;
}
inline void SetRuleId(const std::string &id)
{
    // Validate both the catalog entry and its complete table before replacing
    // the preference. An invalid file never changes the current selection.
    (void)phonetic_ipa::LoadTranscriptionRule(ResourceRoot(), id);
    static std::atomic<unsigned> serial{0};
    const auto target = ResourceRoot() / "rule-selection.txt";
    const auto temporary = ResourceRoot() / ("rule-selection-" + std::to_string(GetCurrentProcessId()) + "-" +
                                             std::to_string(++serial) + ".tmp");
    {
        std::ofstream out(temporary, std::ios::binary);
        out << id << '\n';
        out.close();
        if (!out)
            throw std::runtime_error("无法保存转写规则");
    }
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("无法保存转写规则");
}
inline const phonetic_ipa::HanziTable &Hanzi()
{
    struct State
    {
        std::string id;
        phonetic_ipa::HanziTable table;
    };
    static thread_local State state;
    const std::string id = RuleId();
    if (state.id != id)
    {
        try
        {
            state.table = phonetic_ipa::LoadTranscriptionRule(ResourceRoot(), id);
        }
        catch (const std::exception &)
        {
            state.table = {};
        }
        state.id = id;
    }
    return state.table;
}
} // namespace PhoneticIpa
