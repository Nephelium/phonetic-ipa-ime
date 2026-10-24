#import "IPAEngine.h"
#include <iostream>

static void Require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, const char **argv) {
    @autoreleasepool {
        try {
            Require(argc == 2, "Pass the resource directory");
            NSError *error = nil;
            IPAEngine *engine = [[IPAEngine alloc] initWithResourcePath:@(argv[1]) error:&error];
            Require(engine != nil, error.localizedDescription.UTF8String ?: "Engine initialization failed");
            Require([[engine mappings][@"1"] count] == 0, "Digit one retained IPA outputs");
            Require([[engine mappings][@"\\"] count] >= 15, "Migrated tones missing");
            Require([engine candidates:@"a" pinyin:NO].count > 0, "CIN input returned no candidates");
            NSArray *tones = [engine candidates:@"tone:214" pinyin:NO];
            Require(tones.count == 3 && [tones[0][@"word"] isEqual:@"˨˩˦"] && [tones[1][@"word"] isEqual:@"²¹⁴"], "Tone sequence differs");
            for (NSString *spelling in @[@"wu", @"e"]) {
                NSDictionary *reading = [engine transcription:@"恶" canonical:spelling];
                Require([reading[@"ambiguous"] boolValue] && [reading[@"text"] length] == 0, "Ambiguous character was guessed");
                NSArray *options = reading[@"parts"][0][@"options"];
                Require(options.count == 2, "Tone alternatives missing");
                for (NSDictionary *option in options) Require([option[@"pinyin"] hasPrefix:spelling], "Incompatible syllable shown");
                BOOL found = NO;
                for (NSDictionary *candidate in [engine candidates:spelling pinyin:YES])
                    if ([candidate[@"word"] isEqual:@"恶"]) found = YES;
                Require(found, "Factory pinyin candidate missing");
            }
            for (NSArray *sample in @[@[@"厌恶", @"yan'wu", @"yan4 wu4"], @[@"银行", @"yin'hang", @"yin2 hang2"], @[@"音乐", @"yin'yue", @"yin1 yue4"]]) {
                NSDictionary *reading = [engine transcription:sample[0] canonical:sample[1]];
                Require(![reading[@"ambiguous"] boolValue] && [reading[@"text"] length] > 0, "Whole-word reading unresolved");
                Require([reading[@"parts"][0][@"options"][0][@"pinyin"] isEqual:sample[2]], "Wrong whole-word tone");
            }
            NSDictionary *study = [engine transcription:@"好学" canonical:@"hao'xue"];
            Require([study[@"ambiguous"] boolValue] && [study[@"parts"][0][@"options"] count] == 2, "Polyphone alternatives collapsed");
            Require(engine.transcriptionRules.count == 10, "Ten transcription standards missing");
            Require([engine selectRule:@"unt-narrow" error:&error], "Cannot select narrow rule");
            Require([[engine transcription:@"厌恶" canonical:@"yan'wu"][@"text"] containsString:@"jɛ̠n̚"], "Rule selection did not alter IPA");
            Require(![engine selectRule:@"../beijing" error:&error], "Invalid rule was accepted");
            Require([[engine transcription:@"厌恶" canonical:@"yan'wu"][@"text"] containsString:@"jɛ̠n̚"], "Failed rule selection changed active table");
            Require([engine selectRule:@"beijing" error:&error], "Cannot restore Beijing rule");
            NSDictionary *before = [engine mappings];
            Require(![engine loadCIN:@"/nonexistent/phonetic-ipa-missing.cin" error:&error], "Invalid import accepted");
            Require([[engine mappings] isEqual:before], "Failed import changed active map");
            std::cout << "Apple core: CIN, digit migration, tone forms, weighted pinyin, word readings, ambiguity and failed import passed\n";
            return 0;
        } catch (const std::exception &exception) { std::cerr << exception.what() << '\n'; return 1; }
    }
}
