#pragma once

#include "engine/ipa/ipa_composer.h"
#include "engine/ipa/ipa_rules.h"
#include "utils/common_utils.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace PhoneticIpa
{
inline std::filesystem::path ResourceRoot()
{
    return std::filesystem::u8path(CommonUtils::get_ime_data_path()) / "ipa";
}
inline phonetic_ipa::LiveCinTable &CinCache()
{
    static thread_local phonetic_ipa::LiveCinTable table;
    return table;
}
inline const phonetic_ipa::CinTable &Cin()
{
    return CinCache().Get(ResourceRoot());
}
// True when the user's active.cin exists but could not be loaded, so the
// default table is in use. Settings show this instead of failing silently.
inline bool ActiveCinFailed()
{
    try
    {
        (void)Cin();
    }
    catch (const std::exception &)
    {
        return true;
    }
    return CinCache().ActiveFailed();
}

namespace detail
{
using Clock = std::chrono::steady_clock;
// One process-wide cache shared by every thread. Tables are immutable once
// published, so readers hold a shared_ptr and a later switch never frees a table
// that is still in use. The cache is intentionally leaked so a late reader during
// process shutdown cannot touch a destroyed object.
struct RuleCache
{
    std::mutex mutex;
    std::mutex load_mutex;
    std::string id;
    bool id_valid = false;
    Clock::time_point id_checked{};
    std::shared_ptr<const phonetic_ipa::HanziTable> table;
    std::string table_id;
    std::shared_ptr<const phonetic_ipa::HanziTable::Words> words;
    bool words_loaded = false;
    std::string failed_id;
    Clock::time_point failed_at{};
    std::string error;
};
inline RuleCache &Rules()
{
    static auto *cache = new RuleCache;
    return *cache;
}
// The rule file is re-read at most this often. In-process changes and the
// WM_IPA_RULE_CHANGED broadcast update the cache immediately.
inline constexpr auto kRuleIdRecheck = std::chrono::seconds(2);
// A failed rule table is retried after this delay instead of staying empty
// until the next rule change or restart.
inline constexpr auto kRuleRetryDelay = std::chrono::seconds(5);

inline std::string ReadRuleIdFile()
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
inline std::shared_ptr<const phonetic_ipa::HanziTable::Words> SharedWords()
{
    auto &cache = Rules();
    {
        std::lock_guard lock(cache.mutex);
        if (cache.words_loaded)
            return cache.words;
    }
    const auto path = ResourceRoot() / "word-readings.tsv";
    std::error_code error;
    std::shared_ptr<const phonetic_ipa::HanziTable::Words> words;
    if (std::filesystem::exists(path, error))
        words = phonetic_ipa::HanziTable::ReadWords(path);
    std::lock_guard lock(cache.mutex);
    cache.words = words;
    cache.words_loaded = true;
    return words;
}
inline std::shared_ptr<const phonetic_ipa::HanziTable> BuildTable(phonetic_ipa::HanziTable characters)
{
    auto table = std::make_shared<phonetic_ipa::HanziTable>(std::move(characters));
    table->SetWords(SharedWords());
    return table;
}
inline const std::shared_ptr<const phonetic_ipa::HanziTable> &EmptyTable()
{
    static const auto *empty =
        new std::shared_ptr<const phonetic_ipa::HanziTable>(std::make_shared<const phonetic_ipa::HanziTable>());
    return *empty;
}
} // namespace detail

inline std::string RuleId()
{
    auto &cache = detail::Rules();
    std::lock_guard lock(cache.mutex);
    const auto now = detail::Clock::now();
    if (!cache.id_valid || now - cache.id_checked >= detail::kRuleIdRecheck)
    {
        cache.id = detail::ReadRuleIdFile();
        cache.id_valid = true;
        cache.id_checked = now;
    }
    return cache.id;
}
// Called when another process announces a rule change.
inline void InvalidateRuleId()
{
    auto &cache = detail::Rules();
    std::lock_guard lock(cache.mutex);
    cache.id_valid = false;
    cache.failed_id.clear();
}
inline void SetRuleId(const std::string &id)
{
    // Validate the catalog entry and its character table before replacing the
    // preference. An invalid file never changes the current selection. The
    // shared word table is not reparsed, so this stays cheap on UI threads.
    auto characters = phonetic_ipa::LoadTranscriptionRule(ResourceRoot(), id, false);
    static std::atomic<unsigned> serial{0};
    const auto target = ResourceRoot() / "rule-selection.txt";
    const auto temporary = ResourceRoot() / ("rule-selection-" + std::to_string(GetCurrentProcessId()) + "-" +
                                             std::to_string(++serial) + ".tmp");
    {
        std::ofstream out(temporary, std::ios::binary);
        out << id << '\n';
        out.close();
        if (!out)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error("无法保存转写规则");
        }
    }
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("无法保存转写规则");
    }
    auto &cache = detail::Rules();
    bool words_loaded = false;
    {
        std::lock_guard lock(cache.mutex);
        cache.id = id;
        cache.id_valid = true;
        cache.id_checked = detail::Clock::now();
        cache.failed_id.clear();
        cache.error.clear();
        words_loaded = cache.words_loaded;
    }
    // Reuse the validated table directly when the word table is already in
    // memory; otherwise the first Hanzi() call completes it lazily.
    if (words_loaded)
    {
        auto table = detail::BuildTable(std::move(characters));
        std::lock_guard lock(cache.mutex);
        if (cache.id == id)
        {
            cache.table = std::move(table);
            cache.table_id = id;
        }
    }
}
inline std::shared_ptr<const phonetic_ipa::HanziTable> Hanzi()
{
    const std::string id = RuleId();
    auto &cache = detail::Rules();
    {
        std::lock_guard lock(cache.mutex);
        if (cache.table && cache.table_id == id)
            return cache.table;
        if (cache.failed_id == id && detail::Clock::now() - cache.failed_at < detail::kRuleRetryDelay)
            return detail::EmptyTable();
    }
    std::lock_guard load(cache.load_mutex);
    {
        std::lock_guard lock(cache.mutex);
        if (cache.table && cache.table_id == id)
            return cache.table;
    }
    try
    {
        auto table = detail::BuildTable(phonetic_ipa::LoadTranscriptionRule(ResourceRoot(), id, false));
        std::lock_guard lock(cache.mutex);
        cache.table = table;
        cache.table_id = id;
        cache.failed_id.clear();
        cache.error.clear();
        return table;
    }
    catch (const std::exception &e)
    {
        std::lock_guard lock(cache.mutex);
        cache.failed_id = id;
        cache.failed_at = detail::Clock::now();
        cache.error = e.what();
        return detail::EmptyTable();
    }
}
// True while the selected rule table cannot be loaded. Candidates then show a
// visible hint instead of silently ignoring the selection key.
inline bool HanziFailed()
{
    const std::string id = RuleId();
    auto &cache = detail::Rules();
    std::lock_guard lock(cache.mutex);
    return !cache.failed_id.empty() && cache.failed_id == id;
}
inline std::string HanziError()
{
    auto &cache = detail::Rules();
    std::lock_guard lock(cache.mutex);
    return cache.error;
}
} // namespace PhoneticIpa
