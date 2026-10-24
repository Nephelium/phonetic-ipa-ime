#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace PhoneticIpa::Updates
{
inline constexpr char kVersion[] = "0.1.4";
inline constexpr char kServer[] = "https://www.phonetictoolbox.com/releases/phonetic-ipa-ime/update.json";
inline constexpr char kGithub[] =
    "https://raw.githubusercontent.com/Nephelium/phonetic-ipa-ime/ipa-prototype/updates/latest.json";
using Version = std::array<int, 3>;
inline std::optional<Version> ParseVersion(const std::string &text)
{
    static const std::regex pattern(R"((0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4}))");
    std::smatch match;
    if (!std::regex_match(text, match, pattern))
        return {};
    return Version{std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3])};
}
inline bool TrustedUrl(const std::string &url)
{
    const std::string server = "https://www.phonetictoolbox.com/releases/phonetic-ipa-ime/";
    const std::string github = "https://github.com/Nephelium/phonetic-ipa-ime/releases/download/";
    size_t offset = url.rfind(server, 0) == 0 ? server.size() : url.rfind(github, 0) == 0 ? github.size() : 0;
    if (!offset || url.find("..") != std::string::npos)
        return false;
    // Release filenames are ASCII. No escaping, query, authority or alternate scheme is accepted.
    static const std::regex path(R"([A-Za-z0-9_./-]+)");
    return std::regex_match(url.substr(offset), path);
}
struct Feed
{
    std::string version, notes, url, github_url, sha256, json;
};
inline std::optional<Feed> Parse(const std::string &text, const std::string &platform = "windows")
{
    if (text.size() > 65536)
        return {};
    try
    {
        const auto root = nlohmann::json::parse(text);
        if (!root.at("schema").is_number_integer() || root.at("schema") != 1 ||
            root.at("product") != "phonetic-ipa-ime")
            return {};
        const auto &item = root.at("platforms").at(platform);
        Feed feed{root.at("version"), root.at("notes"), item.at("url"), item.at("githubUrl"), item.at("sha256"), text};
        static const std::regex hash("[0-9a-f]{64}");
        if (!ParseVersion(feed.version) ||
            std::count_if(feed.notes.begin(), feed.notes.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; }) >
                4000 ||
            !TrustedUrl(feed.url) || !TrustedUrl(feed.github_url) || !std::regex_match(feed.sha256, hash) ||
            !item.at("bytes").is_number_integer() || item.at("bytes").get<int64_t>() <= 0 ||
            item.at("bytes").get<int64_t>() > 2147483648LL)
            return {};
        return feed;
    }
    catch (const std::exception &)
    {
        return {};
    }
}
inline std::optional<Feed> Newest(const std::vector<Feed> &feeds)
{
    std::optional<Feed> best;
    for (const auto &feed : feeds)
        if (ParseVersion(feed.version) && (!best || *ParseVersion(feed.version) > *ParseVersion(best->version)))
            best = feed;
    return best;
}
} // namespace PhoneticIpa::Updates
