#pragma once

#include "engine/ipa/ipa_composer.h"
#include "utils/common_utils.h"

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
inline const phonetic_ipa::HanziTable &Hanzi()
{
    static const auto table = [] {
        phonetic_ipa::HanziTable t;
        t.Load(ResourceRoot() / "beijing-toned.tsv");
        return t;
    }();
    return table;
}
} // namespace PhoneticIpa
