#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace phonetic_ipa
{
struct ReadingOption
{
    std::string pinyin;
    std::string ipa;
};
struct ReadingPart
{
    std::string word;
    std::vector<ReadingOption> options;
    int selected = -1;
};
struct Transcription
{
    std::string text;
    bool ambiguous = false;
    bool word_matched = false;
    std::vector<ReadingPart> parts;
    void Refresh()
    {
        text.clear();
        ambiguous = false;
        for (const auto &part : parts)
        {
            if (part.selected < 0 || static_cast<size_t>(part.selected) >= part.options.size())
            {
                ambiguous = true;
                continue;
            }
            if (!text.empty())
                text += ' ';
            text += part.options[part.selected].ipa;
        }
        if (ambiguous)
            text.clear(); // No partial reading or arbitrary first choice can be committed.
    }
    bool CanChoose() const
    {
        return std::any_of(parts.begin(), parts.end(), [](const auto &p) { return p.options.size() > 1; });
    }
};

class HanziTable
{
  public:
    void Load(const std::filesystem::path &path, bool include_word_table = true)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read Beijing IPA table");
        entries_.clear();
        words_.clear();
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto fields = Split(line, '\t');
            if (fields.size() != 3 && fields.size() != 4)
                throw std::runtime_error("Invalid Beijing IPA row");
            entries_[fields[0]].push_back(
                {Normalize(fields[1]), fields[2], fields.size() == 4 ? std::stoi(fields[3]) : -1});
        }
        // Old three-column tables remain readable. Without tones/word data,
        // ambiguous characters are still explicit choices, never first-row defaults.
        const auto words = path.parent_path() / "word-readings.tsv";
        if (include_word_table && std::filesystem::exists(words))
            LoadWords(words);
    }
    void LoadWords(const std::filesystem::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read word pronunciation table");
        words_.clear();
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto fields = Split(line, '\t');
            if (fields.size() != 2)
                throw std::runtime_error("Invalid word pronunciation row");
            words_[fields[0]].push_back(Split(fields[1], '\''));
        }
    }
    Transcription Convert(const std::string &word, const std::string &canonical = {}) const
    {
        const auto entry = words_.find(word);
        static const std::vector<std::vector<std::string>> empty;
        return ConvertWithWordReadings(word, canonical, entry == words_.end() ? empty : entry->second);
    }
    // Mobile hosts can query only this word from an indexed store instead of
    // retaining the complete phrase dictionary in a keyboard extension's memory.
    Transcription ConvertWithWordReadings(const std::string &word, const std::string &canonical,
                                          const std::vector<std::vector<std::string>> &word_readings) const
    {
        const auto chars = Characters(word);
        auto syllables = Split(canonical, '\'');
        if (chars.empty() || (!syllables.empty() && syllables.size() != chars.size()))
            return {};
        for (auto &s : syllables)
            s = Normalize(s);
        Transcription result;
        if (!word_readings.empty())
        {
            ReadingPart phrase{word};
            bool incomplete = false;
            for (const auto &reading : word_readings)
            {
                if (reading.size() != chars.size())
                    continue;
                bool agrees = true;
                for (size_t i = 0; i < chars.size(); ++i)
                    if (!syllables.empty() && Base(reading[i]) != syllables[i])
                        agrees = false;
                if (!agrees)
                    continue;
                ReadingOption option;
                for (size_t i = 0; i < chars.size(); ++i)
                {
                    const auto matches = Match(chars[i], Base(reading[i]), Tone(reading[i]));
                    if (matches.size() != 1)
                    {
                        incomplete = true;
                        break;
                    }
                    if (!option.ipa.empty())
                    {
                        option.ipa += ' ';
                        option.pinyin += ' ';
                    }
                    option.ipa += matches[0].ipa;
                    option.pinyin += reading[i];
                }
                AddUnique(phrase.options, option);
            }
            if (!incomplete && !phrase.options.empty())
            {
                phrase.selected = phrase.options.size() == 1 ? 0 : -1;
                result.parts.push_back(std::move(phrase));
                result.word_matched = true;
                result.Refresh();
                return result;
            }
        }
        for (size_t i = 0; i < chars.size(); ++i)
        {
            ReadingPart part{chars[i], Match(chars[i], syllables.empty() ? "" : syllables[i], -1)};
            if (part.options.empty())
                return {};
            part.selected = part.options.size() == 1 ? 0 : -1;
            result.parts.push_back(std::move(part));
        }
        result.Refresh();
        return result;
    }

  private:
    struct Reading
    {
        std::string pinyin, ipa;
        int tone;
    };
    std::map<std::string, std::vector<Reading>> entries_;
    std::map<std::string, std::vector<std::vector<std::string>>> words_;
    static std::vector<std::string> Split(const std::string &text, char separator)
    {
        std::vector<std::string> result;
        std::istringstream stream(text);
        std::string piece;
        while (std::getline(stream, piece, separator))
            result.push_back(piece);
        return result;
    }
    static std::vector<std::string> Characters(const std::string &word)
    {
        std::vector<std::string> result;
        for (size_t i = 0; i < word.size();)
        {
            const auto first = static_cast<unsigned char>(word[i]);
            const size_t length = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
            if (i + length > word.size())
                return {};
            result.push_back(word.substr(i, length));
            i += length;
        }
        return result;
    }
    static std::string Normalize(std::string value)
    {
        for (const auto &from : {std::string(u8"ü"), std::string("u:")})
            for (size_t pos = 0; (pos = value.find(from, pos)) != std::string::npos; ++pos)
                value.replace(pos, from.size(), "v");
        return value;
    }
    static std::string Base(std::string value)
    {
        if (!value.empty() && value.back() >= '0' && value.back() <= '5')
            value.pop_back();
        return Normalize(value);
    }
    static int Tone(const std::string &value)
    {
        return !value.empty() && value.back() >= '0' && value.back() <= '5' ? (value.back() - '0') % 5 : -1;
    }
    static void AddUnique(std::vector<ReadingOption> &options, const ReadingOption &option)
    {
        if (!option.ipa.empty() &&
            std::none_of(options.begin(), options.end(), [&](const auto &o) { return o.ipa == option.ipa; }))
            options.push_back(option);
    }
    std::vector<ReadingOption> Match(const std::string &word, const std::string &base, int tone) const
    {
        std::vector<ReadingOption> options;
        const auto found = entries_.find(word);
        if (found == entries_.end())
            return options;
        for (const auto &r : found->second)
            if ((base.empty() || r.pinyin == base) && (tone < 0 || tone == r.tone))
                AddUnique(options, {r.pinyin + (r.tone < 0 ? "" : std::to_string(r.tone == 0 ? 5 : r.tone)), r.ipa});
        return options;
    }
};
} // namespace phonetic_ipa
