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
inline bool ReadCinLine(std::istream &in, std::string &line)
{
    line.clear();
    char ch;
    while (in.get(ch))
    {
        if (ch == '\n')
            return true;
        if (ch == '\r')
        {
            if (in.peek() == '\n')
                in.get();
            return true;
        }
        line += ch;
    }
    return !line.empty();
}
// Keep CIN outputs byte-for-byte, including combining marks, duplicate rows and spaces.
class CinTable
{
  public:
    using Entries = std::map<std::string, std::vector<std::string>>;
    using Hotkeys = std::map<std::pair<unsigned, unsigned>, std::string>;
    Entries &EntriesForEdit()
    {
        return entries_;
    }
    const Entries &AllEntries() const
    {
        return entries_;
    }
    Hotkeys &HotkeysForEdit()
    {
        return hotkeys_;
    }
    const Hotkeys &AllHotkeys() const
    {
        return hotkeys_;
    }
    std::string layout = "104";
    void Load(const std::filesystem::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read IPA CIN table");
        entries_.clear();
        hotkeys_.clear();
        layout = "104";
        bool body = false;
        std::string line;
        while (ReadCinLine(in, line))
        {
            if (line.compare(0, 3, "\xef\xbb\xbf") == 0)
                line.erase(0, 3);
            if (line.compare(0, 12, "%ipa_layout ") == 0)
            {
                layout = line.substr(12);
                continue;
            }
            if (line.compare(0, 12, "%ipa_hotkey ") == 0)
            {
                std::istringstream row(line.substr(12));
                unsigned modifiers = 0, key = 0;
                if (!(row >> modifiers >> key) || row.get() != '\t')
                    throw std::runtime_error("Invalid IPA hotkey");
                std::string output;
                std::getline(row, output);
                if (!hotkeys_.emplace(std::make_pair(modifiers, key), output).second)
                    throw std::runtime_error("Duplicate IPA hotkey");
                continue;
            }
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line == "%chardef begin")
            {
                body = true;
                continue;
            }
            if (line == "%chardef end")
            {
                body = false;
                break;
            }
            if (!body || line.empty())
                continue;
            const auto split = line.find_first_of("\t ");
            // A tab-delimited # key is a mapping, not a CIN comment. The editor
            // always uses tabs so all printable keyboard symbols round-trip.
            if (line[0] == '#' && (split == std::string::npos || line[split] != '\t'))
                continue;
            if (split == std::string::npos)
                throw std::runtime_error("Invalid CIN row");
            const auto value = line[split] == '\t' ? split + 1 : line.find_first_not_of("\t ", split);
            if (value != std::string::npos)
                entries_[line.substr(0, split)].push_back(line.substr(value));
        }
        if (body || entries_.empty())
            throw std::runtime_error("Incomplete CIN table");
    }
    const std::vector<std::string> &Lookup(const std::string &code) const
    {
        const auto it = entries_.find(code);
        static const std::vector<std::string> empty;
        return it == entries_.end() ? empty : it->second;
    }
    bool HasPrefix(const std::string &code) const
    {
        const auto it = entries_.lower_bound(code);
        return it != entries_.end() && it->first.compare(0, code.size(), code) == 0;
    }
    bool HasLongerPrefix(const std::string &code) const
    {
        const auto it = entries_.upper_bound(code);
        return it != entries_.end() && it->first.compare(0, code.size(), code) == 0;
    }
    std::string Hotkey(unsigned modifiers, unsigned key) const
    {
        const auto found = hotkeys_.find({modifiers & 7u, key});
        return found == hotkeys_.end() ? std::string() : found->second;
    }
    std::string Serialize(bool extensions = true) const
    {
        std::ostringstream out;
        out << "%gen_inp\n%ename PhoneticIPA\n%cname 国际音标\n%selkey 1234567890\n";
        if (extensions)
        {
            out << "%ipa_layout " << layout << '\n';
            for (const auto &[key, value] : hotkeys_)
                out << "%ipa_hotkey " << key.first << ' ' << key.second << '\t' << value << '\n';
        }
        out << "%chardef begin\n";
        for (const auto &[code, values] : entries_)
            for (const auto &value : values)
                out << code << '\t' << value << '\n';
        out << "%chardef end\n";
        return out.str();
    }

  private:
    Entries entries_;
    Hotkeys hotkeys_;
};

// Each caller owns its cache; complete replacement keeps a malformed update from
// invalidating an already usable map. The editor validates before atomic publication.
class LiveCinTable
{
  public:
    const CinTable &Get(const std::filesystem::path &directory)
    {
        std::error_code error;
        auto path = directory / "active.cin";
        if (!std::filesystem::is_regular_file(path, error))
        {
            path = directory / "default.cin";
            if (!std::filesystem::is_regular_file(path, error))
                path = directory / "ipa.cin";
        }
        const auto stamp = std::filesystem::last_write_time(path, error);
        if (error && path_.empty())
            throw std::runtime_error("Cannot locate IPA CIN table");
        if (!error && (path != path_ || stamp != stamp_))
        {
            try
            {
                if (std::filesystem::file_size(path) > 4 * 1024 * 1024)
                    throw std::runtime_error("IPA CIN file exceeds 4 MB");
                CinTable next;
                next.Load(path);
                table_ = std::move(next);
                path_ = path;
                stamp_ = stamp;
            }
            catch (...)
            {
                if (path_.empty())
                    throw;
            }
        }
        return table_;
    }

  private:
    CinTable table_;
    std::filesystem::path path_;
    std::filesystem::file_time_type stamp_{};
};

} // namespace phonetic_ipa

#include "ipa_readings.h"
