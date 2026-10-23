#include "ipa/ipa_reading_session.h"
#include "engine/contracts/ipa_input.h"
#include <chrono>
#include <iostream>

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
