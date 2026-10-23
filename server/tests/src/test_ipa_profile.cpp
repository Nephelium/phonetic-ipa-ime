#include "ipa/ipa_profile.h"
#include "ipa/ipa_keyboard_layout.h"
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
