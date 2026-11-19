#include <jni.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include "engine/ipa/ipa_composer.h"
#include "engine/ipa/ipa_readings.h"
#include "engine/ipa/ipa_rules.h"

namespace {
std::string utf16ToUtf8(const char16_t* data, size_t len) {
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        uint32_t cp = data[i];
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 1 < len && data[i + 1] >= 0xDC00 && data[i + 1] <= 0xDFFF) {
                cp = 0x10000 + (((cp - 0xD800) << 10) | (data[++i] - 0xDC00));
            } else {
                cp = 0xFFFD;
            }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::u16string utf8ToUtf16(const std::string& text) {
    std::u16string out;
    out.reserve(text.size());
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text.data());
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        uint32_t cp = 0xFFFD;
        unsigned char b0 = bytes[i++];
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0 && i < n && (bytes[i] & 0xC0) == 0x80) {
            cp = ((b0 & 0x1F) << 6) | (bytes[i++] & 0x3F);
            if (cp < 0x80) cp = 0xFFFD;
        } else if ((b0 & 0xF0) == 0xE0 && i + 1 < n && (bytes[i] & 0xC0) == 0x80 && (bytes[i + 1] & 0xC0) == 0x80) {
            cp = ((b0 & 0x0F) << 12) | ((bytes[i] & 0x3F) << 6) | (bytes[i + 1] & 0x3F);
            i += 2;
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
        } else if ((b0 & 0xF8) == 0xF0 && i + 2 < n && (bytes[i] & 0xC0) == 0x80 && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80) {
            cp = ((b0 & 0x07) << 18) | ((bytes[i] & 0x3F) << 12) | ((bytes[i + 1] & 0x3F) << 6) | (bytes[i + 2] & 0x3F);
            i += 3;
            if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD;
        }
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 | (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 | (cp & 0x3FF)));
        }
    }
    return out;
}

std::string utf8(JNIEnv* env, jstring value) {
    if (!value) throw std::invalid_argument("Missing text");
    const auto* chars = env->GetStringChars(value, nullptr);
    if (!chars) throw std::runtime_error("Cannot read text");
    const jsize len = env->GetStringLength(value);
    std::string out = utf16ToUtf8(reinterpret_cast<const char16_t*>(chars), static_cast<size_t>(len > 0 ? len : 0));
    env->ReleaseStringChars(value, chars);
    return out;
}
jstring javaText(JNIEnv* env, const std::string& text) {
    const auto value = utf8ToUtf16(text);
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
struct Engine { std::mutex mutex; phonetic_ipa::CinTable cin; phonetic_ipa::HanziTable table; std::filesystem::path root; };
Engine& engine(jlong handle) {
    if (!handle) throw std::runtime_error("Engine closed");
    return *reinterpret_cast<Engine*>(handle);
}
void fail(JNIEnv* env, const std::exception& error) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    jclass cls = env->FindClass("java/lang/IllegalArgumentException");
    if (cls) env->ThrowNew(cls, error.what());
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
        result->root = root;
        result->table = phonetic_ipa::LoadTranscriptionRule(root, "beijing", false);
        return reinterpret_cast<jlong>(result.release());
    } catch (const std::exception& e) { fail(env, e); return 0; }
}
extern "C" JNIEXPORT void JNICALL Java_org_nephelium_phoneticipa_Engine_nativeClose(JNIEnv*, jclass, jlong handle) { delete reinterpret_cast<Engine*>(handle); }
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeSymbols(JNIEnv* env, jclass, jlong handle, jstring code) {
    try {
        auto& e = engine(handle);
        std::string query = utf8(env, code);
        std::lock_guard<std::mutex> lock(e.mutex);
        return javaText(env, array(phonetic_ipa::InputCandidates(e.cin, query)));
    }
    catch (const std::exception& e) { fail(env, e); return nullptr; }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeProfile(JNIEnv* env, jclass, jlong handle) {
    try {
        auto& e = engine(handle);
        std::lock_guard<std::mutex> lock(e.mutex);
        return javaText(env, profile(e.cin));
    } catch (const std::exception& e) { fail(env, e); return nullptr; }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeReadCIN(JNIEnv* env, jclass, jstring path) {
    try {
        phonetic_ipa::CinTable next; next.Load(std::filesystem::u8path(utf8(env, path)));
        return javaText(env, profile(next));
    } catch (const std::exception& e) { fail(env, e); return nullptr; }
}
extern "C" JNIEXPORT void JNICALL Java_org_nephelium_phoneticipa_Engine_nativeLoadCIN(JNIEnv* env, jclass, jlong handle, jstring path) {
    try {
        auto& e = engine(handle);
        phonetic_ipa::CinTable next; next.Load(std::filesystem::u8path(utf8(env, path)));
        std::lock_guard<std::mutex> lock(e.mutex);
        e.cin = std::move(next);
    } catch (const std::exception& e) { fail(env, e); }
}
extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeReading(JNIEnv* env, jclass, jlong handle, jstring word, jstring canonical, jobjectArray supplied) {
    try {
        std::vector<std::vector<std::string>> readings;
        if (supplied) {
            const jsize count = env->GetArrayLength(supplied);
            for (jsize i = 0; i < count; ++i) {
                auto item = static_cast<jstring>(env->GetObjectArrayElement(supplied, i));
                if (env->ExceptionCheck()) return nullptr;
                if (!item) continue;
                std::string raw;
                try { raw = utf8(env, item); }
                catch (...) { env->DeleteLocalRef(item); throw; }
                env->DeleteLocalRef(item);
                std::istringstream stream(raw);
                std::vector<std::string> row; std::string part;
                while (std::getline(stream, part, '\'')) row.push_back(part);
                readings.push_back(std::move(row));
            }
        }
        auto& e = engine(handle);
        std::string w = utf8(env, word);
        std::string c = utf8(env, canonical);
        phonetic_ipa::Transcription result;
        {
            std::lock_guard<std::mutex> lock(e.mutex);
            result = e.table.ConvertWithWordReadings(w, c, readings);
        }
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

extern "C" JNIEXPORT jstring JNICALL Java_org_nephelium_phoneticipa_Engine_nativeRules(JNIEnv* env, jclass, jlong handle) {
    try {
        auto& e = engine(handle);
        std::filesystem::path root;
        {
            std::lock_guard<std::mutex> lock(e.mutex);
            root = e.root;
        }
        std::string result = "["; bool first = true;
        for (const auto& rule : phonetic_ipa::LoadTranscriptionRules(root)) {
            if (!first) result += ','; first = false;
            result += "{\"id\":" + quote(rule.id) + ",\"name\":" + quote(rule.name) + ",\"shortName\":" + quote(rule.short_name) + '}';
        }
        return javaText(env, result + ']');
    } catch (const std::exception& e) { fail(env, e); return nullptr; }
}
extern "C" JNIEXPORT void JNICALL Java_org_nephelium_phoneticipa_Engine_nativeSetRule(JNIEnv* env, jclass, jlong handle, jstring id) {
    try {
        auto& e = engine(handle);
        std::string ruleId = utf8(env, id);
        std::filesystem::path root;
        {
            std::lock_guard<std::mutex> lock(e.mutex);
            root = e.root;
        }
        auto next = phonetic_ipa::LoadTranscriptionRule(root, ruleId, false);
        std::lock_guard<std::mutex> lock(e.mutex);
        e.table = std::move(next);
    }
    catch (const std::exception& e) { fail(env,e); }
}
