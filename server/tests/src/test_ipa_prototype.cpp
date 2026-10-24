#include "engine/ipa/ipa_composer.h"
#include "engine/contracts/ipa_input.h"
#include <iostream>

static void Require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
    try
    {
        Require(argc == 2, "Pass the real IPA resource directory");
        const auto root = std::filesystem::u8path(argv[1]);
        phonetic_ipa::CinTable cin;
        cin.Load(root / "ipa.cin");
        Require(cin.Lookup("a").size() == 11 && cin.Lookup("a")[2] == u8"ɐ", "CIN a order");
        Require(cin.Lookup("r").size() == 16 && cin.Lookup("r")[1] == u8"ɾ", "CIN r order");
        Require(cin.Lookup("tone").size() == 5 && cin.Lookup("tone")[0] == u8"˥", "Tone code");
        Require(cin.Lookup("click").size() == 5 && cin.Lookup("click")[0] == u8"ʘ", "Click code");
        Require(cin.Lookup("voqs").size() == 58, "Long candidate page retained");
        Require(cin.Lookup("0")[0] == u8"∅", "Numeric code");
        Require(cin.HasPrefix("t") && cin.HasPrefix("to") && cin.HasPrefix("v") && !cin.HasPrefix("xyz"),
                "Prefix state");
        Require(PhoneticIpaKey::IsInput(true, '1', 0) && !PhoneticIpaKey::IsInput(true, '1', 1),
                "Digit selection conflict");
        Require(PhoneticIpaKey::IsInput(true, '[', 0) && PhoneticIpaKey::IsInput(true, '[', 1), "Punctuation code");
        Require(!PhoneticIpaKey::IsInput(false, 'a', 0) && !PhoneticIpaKey::IsInput(true, ' ', 1), "Mode boundary");
        Require(phonetic_ipa::ToneCandidates("55")[0] == u8"˥˥", "High-level tone must remain two levels");
        Require(phonetic_ipa::ToneCandidates("214") == std::vector<std::string>{u8"˨˩˦", u8"²¹⁴", "214"},
                "Five-level tone formats");
        Require(phonetic_ipa::ToneCandidates("12345")[0] == u8"˩˨˧˦˥", "Tone direction reversed");
        for (const auto digits : {"", "0", "6", "29", "2.14", "214x"})
            Require(phonetic_ipa::ToneCandidates(digits).empty(), "Invalid tone accepted");
        for (const auto raw : {"tone:214", "code:a10"})
        {
            std::string typed;
            for (const char ch : std::string(raw))
            {
                Require(PhoneticIpaKey::IsInput(true, ch, typed.size(), PhoneticIpaKey::IsNumericEntry(typed)),
                        "Numeric entry key was mistaken for candidate selection");
                typed += ch;
                Require(!phonetic_ipa::MayAutoCommit(cin, typed), "Prefix committed before numeric entry finished");
            }
        }
        for (char digit = '1'; digit <= '9'; ++digit)
            Require(!PhoneticIpaKey::IsInput(true, digit, 4, PhoneticIpaKey::IsNumericEntry("tone")),
                    "Ordinary one-through-nine candidate selection changed");
        Require(PhoneticIpaKey::IsInput(true, '0', 0) && !PhoneticIpaKey::IsInput(true, '0', 1),
                "Ordinary zero behavior changed");
        cin.EntriesForEdit()["a10"] = {u8"a¹⁰"};
        cin.EntriesForEdit()["tone:214"] = {u8"自定义"};
        Require(phonetic_ipa::InputCandidates(cin, "code:a10")[0] == u8"a¹⁰" &&
                    phonetic_ipa::InputCandidates(cin, "code:tone:214")[0] == u8"自定义",
                "Numeric or reserved-looking custom code unreachable");
        Require(phonetic_ipa::InputCandidates(cin, "tone:214")[0] == u8"˨˩˦" &&
                    phonetic_ipa::InputCandidates(cin, "tone") == cin.Lookup("tone") &&
                    phonetic_ipa::InputCandidates(cin, "0") == cin.Lookup("0") &&
                    phonetic_ipa::InputCandidates(cin, "1") == cin.Lookup("1"),
                "Original CIN mappings changed");
        phonetic_ipa::CinTable defaults;
        defaults.Load(root / "default.cin");
        std::string shifted;
        for (char key : std::string("@!$"))
        {
            Require(PhoneticIpaKey::IsInput(true, key, 0), "Shift digit is not a literal IPA key");
            Require(phonetic_ipa::MayAutoCommit(defaults, std::string(1, key)),
                    "Shift tone requires extra confirmation");
            shifted += defaults.Lookup(std::string(1, key))[0];
        }
        Require(shifted == u8"²¹⁴", "Held Shift+214 output mismatch");
        Require(phonetic_ipa::ToneShortcutOutput(defaults, '1') == u8"˩" &&
                    phonetic_ipa::ToneShortcutOutput(defaults, '5') == u8"˥",
                "Physical Shift tone defaults mismatch");
        auto chordOverride = defaults;
        chordOverride.HotkeysForEdit()[{1, '2'}] = u8"²";
        Require(phonetic_ipa::ToneShortcutOutput(chordOverride, '2') == u8"²", "Tone editor override ignored");

        Require(defaults.Lookup("0") == cin.Lookup("0"), "Defaults changed zero mapping");
        for (char digit = '1'; digit <= '9'; ++digit)
            Require(defaults.Lookup(std::string(1, digit)).empty(), "Candidate selection digit has IPA mappings");
        auto backslash = cin.Lookup("1");
        Require(backslash.size() == 15, "Original tone mapping count changed");
        backslash.insert(backslash.end(), cin.Lookup("\\").begin(), cin.Lookup("\\").end());
        Require(defaults.Lookup("\\") == backslash, "Backslash must preserve moved tones and original outputs");
        phonetic_ipa::HanziTable hanzi;
        hanzi.Load(root / "beijing-toned.tsv");
        Require(hanzi.Convert(u8"汉字", "han'zi").text == u8"xan˥˩ tsɹ̩˥˩", "Hanzi IPA codepoints");
        Require(hanzi.Convert(u8"中国", "zhong'guo").text == u8"tʃ̺uŋ˥ kuo˧˥", "Phrase conversion");
        Require(hanzi.Convert(u8"好", "hao").ambiguous, "Ambiguous tone marked");
        Require(hanzi.Convert(u8"汉", "hao").text.empty(), "Spelling mismatch must not invent reading");
        Require(hanzi.Convert("unmapped").text.empty(), "Unknown text has no IPA");
        std::cout << "IPA prototype resource and key policy checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
