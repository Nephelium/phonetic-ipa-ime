#include "common_utils.h"

#include <aclapi.h>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <sddl.h>
#include <shlobj.h>
#include <vector>

namespace
{
constexpr wchar_t kAppName[] = L"phonetic-ipa-prototype";

bool PathHasEmptyComponent(const std::wstring &path)
{
    if (path.empty())
    {
        return true;
    }
    size_t index = 0;
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
    {
        index = 2;
    }
    else if (path[0] == L'\\')
    {
        return true;
    }
    for (; index + 1 < path.size(); ++index)
    {
        if (path[index] == L'\\' && path[index + 1] == L'\\')
        {
            return true;
        }
    }
    return false;
}

bool IsUsableAbsolutePath(const std::wstring &path)
{
    if (PathHasEmptyComponent(path))
    {
        return false;
    }
    const bool has_drive = path.size() >= 3 &&
                           ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
                           path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
    if (has_drive)
    {
        return true;
    }
    return path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\';
}

std::wstring QueryEnvironmentW(const wchar_t *name)
{
    const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0)
    {
        return {};
    }
    std::wstring value(needed, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
    if (written == 0 || written >= needed)
    {
        return {};
    }
    value.resize(written);
    return value;
}

// The installer records the read-only factory resource directory under Program Files.
// KEY_WOW64_64KEY is mandatory: a 32-bit reader would otherwise be redirected to Wow6432Node,
// where the 64-bit setup never wrote anything.
std::wstring QueryFactoryDataDir()
{
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\PhoneticIpa\\Prototype", L"FactoryDataDir",
                     RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
        bytes < sizeof(wchar_t) || bytes > 32768 * sizeof(wchar_t))
    {
        return {};
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\PhoneticIpa\\Prototype", L"FactoryDataDir",
                     RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, buffer.data(), &bytes) != ERROR_SUCCESS)
    {
        return {};
    }
    return std::wstring(buffer.data());
}

std::wstring QueryKnownFolder(REFKNOWNFOLDERID folder_id)
{
    PWSTR known_path = nullptr;
    if (FAILED(SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT, nullptr, &known_path)) || !known_path)
    {
        return {};
    }
    std::wstring result(known_path);
    CoTaskMemFree(known_path);
    return result;
}

void ClearReadOnlyAttribute(const std::wstring &path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_READONLY) == 0)
    {
        return;
    }
    SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
}

bool GrantsFullAccess(ACCESS_MASK mask)
{
    return (mask & GENERIC_ALL) != 0 || (mask & FILE_ALL_ACCESS) == FILE_ALL_ACCESS;
}

// SetNamedSecurityInfoW 写入可继承 ACE 时会把它传播到整棵子树，目录下文件一多，每次启动都重写一遍
// 就要花上百毫秒。已经授过权（安装器或上次启动）就不再写。
bool HasUsersFullAccessAce(PACL dacl, PSID users_sid)
{
    ACL_SIZE_INFORMATION info{};
    if (!dacl || !GetAclInformation(dacl, &info, sizeof(info), AclSizeInformation))
    {
        return false;
    }
    bool effective = false;
    bool inheritable = false;
    for (DWORD index = 0; index < info.AceCount; ++index)
    {
        void *raw = nullptr;
        if (!GetAce(dacl, index, &raw))
        {
            continue;
        }
        const auto *header = static_cast<const ACE_HEADER *>(raw);
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE)
        {
            continue;
        }
        const auto *ace = static_cast<const ACCESS_ALLOWED_ACE *>(raw);
        if (!EqualSid(const_cast<DWORD *>(&ace->SidStart), users_sid) || !GrantsFullAccess(ace->Mask))
        {
            continue;
        }
        if ((header->AceFlags & INHERIT_ONLY_ACE) == 0)
        {
            effective = true;
        }
        if ((header->AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)) ==
            (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE))
        {
            inheritable = true;
        }
    }
    return effective && inheritable;
}

bool HasMediumNoWriteUpLabel(const std::wstring &path, PSID medium_sid)
{
    PACL sacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION, nullptr, nullptr, nullptr,
                              &sacl, &descriptor) != ERROR_SUCCESS)
    {
        return false;
    }
    bool found = false;
    ACL_SIZE_INFORMATION info{};
    if (sacl && GetAclInformation(sacl, &info, sizeof(info), AclSizeInformation))
    {
        for (DWORD index = 0; index < info.AceCount && !found; ++index)
        {
            void *raw = nullptr;
            if (!GetAce(sacl, index, &raw) ||
                static_cast<const ACE_HEADER *>(raw)->AceType != SYSTEM_MANDATORY_LABEL_ACE_TYPE)
            {
                continue;
            }
            const auto *ace = static_cast<const SYSTEM_MANDATORY_LABEL_ACE *>(raw);
            found = EqualSid(const_cast<DWORD *>(&ace->SidStart), medium_sid) &&
                    (ace->Mask & SYSTEM_MANDATORY_LABEL_NO_WRITE_UP) != 0;
        }
    }
    LocalFree(descriptor);
    return found;
}

void ApplyOwnerAccessAndMediumIntegrity(const std::wstring &path)
{
    HANDLE token = nullptr;
    std::vector<BYTE> token_user_buf;
    PSID owner_sid = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        DWORD needed = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
        if (needed > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            token_user_buf.resize(needed);
            if (GetTokenInformation(token, TokenUser, token_user_buf.data(), needed, &needed))
            {
                owner_sid = reinterpret_cast<const TOKEN_USER *>(token_user_buf.data())->User.Sid;
            }
        }
        CloseHandle(token);
    }
    if (owner_sid)
    {
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = GENERIC_ALL;
        access.grfAccessMode = GRANT_ACCESS;
        access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_USER;
        access.Trustee.ptstrName = static_cast<LPWSTR>(owner_sid);

        PACL old_dacl = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        PACL new_dacl = nullptr;
        if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &old_dacl,
                                  nullptr, &descriptor) == ERROR_SUCCESS)
        {
            if (!HasUsersFullAccessAce(old_dacl, owner_sid) &&
                SetEntriesInAclW(1, &access, old_dacl, &new_dacl) == ERROR_SUCCESS)
            {
                SetNamedSecurityInfoW(const_cast<wchar_t *>(path.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                      nullptr, nullptr, new_dacl, nullptr);
                LocalFree(new_dacl);
            }
            LocalFree(descriptor);
        }
    }

    PSID medium_sid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-16-8192", &medium_sid))
    {
        return;
    }
    if (HasMediumNoWriteUpLabel(path, medium_sid))
    {
        LocalFree(medium_sid);
        return;
    }
    const DWORD sacl_size = sizeof(ACL) + GetLengthSid(medium_sid) + sizeof(SYSTEM_MANDATORY_LABEL_ACE) + 32;
    std::vector<BYTE> sacl_buffer(sacl_size);
    auto *sacl = reinterpret_cast<PACL>(sacl_buffer.data());
    if (InitializeAcl(sacl, sacl_size, ACL_REVISION) &&
        AddMandatoryAce(sacl, ACL_REVISION, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, medium_sid))
    {
        SetNamedSecurityInfoW(const_cast<wchar_t *>(path.c_str()), SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION, nullptr,
                              nullptr, nullptr, sacl);
    }
    LocalFree(medium_sid);
}

void EnsureMediumIntegrityWritableDirectory(const std::wstring &path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    ClearReadOnlyAttribute(path);
    ApplyOwnerAccessAndMediumIntegrity(path);
}

void EnsureMediumIntegrityWritableExistingPath(const std::wstring &path)
{
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        return;
    }
    ClearReadOnlyAttribute(path);
    ApplyOwnerAccessAndMediumIntegrity(path);
}
} // namespace

namespace CommonUtils
{
std::wstring get_local_appdata_path_w()
{
    const std::wstring from_env = QueryEnvironmentW(L"LOCALAPPDATA");
    if (IsUsableAbsolutePath(from_env))
    {
        return from_env;
    }

    const std::wstring known = QueryKnownFolder(FOLDERID_LocalAppData);
    if (IsUsableAbsolutePath(known))
    {
        return known;
    }
    return {};
}

std::wstring get_ime_data_path_w()
{
    // Resolution order, shared with the engine (engine/core/data_path.h) and the TSF DLL:
    // explicit environment override, then the per-account default under LocalAppData. Personal
    // data never follows a machine-wide registry value, so accounts do not share keymaps.
    // Resolved on every call, not cached: tests relocate the profile between cases inside one
    // process.
    const std::wstring from_env = QueryEnvironmentW(L"PHONETIC_IPA_DATA_DIR");
    if (IsUsableAbsolutePath(from_env))
    {
        return from_env;
    }

    const std::wstring local = get_local_appdata_path_w();
    if (local.empty())
    {
        return {};
    }
    return (std::filesystem::path(local) / kAppName).wstring();
}

std::wstring get_factory_data_path_w()
{
    // Not used while PHONETIC_IPA_DATA_DIR redirects a development or test run.
    if (IsUsableAbsolutePath(QueryEnvironmentW(L"PHONETIC_IPA_DATA_DIR")))
    {
        return {};
    }
    const std::wstring factory = QueryFactoryDataDir();
    return IsUsableAbsolutePath(factory) ? factory : std::wstring();
}

std::wstring get_ime_config_dir_w()
{
    // Configuration normally lives in the data directory. The separate override exists for tests
    // that must redirect config writes without moving the dictionaries with them: the engine
    // resolves its own data directory (engine/core/data_path.h), so PHONETIC_IPA_DATA_DIR
    // would take the dictionaries along. Nothing in the product sets this.
    const std::wstring from_env = QueryEnvironmentW(L"PHONETIC_IPA_CONFIG_DIR");
    if (IsUsableAbsolutePath(from_env))
    {
        return from_env;
    }
    return get_ime_data_path_w();
}

void ensure_ime_data_writable()
{
    const std::wstring dir = get_ime_config_dir_w();
    if (!IsUsableAbsolutePath(dir))
    {
        return;
    }
    EnsureMediumIntegrityWritableDirectory(dir);
    static const wchar_t *const kFiles[] = {L"config.toml", L"config.base.toml", L"config.default.toml",
                                            L"config.toml.tmp"};
    for (const wchar_t *name : kFiles)
    {
        EnsureMediumIntegrityWritableExistingPath((std::filesystem::path(dir) / name).wstring());
    }
}
} // namespace CommonUtils
