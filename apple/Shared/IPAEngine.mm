#import "IPAEngine.h"
#include "engine/ipa/ipa_composer.h"
#include "engine/ipa/ipa_readings.h"
#include <sqlite3.h>

static NSString *Text(const std::string &value) {
    return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"";
}
static void SetError(NSError **error, const char *message) {
    if (error) *error = [NSError errorWithDomain:@"PhoneticIPA" code:1
                                      userInfo:@{NSLocalizedDescriptionKey: @(message)}];
}
@implementation IPAEngine {
    phonetic_ipa::CinTable _cin;
    phonetic_ipa::HanziTable _table;
    sqlite3 *_database;
}
- (instancetype)initWithResourcePath:(NSString *)path error:(NSError **)error {
    self = [super init];
    if (self) {
        try {
            const auto root = std::filesystem::u8path(path.UTF8String);
            _cin.Load(root / "default.cin");
            _table.Load(root / "beijing-toned.tsv", false);
            if (sqlite3_open_v2((root / "lookup.sqlite").c_str(), &_database, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
                throw std::runtime_error("无法打开拼音和词语读音资料");
            sqlite3_exec(_database, "PRAGMA cache_size=-1024", nullptr, nullptr, nullptr);
        } catch (const std::exception &exception) {
            SetError(error, exception.what());
            return nil;
        }
    }
    return self;
}
- (void)dealloc { if (_database) sqlite3_close(_database); }
- (NSDictionary *)transcription:(NSString *)word canonical:(NSString *)canonical {
    std::vector<std::vector<std::string>> readings;
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(_database, "SELECT reading FROM readings WHERE word=?", -1, &statement, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, word.UTF8String, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(statement) == SQLITE_ROW) {
            std::istringstream stream(reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)));
            std::vector<std::string> parts;
            std::string part;
            while (std::getline(stream, part, '\'')) parts.push_back(part);
            readings.push_back(std::move(parts));
        }
    }
    sqlite3_finalize(statement);
    const auto value = _table.ConvertWithWordReadings(word.UTF8String, canonical.UTF8String, readings);
    NSMutableArray *parts = [NSMutableArray array];
    for (const auto &part : value.parts) {
        NSMutableArray *options = [NSMutableArray array];
        for (const auto &option : part.options)
            [options addObject:@{@"pinyin": Text(option.pinyin), @"ipa": Text(option.ipa)}];
        [parts addObject:@{@"word": Text(part.word), @"selected": @(part.selected), @"options": options}];
    }
    return @{@"text": Text(value.text), @"ambiguous": @(value.ambiguous), @"parts": parts};
}
- (NSArray<NSDictionary *> *)candidates:(NSString *)code pinyin:(BOOL)pinyin {
    NSMutableArray *result = [NSMutableArray array];
    if (!pinyin || [code hasPrefix:@"tone:"] || [code hasPrefix:@"code:"]) {
        for (const auto &value : phonetic_ipa::InputCandidates(_cin, code.UTF8String))
            [result addObject:@{@"word": Text(value), @"canonical": @"", @"ipa": @"", @"symbol": @YES}];
        return result;
    }
    // The prototype uses the fixed upstream word weights and complete spellings.
    // Sentence composition/autocorrection remain the upstream engine's separate job.
    sqlite3_stmt *statement = nullptr;
    const char *sql = "SELECT word,canonical FROM candidates WHERE code=? ORDER BY weight DESC,word,canonical LIMIT 180";
    if (sqlite3_prepare_v2(_database, sql, -1, &statement, nullptr) == SQLITE_OK) {
        NSString *normalized = [[code lowercaseString] stringByReplacingOccurrencesOfString:@"'" withString:@""];
        sqlite3_bind_text(statement, 1, normalized.UTF8String, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(statement) == SQLITE_ROW) {
            NSString *word = @((const char *)sqlite3_column_text(statement, 0));
            NSString *canonical = @((const char *)sqlite3_column_text(statement, 1));
            NSDictionary *reading = [self transcription:word canonical:canonical];
            [result addObject:@{@"word": word, @"canonical": canonical, @"ipa": reading[@"text"],
                               @"reading": reading, @"symbol": @NO}];
        }
    }
    sqlite3_finalize(statement);
    return result;
}
- (NSDictionary<NSString *, NSArray<NSString *> *> *)mappings {
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    for (const auto &[key, values] : _cin.AllEntries()) {
        NSMutableArray *outputs = [NSMutableArray array];
        for (const auto &value : values) [outputs addObject:Text(value)];
        result[Text(key)] = outputs;
    }
    return result;
}
- (BOOL)loadCIN:(NSString *)path error:(NSError **)error {
    try {
        phonetic_ipa::CinTable next;
        next.Load(std::filesystem::u8path(path.UTF8String));
        auto &entries = next.EntriesForEdit();
        for (auto item = entries.begin(); item != entries.end();) {
            auto &outputs = item->second;
            outputs.erase(std::remove(outputs.begin(), outputs.end(), std::string{}), outputs.end());
            if (outputs.empty()) item = entries.erase(item); else ++item;
        }
        if (entries.empty()) throw std::runtime_error("码表没有可输入的候选");
        _cin = std::move(next);
        return YES;
    } catch (const std::exception &exception) {
        SetError(error, exception.what());
        return NO;
    }
}
- (NSDictionary *)profileMetadata {
    NSMutableArray *hotkeys = [NSMutableArray array];
    for (const auto &[key, value] : _cin.AllHotkeys())
        [hotkeys addObject:@{@"modifiers": @(key.first), @"virtualKey": @(key.second), @"output": Text(value)}];
    return @{@"layout": Text(_cin.layout), @"hotkeys": hotkeys};
}
@end
