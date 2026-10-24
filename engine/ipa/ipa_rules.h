#pragma once
#include "ipa_readings.h"
#include <set>

namespace phonetic_ipa
{
struct TranscriptionRule
{
    std::string id, name, short_name, file;
};

inline std::vector<TranscriptionRule> LoadTranscriptionRules(const std::filesystem::path &root)
{
    const auto catalog = root / "hanzi-rules.tsv";
    if (!std::filesystem::exists(catalog))
        return {{"beijing", "Standard Chinese (Beijing)", "北京宽式", "beijing-toned.tsv"}};
    if (std::filesystem::file_size(catalog) > 65536)
        throw std::runtime_error("Transcription rule catalog exceeds 64 KiB");
    std::ifstream input(catalog, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read transcription rule catalog");
    std::vector<TranscriptionRule> rules;
    std::set<std::string> ids;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream fields(line);
        TranscriptionRule rule;
        if (!std::getline(fields, rule.id, '\t') || !std::getline(fields, rule.name, '\t') ||
            !std::getline(fields, rule.short_name, '\t') || !std::getline(fields, rule.file, '\t') || rule.id.empty() ||
            rule.id.size() > 64 || rule.name.empty() || rule.name.size() > 256 || rule.short_name.empty() ||
            rule.short_name.size() > 96 || rules.size() >= 64 ||
            !std::all_of(
                rule.id.begin(), rule.id.end(),
                [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; }) ||
            rule.file != "hanzi-rule-" + rule.id + ".tsv" || !ids.insert(rule.id).second)
            throw std::runtime_error("Invalid transcription rule catalog");
        rules.push_back(std::move(rule));
    }
    if (rules.empty())
        throw std::runtime_error("Empty transcription rule catalog");
    return rules;
}

inline HanziTable LoadTranscriptionRule(const std::filesystem::path &root, const std::string &id,
                                        bool include_word_table = true)
{
    const auto rules = LoadTranscriptionRules(root);
    const auto found = std::find_if(rules.begin(), rules.end(), [&](const auto &rule) { return rule.id == id; });
    if (found == rules.end())
        throw std::runtime_error("Unknown transcription rule: " + id);
    HanziTable table;
    table.Load(root / found->file, include_word_table);
    return table;
}
} // namespace phonetic_ipa
