#pragma once

#include "utils/common_utils.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>

// Per-account data directory (2026-11-01 review H1/H2).
//
// The installer puts the program and its factory resources under Program Files, which only
// administrators can change, and records that factory directory as FactoryDataDir. Personal data
// lives in each account's %LOCALAPPDATA%\phonetic-ipa-prototype. Seed() copies factory files that
// the account does not have yet, and refreshes factory-owned files after an upgrade (when
// factory-stamp.txt changes). Personal files are never overwritten once they exist.
namespace PhoneticIpa::UserData
{
inline std::wstring Lower(std::wstring text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });
    return text;
}

inline bool IsPersonal(const std::filesystem::path &relative)
{
    const std::wstring path = Lower(relative.generic_wstring());
    const std::wstring name = Lower(relative.filename().wstring());
    for (const wchar_t *exact : {L"config.toml", L"user_dict.dat", L"ipa/active.cin", L"ipa/active.cin.previous",
                                 L"ipa/rule-selection.txt", L"ipa/custom-icon-name.txt"})
        if (path == exact)
            return true;
    if (name.rfind(L"msime_user.db", 0) == 0)
        return true;
    if (path.rfind(L"ipa/", 0) == 0)
        for (const wchar_t *prefix : {L"user", L"custom", L"onboarding", L"update"})
            if (name.rfind(prefix, 0) == 0)
                return true;
    // Leftovers of earlier save and upgrade schemes are personal copies as well.
    for (const wchar_t *marker : {L".backup-", L".pending-", L".before-upgrade-", L".before-"})
        if (name.find(marker) != std::wstring::npos)
            return true;
    return false;
}

inline std::string ReadStamp(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    std::string text;
    std::getline(input, text);
    if (!text.empty() && text.back() == '\r')
        text.pop_back();
    if (text.empty() || text.size() > 128)
        return {};
    for (const unsigned char ch : text)
    {
        const bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '.' ||
                        ch == '_' || ch == '-';
        if (!ok)
            return {};
    }
    return text;
}

// Copies through a temporary name so a reader never sees a half-written resource.
inline bool CopyResource(const std::filesystem::path &from, const std::filesystem::path &to)
{
    static std::atomic<unsigned> serial{0};
    std::error_code error;
    std::filesystem::create_directories(to.parent_path(), error);
    const auto temporary = std::filesystem::path(to.wstring() + L".seed-" + std::to_wstring(GetCurrentProcessId()) +
                                                 L"-" + std::to_wstring(++serial));
    if (!CopyFileW(from.c_str(), temporary.c_str(), FALSE))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    const DWORD temp_attrs = GetFileAttributesW(temporary.c_str());
    if (temp_attrs != INVALID_FILE_ATTRIBUTES && (temp_attrs & FILE_ATTRIBUTE_READONLY) != 0)
        SetFileAttributesW(temporary.c_str(), temp_attrs & ~FILE_ATTRIBUTE_READONLY);
    const DWORD target_attrs = GetFileAttributesW(to.c_str());
    if (target_attrs != INVALID_FILE_ATTRIBUTES && (target_attrs & FILE_ATTRIBUTE_READONLY) != 0)
        SetFileAttributesW(to.c_str(), target_attrs & ~FILE_ATTRIBUTE_READONLY);
    if (!MoveFileExW(temporary.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

// The engine writes its runtime databases (msime.db, english.db, ...), so they may hold learned
// data. They are replaced only when the factory dictionary itself changes, keeping the old copy.
inline bool IsEngineDatabase(const std::filesystem::path &relative)
{
    return relative.parent_path().empty() && Lower(relative.extension().wstring()) == L".db";
}

inline bool SameFile(const std::filesystem::path &a, const std::filesystem::path &b)
{
    std::error_code error;
    const auto size_a = std::filesystem::file_size(a, error);
    if (error)
        return false;
    const auto size_b = std::filesystem::file_size(b, error);
    if (error || size_a != size_b)
        return false;
    const auto time_a = std::filesystem::last_write_time(a, error);
    if (error)
        return false;
    const auto time_b = std::filesystem::last_write_time(b, error);
    return !error && time_a == time_b; // CopyFileW keeps the source timestamp.
}

namespace detail
{
struct ScopedSeedMutex
{
    HANDLE handle = nullptr;
    bool locked = false;
    explicit ScopedSeedMutex(const wchar_t *name) : handle(CreateMutexW(nullptr, FALSE, name))
    {
        if (handle)
        {
            const DWORD wait = WaitForSingleObject(handle, 60000);
            locked = (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED);
        }
    }
    ~ScopedSeedMutex()
    {
        if (handle)
        {
            if (locked)
                ReleaseMutex(handle);
            CloseHandle(handle);
        }
    }
};

inline bool WriteStampAtomically(const std::filesystem::path &path, const std::string &stamp)
{
    static std::atomic<unsigned> serial{0};
    const auto temporary = std::filesystem::path(path.wstring() + L".seed-" + std::to_wstring(GetCurrentProcessId()) +
                                                 L"-" + std::to_wstring(++serial));
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out << stamp << '\n';
        out.flush();
        if (!out)
        {
            out.close();
            DeleteFileW(temporary.c_str());
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}
} // namespace detail

inline void SeedFrom(const std::filesystem::path &factory, const std::filesystem::path &user)
{
    std::error_code error;
    if (user.empty() || !std::filesystem::is_directory(factory, error) ||
        std::filesystem::equivalent(factory, user, error))
        return;
    // The Server and the settings process may both start at logon.
    detail::ScopedSeedMutex mutex(L"Local\\PhoneticIpa_SeedUserData");
    if (!mutex.locked)
        return;
    const std::string stamp = ReadStamp(factory / "factory-stamp.txt");
    const bool refresh = !stamp.empty() && stamp != ReadStamp(user / "factory-stamp.txt");
    const std::filesystem::path manifest = "dictionary-manifest.json";
    const bool dictionary_changed =
        refresh && std::filesystem::exists(factory / manifest, error) && !SameFile(factory / manifest, user / manifest);
    bool complete = true;
    std::error_code walk_error;
    for (auto it = std::filesystem::recursive_directory_iterator(
             factory, std::filesystem::directory_options::skip_permission_denied, walk_error);
         !walk_error && it != std::filesystem::recursive_directory_iterator(); it.increment(walk_error))
    {
        std::error_code item_error;
        if (!it->is_regular_file(item_error))
            continue;
        const auto relative = std::filesystem::relative(it->path(), factory, item_error);
        // The dictionary manifest is written last, so an interrupted database update is retried.
        if (item_error || relative.empty() || relative == "factory-stamp.txt" || relative == manifest)
            continue;
        const auto target = user / relative;
        if (!std::filesystem::exists(target, item_error))
        {
            complete = CopyResource(it->path(), target) && complete;
            continue;
        }
        if (!refresh || IsPersonal(relative) || SameFile(it->path(), target))
            continue;
        if (IsEngineDatabase(relative))
        {
            if (!dictionary_changed)
                continue;
            const auto kept = std::filesystem::path(target.wstring() + L".before-upgrade-" +
                                                    std::filesystem::u8path(stamp).wstring());
            if (!std::filesystem::exists(kept, item_error) && !CopyFileW(target.c_str(), kept.c_str(), TRUE))
            {
                complete = false;
                continue;
            }
            bool companion_ok = true;
            for (const wchar_t *suffix : {L"-wal", L"-shm", L"-journal"})
            {
                const auto companion = std::filesystem::path(target.wstring() + suffix);
                std::error_code companion_error;
                if (!std::filesystem::exists(companion, companion_error))
                    continue;
                const auto kept_companion = std::filesystem::path(kept.wstring() + suffix);
                if (!std::filesystem::exists(kept_companion, companion_error))
                    CopyFileW(companion.c_str(), kept_companion.c_str(), TRUE);
                std::filesystem::remove(companion, companion_error);
                if (std::filesystem::exists(companion, companion_error))
                {
                    companion_ok = false;
                    break;
                }
            }
            if (!companion_ok)
            {
                complete = false;
                continue;
            }
        }
        complete = CopyResource(it->path(), target) && complete;
    }
    if (std::filesystem::exists(factory / manifest, error) &&
        (!std::filesystem::exists(user / manifest, error) || (refresh && complete)))
        complete = CopyResource(factory / manifest, user / manifest) && complete;
    if (refresh && complete && !walk_error)
        detail::WriteStampAtomically(user / "factory-stamp.txt", stamp);
}

inline void Seed()
{
    const std::wstring factory = CommonUtils::get_factory_data_path_w();
    if (!factory.empty())
        SeedFrom(factory, CommonUtils::get_ime_data_path_w());
}
} // namespace PhoneticIpa::UserData
