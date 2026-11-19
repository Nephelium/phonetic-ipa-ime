#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
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
    using Words = std::map<std::string, std::vector<std::vector<std::string>>>;
    void Load(const std::filesystem::path &path, bool include_word_table = true)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read Beijing IPA table");
        entries_.clear();
        words_.reset();
        std::string line;
        for (size_t number = 1; std::getline(in, line); ++number)
        {
            if (!CleanLine(line, number))
                continue;
            const auto fields = Split(line, '\t');
            if (fields.size() != 3 && fields.size() != 4)
                throw std::runtime_error("Invalid Beijing IPA row at line " + std::to_string(number));
            entries_[fields[0]].push_back(
                {Normalize(fields[1]), fields[2], fields.size() == 4 ? ParseTone(fields[3], number) : -1});
        }
        // Old three-column tables remain readable. Without tones/word data,
        // ambiguous characters are still explicit choices, never first-row defaults.
        const auto words = path.parent_path() / "word-readings.tsv";
        if (include_word_table && std::filesystem::exists(words))
            LoadWords(words);
    }
    void LoadWords(const std::filesystem::path &path)
    {
        words_ = ReadWords(path);
    }
    // The word table is identical for every transcription rule. Hosts that switch
    // rules can parse it once and share it between tables instead of reparsing.
    static std::shared_ptr<const Words> ReadWords(const std::filesystem::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read word pronunciation table");
        auto words = std::make_shared<Words>();
        std::string line;
        for (size_t number = 1; std::getline(in, line); ++number)
        {
            if (!CleanLine(line, number))
                continue;
            const auto fields = Split(line, '\t');
            if (fields.size() != 2)
                throw std::runtime_error("Invalid word pronunciation row at line " + std::to_string(number));
            (*words)[fields[0]].push_back(Split(fields[1], '\''));
        }
        return words;
    }
    void SetWords(std::shared_ptr<const Words> words)
    {
        words_ = std::move(words);
    }
    const std::shared_ptr<const Words> &SharedWords() const
    {
        return words_;
    }
    Transcription Convert(const std::string &word, const std::string &canonical = {}) const
    {
        static const std::vector<std::vector<std::string>> empty;
        if (!words_)
            return ConvertWithWordReadings(word, canonical, empty);
        const auto entry = words_->find(word);
        return ConvertWithWordReadings(word, canonical, entry == words_->end() ? empty : entry->second);
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
            for (const auto &reading : word_readings)
            {
                if (reading.size() != chars.size())
                    continue;
                bool agrees = true;
                for (size_t i = 0; i < chars.size(); ++i)
                    if (!syllables.empty() && ReadingBase(reading[i], chars[i]) != syllables[i])
                        agrees = false;
                if (!agrees)
                    continue;
                ReadingOption option;
                bool ok = true;
                for (size_t i = 0; i < chars.size(); ++i)
                {
                    const auto base = ReadingBase(reading[i], chars[i]);
                    const int tone = Tone(reading[i]);
                    auto matches = Match(chars[i], base, tone);
                    // When the table lacks a neutral-tone row, fall back only when the
                    // character has a single base reading for that syllable (never guess).
                    if (matches.empty() && tone == 0)
                    {
                        auto fallback = Match(chars[i], base, -1);
                        if (fallback.size() == 1)
                            matches = std::move(fallback);
                    }
                    if (matches.size() != 1)
                    {
                        ok = false;
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
                if (ok)
                    AddUnique(phrase.options, option);
            }
            if (!phrase.options.empty())
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
    std::shared_ptr<const Words> words_;
    // Strips CR and a leading UTF-8 BOM; returns false for blank lines.
    static bool CleanLine(std::string &line, size_t number)
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (number == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0)
            line.erase(0, 3);
        return !line.empty();
    }
    static int ParseTone(const std::string &field, size_t number)
    {
        if (field.size() != 1 || field[0] < '0' || field[0] > '5')
            throw std::runtime_error("Invalid tone \"" + field + "\" at line " + std::to_string(number));
        return (field[0] - '0') % 5;
    }
    static std::vector<std::string> Split(const std::string &text, char separator)
    {
        std::vector<std::string> result;
        if (text.empty())
            return result;
        size_t start = 0;
        for (;;)
        {
            const size_t pos = text.find(separator, start);
            if (pos == std::string::npos)
            {
                result.push_back(text.substr(start));
                break;
            }
            result.push_back(text.substr(start, pos - start));
            start = pos + 1;
        }
        return result;
    }
    static std::vector<std::string> Characters(const std::string &word)
    {
        std::vector<std::string> result;
        for (size_t i = 0; i < word.size();)
        {
            const auto b0 = static_cast<unsigned char>(word[i]);
            size_t length = 0;
            uint32_t cp = 0;
            if (b0 < 0x80)
            {
                length = 1;
                cp = b0;
            }
            else if ((b0 & 0xe0) == 0xc0)
            {
                length = 2;
                cp = b0 & 0x1f;
            }
            else if ((b0 & 0xf0) == 0xe0)
            {
                length = 3;
                cp = b0 & 0x0f;
            }
            else if ((b0 & 0xf8) == 0xf0)
            {
                length = 4;
                cp = b0 & 0x07;
            }
            else
                return {};
            if (i + length > word.size())
                return {};
            for (size_t j = 1; j < length; ++j)
            {
                const auto cont = static_cast<unsigned char>(word[i + j]);
                if ((cont & 0xc0) != 0x80)
                    return {};
                cp = (cp << 6) | (cont & 0x3f);
            }
            if ((length == 2 && cp < 0x80) || (length == 3 && cp < 0x800) ||
                (length == 4 && (cp < 0x10000 || cp > 0x10ffff)) || (cp >= 0xd800 && cp <= 0xdfff))
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
    static std::string ReadingBase(const std::string &syllable, const std::string &ch)
    {
        std::string base = Base(syllable);
        if (base == "r" && (ch == u8"儿" || ch == u8"兒"))
            return "er";
        return base;
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
