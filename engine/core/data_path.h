#pragma once

#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <shlobj.h>
#endif

namespace metasequoia
{
#ifdef _WIN32
namespace detail
{
inline std::optional<std::wstring> wide_environment_variable(const wchar_t *name)
{
    wchar_t *buffer = nullptr;
    size_t size = 0;
    if (_wdupenv_s(&buffer, &size, name) != 0 || buffer == nullptr)
    {
        return std::nullopt;
    }

    const std::unique_ptr<wchar_t, decltype(&std::free)> owned_buffer(buffer, &std::free);
    return std::wstring(owned_buffer.get());
}

} // namespace detail
#endif

inline std::filesystem::path path_from_utf8(const char *path)
{
#if defined(__cpp_lib_char8_t)
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(path)));
#else
    return std::filesystem::u8path(path);
#endif
}

inline std::filesystem::path data_directory()
{
#ifdef _WIN32
    if (const auto override_path = detail::wide_environment_variable(L"PHONETIC_IPA_DATA_DIR"))
    {
        const std::filesystem::path path(*override_path);
        if (path.is_absolute())
        {
            return path;
        }
    }
#else
    if (const char *override_path = std::getenv("PHONETIC_IPA_DATA_DIR"))
    {
        const std::filesystem::path path = path_from_utf8(override_path);
        if (path.is_absolute())
        {
            return path;
        }
    }
#endif

#ifdef _WIN32
    // Personal data always lives in the current account's LOCALAPPDATA (2026-11-01 review H2). The
    // installer records only the read-only factory directory, which the Server copies from.
    if (const auto local_app_data = detail::wide_environment_variable(L"LOCALAPPDATA"))
    {
        const std::filesystem::path root(*local_app_data);
        if (root.is_absolute())
        {
            return root / L"phonetic-ipa-prototype";
        }
    }

    PWSTR known_path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &known_path)) && known_path)
    {
        const std::filesystem::path result = std::filesystem::path(known_path) / L"phonetic-ipa-prototype";
        CoTaskMemFree(known_path);
        return result;
    }
#elif defined(__APPLE__)
    if (const char *home = std::getenv("HOME"))
    {
        const std::filesystem::path root(home);
        if (root.is_absolute())
        {
            return root / "Library" / "Application Support" / "metasequoiaime";
        }
    }
#else
    if (const char *xdg_data_home = std::getenv("XDG_DATA_HOME"))
    {
        const std::filesystem::path root(xdg_data_home);
        if (root.is_absolute())
        {
            return root / "metasequoiaime";
        }
    }
    if (const char *home = std::getenv("HOME"))
    {
        const std::filesystem::path root(home);
        if (root.is_absolute())
        {
            return root / ".local" / "share" / "metasequoiaime";
        }
    }
#endif

    return {};
}

inline std::filesystem::path data_file_path(const std::filesystem::path &relative_path)
{
    const std::filesystem::path directory = data_directory();
    if (directory.empty() || relative_path.empty() || relative_path.is_absolute())
    {
        return {};
    }
    return directory / relative_path;
}

inline std::string path_to_utf8(const std::filesystem::path &path)
{
    const auto utf8_path = path.u8string();
#if defined(__cpp_lib_char8_t)
    return {reinterpret_cast<const char *>(utf8_path.data()), utf8_path.size()};
#else
    return utf8_path;
#endif
}
} // namespace metasequoia
