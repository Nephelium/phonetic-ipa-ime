#pragma once
#include "engine/ipa/ipa_tables.h"
#include "engine/contracts/ipa_input.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <atomic>

namespace PhoneticIpa
{
inline void ValidateOutput(const std::string &value)
{
    if (value.empty() || value.size() > 1024 || value.find_first_of("\r\n\t") != std::string::npos ||
        value.find('\0') != std::string::npos ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0))
        throw std::runtime_error("输出须为有效 Unicode，单项不超过 1024 字节，不含换行或制表符");
}
inline void ValidateProfile(const phonetic_ipa::CinTable &table)
{
    if (table.AllEntries().empty() || table.AllEntries().size() > 10000 || table.AllHotkeys().size() > 1000)
        throw std::runtime_error("方案须包含 1–10000 组编码，组合键不超过 1000 项");
    if (table.layout != "104" && table.layout != "87" && table.layout != "75" && table.layout != "65" &&
        table.layout != "60")
        throw std::runtime_error("不支持的键盘布局");
    for (const auto &[code, values] : table.AllEntries())
    {
        if (code.empty() || code.size() > 128 || code.find_first_of(" \t\r\n") != std::string::npos ||
            !std::all_of(code.begin(), code.end(), [](unsigned char c) { return c > 32 && c != 127; }) ||
            !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, code.data(), static_cast<int>(code.size()), nullptr,
                                 0) ||
            values.empty() || values.size() > 200)
            throw std::runtime_error("编码须为有效 Unicode 且不含空格，每组 1–200 个候选");
        for (const auto &value : values)
            ValidateOutput(value);
    }
    for (const auto &[key, value] : table.AllHotkeys())
    {
        if (!PhoneticIpaKey::EditableChord(key.first, key.second) &&
            !PhoneticIpaKey::IsToneShortcut(key.second, key.first))
            throw std::runtime_error("此组合键属于系统或输入法控制键，请选择其他组合");
        ValidateOutput(value);
    }
}
inline std::string ProfileJson(const phonetic_ipa::CinTable &table)
{
    ValidateProfile(table);
    nlohmann::json result = {{"format", "phonetic-ipa-keymap"},
                             {"version", 1},
                             {"layout", table.layout},
                             {"sequences", table.AllEntries()},
                             {"hotkeys", nlohmann::json::array()}};
    for (const auto &[key, value] : table.AllHotkeys())
        result["hotkeys"].push_back({{"modifiers", key.first}, {"virtualKey", key.second}, {"output", value}});
    return result.dump(2);
}
inline phonetic_ipa::CinTable ImportProfile(const std::filesystem::path &file)
{
    if (std::filesystem::file_size(file) > 4 * 1024 * 1024)
        throw std::runtime_error("文件超过 4 MB，请缩小码表");
    phonetic_ipa::CinTable table;
    if (file.extension() == L".json" || file.extension() == L".JSON")
    {
        std::ifstream input(file, std::ios::binary);
        const auto json = nlohmann::json::parse(input);
        if (json.at("format") != "phonetic-ipa-keymap" || json.at("version") != 1)
            throw std::runtime_error("不支持的方案格式或版本");
        table.layout = json.at("layout").get<std::string>();
        table.EntriesForEdit() = json.at("sequences").get<phonetic_ipa::CinTable::Entries>();
        for (const auto &item : json.at("hotkeys"))
        {
            const auto key =
                std::make_pair(item.at("modifiers").get<unsigned>(), item.at("virtualKey").get<unsigned>());
            if (!table.HotkeysForEdit().emplace(key, item.at("output").get<std::string>()).second)
                throw std::runtime_error("方案中包含重复组合键");
        }
    }
    else
        table.Load(file);
    ValidateProfile(table);
    return table;
}
inline void WriteProfileFile(const std::filesystem::path &path, const std::string &text)
{
    static std::atomic<unsigned> sequence{0};
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L"-" +
                        std::to_wstring(++sequence);
    const auto temporary = std::filesystem::path(path.wstring() + L".pending-" + suffix);
    HANDLE handle =
        CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("无法创建临时文件，请检查保存位置");
    DWORD written = 0;
    const bool ok = WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
                    written == text.size() && FlushFileBuffers(handle);
    CloseHandle(handle);
    if (!ok)
        throw std::runtime_error("写入失败，原方案保留");
    if (std::filesystem::exists(path))
    {
        const auto backup = std::filesystem::path(path.wstring() + L".backup-" + suffix);
        if (!ReplaceFileW(path.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr))
            throw std::runtime_error("保存失败，原方案与临时文件均保留");
    }
    else if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("保存失败，临时文件保留");
}
} // namespace PhoneticIpa
