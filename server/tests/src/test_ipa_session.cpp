#include "engine/core/input_session.h"
#include "engine/ipa/ipa_tables.h"
#include <iostream>
#include <stdexcept>

static void Require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

int main()
{
    try
    {
        metasequoia::InputSession session(SchemeType::Quanpin);
        session.set_dedicated_english_mode(true);
        for (const auto &code : {"t", "to", "ton", "tone", "tone:214", "code:a10", "[[", "1", "voqs", "a", ""})
        {
            session.set_pinyin_sequence(code);
            session.set_pinyin_sequence_with_cases(code);
            session.recompute_candidates();
            Require(session.get_pinyin_sequence_with_cases() == code, "CIN literal preedit lost or normalized");
            Require(session.get_pinyin_sequence() == code && session.get_pure_pinyin_sequence() == code &&
                        session.get_pinyin_segmentation() == code &&
                        session.get_pinyin_segmentation_with_cases() == code,
                    "Legacy session views must preserve CIN codes");
        }
        session.reset_state();
        Require(session.get_pinyin_sequence().empty() && session.dedicated_english_mode(), "Cancel must retain mode");
        session.set_dedicated_english_mode(false);
        session.set_pinyin_sequence("hanzi");
        session.set_pinyin_sequence_with_cases("hanzi");
        session.recompute_candidates();
        const auto &candidates = session.get_candidates();
        const auto found =
            std::find_if(candidates.begin(), candidates.end(), [](const auto &item) { return item.word == u8"汉字"; });
        Require(found != candidates.end(), "Real dictionary did not return Hanzi candidate");
        phonetic_ipa::HanziTable table;
        table.Load(metasequoia::RuntimePaths::legacy().resources / "ipa/beijing-toned.tsv");
        Require(table.Convert(found->word, found->canonical_pinyin).text == u8"xan˥˩ tsɹ̩˥˩",
                "Real candidate reading mismatch");
        for (const auto &spelling : {std::string("wu"), std::string("e")})
        {
            session.reset_state();
            session.set_pinyin_sequence(spelling);
            session.set_pinyin_sequence_with_cases(spelling);
            session.recompute_candidates();
            const auto &items = session.get_candidates();
            const auto evil =
                std::find_if(items.begin(), items.end(), [](const auto &item) { return item.word == u8"恶"; });
            Require(evil != items.end(), "Real dictionary is missing the polyphonic candidate");
            Require(evil->canonical_pinyin == spelling, "Candidate reading did not retain the typed syllable");
            const auto ipa = table.Convert(evil->word, evil->canonical_pinyin);
            Require(ipa.text.empty() && ipa.ambiguous, "Ambiguous IPA must wait for an explicit choice");
            for (const auto &option : ipa.parts.at(0).options)
                Require(option.ipa.rfind(spelling == "wu" ? "wu" : u8"ɤ", 0) == 0,
                        "Polyphonic IPA used another syllable");
            std::cout << "Verified typed " << spelling << " -> canonical " << evil->canonical_pinyin
                      << " -> explicit tone choices, no default\n";
        }
        std::cout << "Literal session, cancellation, mode switch and real dictionary IPA passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
