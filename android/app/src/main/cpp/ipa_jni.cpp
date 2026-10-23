#include <jni.h>
#include <codecvt>
#include <locale>
#include <memory>
#include "engine/ipa/ipa_composer.h"
#include "engine/ipa/ipa_readings.h"

namespace {
std::string utf8(JNIEnv* env, jstring value) {
    if (!value) throw std::invalid_argument("Missing text");
    const auto* chars = env->GetStringChars(value, nullptr);
    if (!chars) throw std::runtime_error("Cannot read text");
    std::u16string text(reinterpret_cast<const char16_t*>(chars), env->GetStringLength(value));
    env->ReleaseStringChars(value, chars);
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(text);
}
jstring javaText(JNIEnv* env, const std::string& text) {
    const auto value = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().from_bytes(text);
    return env->NewString(reinterpret_cast<const jchar*>(value.data()), static_cast<jsize>(value.size()));
}
std::string quote(const std::string& text) {
    std::string out = "\"";
    const char* digits = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c < 32) { out += "\\u00"; out += digits[c >> 4]; out += digits[c & 15]; }
        else out += c;
    }
    return out + '"';
}
std::string array(const std::vector<std::string>& values) {
    std::string out = "[";
    for (size_t i = 0; i < values.size(); ++i) { if (i) out += ','; out += quote(values[i]); }
    return out + ']';
}
struct Engine { phonetic_ipa::CinTable cin; phonetic_ipa::HanziTable table; };
Engine& engine(jlong handle) {
    if (!handle) throw std::runtime_error("Engine closed");
    return *reinterpret_cast<Engine*>(handle);
}
void fail(JNIEnv* env, const std::exception& error) {
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), error.what());
}
std::string profile(const phonetic_ipa::CinTable& cin) {
    std::string out = "{\"format\":\"phonetic-ipa-keymap\",\"version\":1,\"pageSize\":6,\"layout\":" + quote(cin.layout) + ",\"sequences\":{";
    bool first = true;
    for (const auto& [key, values] : cin.AllEntries()) {
        if (!first) out += ','; first = false;
        out += quote(key) + ':' + array(values);
    }
    out += "},\"hotkeys\":["; first = true;
    for (const auto& [key, value] : cin.AllHotkeys()) {
        if (!first) out += ','; first = false;
        out += "{\"modifiers\":" + std::to_string(key.first) + ",\"virtualKey\":" + std::to_string(key.second) + ",\"output\":" + quote(value) + '}';
    }
    return out + "]}";
}
}
extern "C" JNIEXPORT jlong JNICALL Java_org_nephelium_phoneticipa_Engine_nativeOpen(JNIEnv* env, jclass, jstring directory) {
    try {
        auto result = std::make_unique<Engine>();
        const auto root = std::filesystem::u8path(utf8(env, directory));
        result->cin.Load(root / "default.cin");
        result->table.Load(root / "beijing-toned.tsv", false);
        return reinterpret_cast<jlong>(result.release());
    } catch (const std::exception& e) { fail(env, e); return 0; }
}
extern "C" JNIEXPORT void JNICALL Java_org_nephelium_phoneticipa_Engine_nativeClose(JNIEnv*, jclass, jlong handle) { delete reinterpret_cast<Engine*>(handle); }
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeSymbols(JNIEnv* env, jclass, jlong handle, jstring code) {
    try { return javaText(env, array(phonetic_ipa::InputCandidates(engine(handle).cin, utf8(env, code)))); }
    catch (const std::exception& e) { fail(env, e); return nullptr; }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeProfile(JNIEnv* env, jclass, jlong handle) {
    try { return javaText(env, profile(engine(handle).cin)); } catch (const std::exception& e) { fail(env, e); return nullptr; }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeReadCIN(JNIEnv* env, jclass, jstring path) {
    try {
        phonetic_ipa::CinTable next; next.Load(std::filesystem::u8path(utf8(env, path)));
        return javaText(env, profile(next));
    } catch (const std::exception& e) { fail(env, e); return nullptr; }
}
extern "C" JNIEXPORT void JNICALL Java_org_nephelium_phoneticipa_Engine_nativeLoadCIN(JNIEnv* env, jclass, jlong handle, jstring path) {
    try {
        phonetic_ipa::CinTable next; next.Load(std::filesystem::u8path(utf8(env, path)));
        engine(handle).cin = std::move(next);
    } catch (const std::exception& e) { fail(env, e); }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeReading(JNIEnv* env, jclass, jlong handle, jstring word, jstring canonical, jobjectArray supplied) {
    try {
        std::vector<std::vector<std::string>> readings;
        for (jsize i = 0; i < env->GetArrayLength(supplied); ++i) {
            auto item = static_cast<jstring>(env->GetObjectArrayElement(supplied, i));
            std::istringstream stream(utf8(env, item)); env->DeleteLocalRef(item);
            std::vector<std::string> row; std::string part;
            while (std::getline(stream, part, '\'')) row.push_back(part);
            readings.push_back(std::move(row));
        }
        const auto result = engine(handle).table.ConvertWithWordReadings(utf8(env, word), utf8(env, canonical), readings);
        std::string out = "{\"text\":" + quote(result.text) + ",\"ambiguous\":" + (result.ambiguous ? "true" : "false") + ",\"parts\":[";
        bool first = true;
        for (const auto& part : result.parts) {
            if (!first) out += ','; first = false;
            out += "{\"word\":" + quote(part.word) + ",\"selected\":" + std::to_string(part.selected) + ",\"options\":[";
            bool optionFirst = true;
            for (const auto& option : part.options) {
                if (!optionFirst) out += ','; optionFirst = false;
                out += "{\"pinyin\":" + quote(option.pinyin) + ",\"ipa\":" + quote(option.ipa) + '}';
            }
            out += "]}";
        }
        return javaText(env, out + "]}");
    } catch (const std::exception& e) { fail(env, e); return nullptr; }
}
