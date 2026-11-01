#include "ipa/ipa_profile.h"
#include "ipa/ipa_keyboard_layout.h"
#include "ipa/ipa_user_data.h"
#include <fstream>
#include <iostream>

static void Require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class F> static void Reject(F action, const char *message)
{
    bool failed = false;
    try
    {
        action();
    }
    catch (...)
    {
        failed = true;
    }
    Require(failed, message);
}
int main(int argc, char **argv)
{
    try
    {
        Require(argc == 3, "Resource and scratch directories required");
        Require(PhoneticIpa::LayoutForReportedKeyCount(104) == "104" &&
                    PhoneticIpa::LayoutForReportedKeyCount(87) == "87" &&
                    PhoneticIpa::LayoutForReportedKeyCount(84) == "75" &&
                    PhoneticIpa::LayoutForReportedKeyCount(68) == "65" &&
                    PhoneticIpa::LayoutForReportedKeyCount(61) == "60" &&
                    PhoneticIpa::LayoutForReportedKeyCount(0).empty(),
                "Keyboard size matching");
        for (const auto &layout : {"104", "87", "75", "65", "60"})
        {
            const auto keys = PhoneticIpa::KeyboardKeys(layout);
            if (std::string(layout) == "104")
                Require(keys.size() == 104, "Full keyboard key count");
            if (std::string(layout) == "87")
                Require(keys.size() == 87, "TKL keyboard key count");
            for (float width : {300.0f, 450.0f, 600.0f, 820.0f, 1100.0f})
                for (size_t i = 0; i < keys.size(); ++i)
                {
                    const auto a = PhoneticIpa::KeyBounds(keys[i], layout, width, 1);
                    Require(a.width > 0 && a.height > 0 && a.x >= 0 && a.x + a.width <= width,
                            "Hover left keyboard bounds");
                    for (size_t j = i + 1; j < keys.size(); ++j)
                    {
                        const auto b = PhoneticIpa::KeyBounds(keys[j], layout, width, 1);
                        Require(a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y ||
                                    b.y + b.height <= a.y,
                                "Hover key overlaps neighbor");
                    }
                }
        }
        auto root = std::filesystem::u8path(argv[1]);
        auto scratch = std::filesystem::u8path(argv[2]) / std::to_string(GetTickCount64());
        std::filesystem::create_directories(scratch);
        auto original = PhoneticIpa::ImportProfile(root / "ipa.cin");
        original.layout = "65";
        original.EntriesForEdit()["#"] = {u8"ʕ"};
        original.EntriesForEdit()["test"] = {u8"ã", u8" ɐ ", u8"ã"};
        original.HotkeysForEdit()[{6, 'A'}] = u8"ɐ";
        original.HotkeysForEdit()[{1, '2'}] = u8"˨";
        PhoneticIpa::WriteProfileFile(scratch / "roundtrip.json", PhoneticIpa::ProfileJson(original));
        const auto json = PhoneticIpa::ImportProfile(scratch / "roundtrip.json");
        Require(json.AllEntries() == original.AllEntries() && json.AllHotkeys() == original.AllHotkeys() &&
                    json.layout == "65",
                "JSON roundtrip lost data");
        PhoneticIpa::WriteProfileFile(scratch / "active.cin", original.Serialize());
        const auto cin = PhoneticIpa::ImportProfile(scratch / "active.cin");
        Require(cin.AllEntries() == original.AllEntries() && cin.AllHotkeys() == original.AllHotkeys(),
                "CIN roundtrip lost data");
        phonetic_ipa::LiveCinTable live;
        Require(live.Get(scratch).Hotkey(6, 'A') == u8"ɐ", "Runtime did not load custom hotkey");
        auto changed = original;
        changed.HotkeysForEdit()[{6, 'A'}] = u8"ɾ";
        PhoneticIpa::WriteProfileFile(scratch / "active.cin", changed.Serialize());
        Require(live.Get(scratch).Hotkey(6, 'A') == u8"ɾ", "Runtime did not reload updated map");
        HANDLE lock = CreateFileW((scratch / "active.cin").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        Require(lock != INVALID_HANDLE_VALUE, "Could not lock test fixture");
        Reject([&] { PhoneticIpa::WriteProfileFile(scratch / "active.cin", original.Serialize()); },
               "Locked publication should fail");
        CloseHandle(lock);
        Require(PhoneticIpa::ImportProfile(scratch / "active.cin").Hotkey(6, 'A') == u8"ɾ",
                "Failed publication destroyed old map");
        auto invalid = original;
        invalid.HotkeysForEdit()[{3, 'I'}] = "x";
        Reject([&] { PhoneticIpa::ValidateProfile(invalid); }, "Reserved toggle accepted");
        invalid = original;
        invalid.EntriesForEdit()["bad"] = {std::string("\xff", 1)};
        Reject([&] { PhoneticIpa::ValidateProfile(invalid); }, "Malformed UTF8 accepted");
        PhoneticIpa::WriteProfileFile(scratch / "bad.json", "{\"version\":999}");
        Reject([&] { PhoneticIpa::ImportProfile(scratch / "bad.json"); }, "Bad import accepted");
        PhoneticIpa::WriteProfileFile(scratch / "active.cin", "broken");
        Require(live.Get(scratch).Hotkey(6, 'A') == u8"ɾ", "Malformed external edit replaced live map");
        {
            // 2026-11-01 review H3/M11/L1: a damaged active.cin that never loaded
            // falls back to default.cin instead of failing, and empty outputs are dropped.
            const auto fallback = scratch / "fallback";
            std::filesystem::create_directories(fallback);
            PhoneticIpa::WriteProfileFile(fallback / "active.cin", "broken");
            PhoneticIpa::WriteProfileFile(fallback / "default.cin", "%chardef begin\na\t\nb\tx\nc\ty\n%chardef end\n");
            phonetic_ipa::LiveCinTable fresh;
            const auto &table = fresh.Get(fallback);
            Require(table.Lookup("b").size() == 1 && fresh.ActiveFailed() &&
                        fresh.LoadedPath().filename() == "default.cin",
                    "Damaged active.cin did not fall back to default.cin");
            Require(table.Lookup("a").empty(), "Empty CIN output produced a blank candidate");
            Require(fresh.Get(fallback).Lookup("c").size() == 1 && fresh.ActiveFailed(),
                    "Fallback state not kept for an unchanged damaged file");
            std::filesystem::remove(fallback / "default.cin");
            Reject(
                [&] {
                    phonetic_ipa::LiveCinTable none;
                    (void)none.Get(fallback);
                },
                "Missing tables were reported as loaded");
            PhoneticIpa::WriteProfileFile(fallback / "active.cin", "%chardef begin\nb\tz\n%chardef end\n");
            Require(fresh.Get(fallback).Lookup("b")[0] == "z" && !fresh.ActiveFailed(),
                    "Repaired active.cin was not picked up");
            // M9: an applied profile keeps one fixed-name backup instead of one per save.
            PhoneticIpa::WriteProfileFile(fallback / "active.cin", "%chardef begin\nb\tw\n%chardef end\n",
                                          PhoneticIpa::ProfileBackup::Previous);
            PhoneticIpa::WriteProfileFile(fallback / "active.cin", "%chardef begin\nb\tv\n%chardef end\n",
                                          PhoneticIpa::ProfileBackup::Previous);
            size_t backups = 0;
            for (const auto &entry : std::filesystem::directory_iterator(fallback))
                if (entry.path().filename().wstring().find(L".backup-") != std::wstring::npos ||
                    entry.path().filename() == "active.cin.previous")
                    ++backups;
            Require(backups == 1 && std::filesystem::exists(fallback / "active.cin.previous"),
                    "Profile saves accumulated backup files");
            // H2: each account is seeded from the factory folder; personal files are never replaced.
            {
                const auto factory = fallback / "factory";
                const auto account = fallback / "account";
                const auto write = [](const std::filesystem::path &path, const std::string &text) {
                    std::filesystem::create_directories(path.parent_path());
                    std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
                };
                const auto read = [](const std::filesystem::path &path) {
                    std::ifstream input(path, std::ios::binary);
                    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
                };
                write(factory / "config.toml", "factory-config");
                write(factory / "ipa" / "default.cin", "keys-1");
                write(factory / "factory-stamp.txt", "app-1\n");
                write(account / "ipa" / "active.cin", "mine");
                PhoneticIpa::UserData::SeedFrom(factory, account);
                Require(read(account / "config.toml") == "factory-config" &&
                            read(account / "ipa" / "default.cin") == "keys-1" &&
                            read(account / "ipa" / "active.cin") == "mine",
                        "First seed did not copy factory files or replaced a personal file");
                write(account / "config.toml", "my-config");
                write(factory / "config.toml", "factory-config-2");
                write(factory / "ipa" / "default.cin", "keys-2");
                write(factory / "factory-stamp.txt", "app-2\n");
                PhoneticIpa::UserData::SeedFrom(factory, account);
                Require(read(account / "ipa" / "default.cin") == "keys-2" &&
                            read(account / "config.toml") == "my-config" &&
                            read(account / "factory-stamp.txt") == "app-2\n",
                        "Upgrade seed did not refresh factory files or replaced personal config");
                Require(PhoneticIpa::UserData::IsPersonal("msime_user.db-wal") &&
                            PhoneticIpa::UserData::IsPersonal("ipa/onboarding-v2.done") &&
                            !PhoneticIpa::UserData::IsPersonal("ipa/default.cin") &&
                            !PhoneticIpa::UserData::IsPersonal("msime.db"),
                        "Personal file classification");
                // Engine databases keep learned data across upgrades and change only with the dictionary.
                write(factory / "msime.db", "db-1");
                write(factory / "dictionary-manifest.json", "dict-1");
                write(factory / "factory-stamp.txt", "app-3\n");
                PhoneticIpa::UserData::SeedFrom(factory, account);
                write(account / "msime.db", "learned");
                write(factory / "msime.db", "db-2");
                write(factory / "factory-stamp.txt", "app-4\n");
                PhoneticIpa::UserData::SeedFrom(factory, account);
                Require(read(account / "msime.db") == "learned" && read(account / "factory-stamp.txt") == "app-4\n",
                        "Upgrade without a new dictionary replaced the engine database");
                write(factory / "dictionary-manifest.json", "dict-2");
                write(factory / "factory-stamp.txt", "app-5\n");
                PhoneticIpa::UserData::SeedFrom(factory, account);
                Require(read(account / "msime.db") == "db-2" &&
                            read(account / "msime.db.before-upgrade-app-5") == "learned" &&
                            read(account / "dictionary-manifest.json") == "dict-2",
                        "New dictionary was not installed or the old database was not kept");
            }
            // L9: host clipboard and undo shortcuts cannot become IPA chords.
            for (const unsigned key : {'A', 'C', 'S', 'V', 'X', 'Y', 'Z'})
                Require(!PhoneticIpaKey::EditableChord(2, key), "System shortcut accepted as IPA chord");
            Require(PhoneticIpaKey::EditableChord(6, 'C') && PhoneticIpaKey::EditableChord(2, 'Q'),
                    "Ordinary chord rejected");
            // M3: Shift+1..5 stays punctuation with Caps Lock (Hanzi output).
            Require(PhoneticIpaKey::ToneShortcutApplies(true, true) &&
                        PhoneticIpaKey::ToneShortcutApplies(false, false) &&
                        !PhoneticIpaKey::ToneShortcutApplies(false, true),
                    "Tone shortcut mode rule");
        }
        std::cout << "CIN/JSON Unicode roundtrip, atomic save, failure preservation, live reload and chord validation "
                     "passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
