#include "ipa/ipa_reading_session.h"
#include "engine/contracts/ipa_input.h"
#include "engine/ipa/ipa_rules.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <windows.h>

static void Require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
    try
    {
        Require(argc == 2, "Pass the IPA resource directory");
        phonetic_ipa::HanziTable table;
        table.Load(std::filesystem::u8path(argv[1]) / "beijing-toned.tsv");
        for (const auto &test : std::vector<std::pair<std::string, std::string>>{{u8"厌恶", "yan'wu"},
                                                                                 {u8"银行", "yin'hang"},
                                                                                 {u8"长大", "zhang'da"},
                                                                                 {u8"音乐", "yin'yue"},
                                                                                 {u8"重庆", "chong'qing"},
                                                                                 {u8"漢字", "han'zi"}})
        {
            const auto result = table.Convert(test.first, test.second);
            Require(result.word_matched && !result.ambiguous && !result.text.empty(), "Word reading unresolved");
            std::cout << test.first << " -> " << result.parts[0].options[0].pinyin << " -> " << result.text << '\n';
        }
        {
            // 2026-11-01 review L2: BOM and blank lines are accepted; a bad tone names its line.
            const auto scratch =
                std::filesystem::temp_directory_path() / ("ipa-readings-" + std::to_string(GetTickCount64()));
            std::filesystem::create_directories(scratch);
            {
                std::ofstream out(scratch / "rows.tsv", std::ios::binary);
                out << "\xef\xbb\xbf" << u8"好\thao\txau˨˩˦\t3\n\n" << u8"好\thao\txau˥˩\t4\r\n";
            }
            phonetic_ipa::HanziTable bom;
            bom.Load(scratch / "rows.tsv", false);
            Require(bom.Convert(u8"好", "hao").parts.at(0).options.size() == 2, "BOM or blank line broke the table");
            {
                std::ofstream out(scratch / "bad.tsv", std::ios::binary);
                out << u8"好\thao\txau\t3\n" << u8"好\thao\txau\tx\n";
            }
            bool named = false;
            try
            {
                phonetic_ipa::HanziTable bad;
                bad.Load(scratch / "bad.tsv", false);
            }
            catch (const std::runtime_error &e)
            {
                named = std::string(e.what()).find("line 2") != std::string::npos;
            }
            Require(named, "Invalid tone did not report its line");
            {
                std::ofstream out(scratch / "trailing-empty.tsv", std::ios::binary);
                out << u8"好\thao\txau\t\n";
            }
            bool trailing_rejected = false;
            try
            {
                phonetic_ipa::HanziTable bad_trailing;
                bad_trailing.Load(scratch / "trailing-empty.tsv", false);
            }
            catch (const std::runtime_error &)
            {
                trailing_rejected = true;
            }
            Require(trailing_rejected, "Trailing empty tone column was accepted");
            std::error_code ignored;
            std::filesystem::remove_all(scratch, ignored);
        }
        for (const auto &word_case : std::vector<std::pair<std::string, std::string>>{
                 {u8"喜欢", "xi'huan"}, {u8"一下儿", "yi'xia'er"}, {u8"个头", "ge'tou"}})
        {
            const auto resolved = table.Convert(word_case.first, word_case.second);
            Require(resolved.word_matched && !resolved.ambiguous && !resolved.text.empty(),
                    "Neutral-tone fallback or erhua word reading failed");
        }
        Require(table.Convert(u8"一个", "yi'ge").ambiguous, "Ambiguous base-tone neutral word guessed a tone");
        const auto partial_reading =
            table.ConvertWithWordReadings(u8"个头", "ge'tou", {{"ge4", "tou2"}, {"ge5", "tou2"}});
        Require(partial_reading.word_matched && !partial_reading.ambiguous &&
                    partial_reading.parts.at(0).options.size() == 1 &&
                    partial_reading.parts.at(0).options[0].pinyin == "ge4 tou2",
                "Missing variant vetoed valid reading or recorded a partial option");
        const auto disgust = table.Convert(u8"厌恶", "yan'wu");
        phonetic_ipa::HanziTable indexedTable;
        indexedTable.Load(std::filesystem::u8path(argv[1]) / "beijing-toned.tsv", false);
        const auto indexedDisgust = indexedTable.ConvertWithWordReadings(u8"厌恶", "yan'wu", {{"yan4", "wu4"}});
        Require(indexedDisgust.word_matched && indexedDisgust.text == disgust.text,
                "Indexed word readings differ from the full table");
        const auto indexedAmbiguous =
            indexedTable.ConvertWithWordReadings(u8"好学", "hao'xue", {{"hao3", "xue2"}, {"hao4", "xue2"}});
        Require(indexedAmbiguous.ambiguous && indexedAmbiguous.text.empty() &&
                    indexedAmbiguous.parts[0].options.size() == 2,
                "Indexed alternatives silently selected a reading");
        Require(disgust.parts[0].options[0].pinyin == "yan4 wu4" && disgust.text.find(u8"wu˥˩") != std::string::npos,
                "Word must select wu4");
        for (const auto spelling : {"wu", "e"})
        {
            const auto result = table.Convert(u8"恶", spelling);
            Require(result.ambiguous && result.text.empty() && result.parts.size() == 1, "Single character guessed");
            Require(result.parts[0].options.size() == 2, "Expected two tones");
            for (const auto &option : result.parts[0].options)
                Require(option.pinyin.rfind(spelling, 0) == 0, "Different typed syllable leaked into choices");
        }
        const auto study = table.Convert(u8"好学", "hao'xue");
        Require(study.word_matched && study.ambiguous && study.text.empty() && study.parts[0].options.size() == 2,
                "Dictionary alternatives collapsed into first reading");
        Require(table.Convert(u8"厌恶", "yan'e").text.empty(), "Mismatched word pronunciation accepted");
        Require(table.Convert(u8"汉字", "han").text.empty(), "Partial spelling accepted");
        Require(table.Convert(u8"汉X", "han'x").parts.empty(), "Partial transcription published");

        PhoneticIpa::ReadingSession session;
        session.SetContext("wu");
        Require(session.Begin(table, u8"恶", "wu"), "Cannot open unresolved reading");
        Require(!session.Choose(999) && session.Active(), "Invalid choice changed state");
        const auto *part = session.Pending();
        const auto it =
            std::find_if(part->options.begin(), part->options.end(), [](const auto &r) { return r.pinyin == "wu4"; });
        Require(it != part->options.end(), "Missing wu4");
        Require(session.Choose(static_cast<size_t>(it - part->options.begin())) && !session.Active(), "Choice failed");
        Require(session.Resolve(table, u8"恶", "wu").text == u8"wu˥˩", "Chosen reading not retained");
        Require(session.Resolve(table, u8"恶", "e").text.empty(), "Choice leaked across spellings");
        Require(session.Begin(table, u8"恶", "wu"), "Cannot reopen reading choice");
        session.Cancel();
        Require(session.Resolve(table, u8"恶", "wu").text == u8"wu˥˩", "Cancel erased prior confirmed choice");
        session.SetContext("wux");
        Require(session.Resolve(table, u8"恶", "wu").text.empty(), "Editing retained stale choice");
        session.SetContext("wu");
        session.Begin(table, u8"恶", "wu");
        session.Cancel();
        Require(session.Resolve(table, u8"恶", "wu").ambiguous, "Cancel chose a default");
        session.Begin(table, u8"恶恶", "wu'wu");
        Require(session.Choose(1) && session.Active(), "First character ended choice too early");
        Require(session.Resolve(table, u8"恶恶", "wu'wu").text.empty(), "Partial choices published");
        session.Cancel();
        Require(session.Begin(table, u8"恶恶", "wu'wu"), "Cannot reopen multi-character choice");
        Require(session.Choose(0) && session.Choose(1), "Multi-character selection failed");
        Require(session.Resolve(table, u8"恶恶", "wu'wu").text == u8"wu˥ wu˥˩", "Multi-character result wrong");
        session.Clear();
        Require(session.Resolve(table, u8"恶恶", "wu'wu").text.empty(), "New composition retained choices");
        for (unsigned modifiers = 0; modifiers < 8; ++modifiers)
            Require(PhoneticIpaKey::IsReadingChoiceKey(0x0d, modifiers) == (modifiers == 3),
                    "Shortcut modifier conflict");

        const auto root = std::filesystem::u8path(argv[1]);
        const auto rules = phonetic_ipa::LoadTranscriptionRules(root);
        Require(rules.size() == 10, "Ten transcription standards must be packaged");
        std::set<std::string> distinct;
        for (const auto &rule : rules)
        {
            const auto alternate = phonetic_ipa::LoadTranscriptionRule(root, rule.id);
            const auto word = alternate.Convert(u8"厌恶", "yan'wu");
            Require(!word.text.empty() && word.word_matched, "Alternate rule lost word-level reading");
            distinct.insert(word.text);
            const auto evil = alternate.Convert(u8"恶", "wu");
            Require(evil.ambiguous && evil.parts.size() == 1 && evil.parts[0].options.size() == 2,
                    "Alternate rule guessed or leaked a different spelling");
            for (const auto &option : evil.parts[0].options)
                Require(option.pinyin == "wu1" || option.pinyin == "wu4", "wu leaked an e reading");
        }
        Require(distinct.size() >= 5, "Rule selection did not change transcription");
        PhoneticIpa::ReadingSession ruleSession;
        const auto broad = phonetic_ipa::LoadTranscriptionRule(root, "beijing");
        const auto narrow = phonetic_ipa::LoadTranscriptionRule(root, "unt-narrow");
        ruleSession.SetContext("h\tbeijing");
        Require(ruleSession.Begin(broad, u8"好", "hao") && ruleSession.Choose(0), "h reading choice failed");
        const auto chosen = ruleSession.Resolve(broad, u8"好", "hao");
        Require(!chosen.text.empty(), "Reading choice not resolved");
        ruleSession.SetContext("h\tunt-narrow");
        Require(ruleSession.Resolve(narrow, u8"好", "hao").ambiguous, "Rule switch retained stale IPA");
        bool rejected = false;
        try
        {
            (void)phonetic_ipa::LoadTranscriptionRule(root, "../beijing");
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        Require(rejected, "Unknown/path-traversal rule was accepted");

        const auto started = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i)
            Require(table.Convert(u8"厌恶", "yan'wu").text == disgust.text, "Repeated lookup changed");
        std::cout << "1000 word lookups: "
                  << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count()
                  << " ms\nTone-aware words, explicit choices, cancellation and isolation passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
