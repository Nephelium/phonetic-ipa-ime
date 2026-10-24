#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
@interface IPAEngine : NSObject
- (nullable instancetype)initWithResourcePath:(NSString *)path error:(NSError **)error;
- (NSArray<NSDictionary *> *)candidates:(NSString *)code pinyin:(BOOL)pinyin;
- (NSDictionary *)transcription:(NSString *)word canonical:(NSString *)canonical;
- (NSDictionary<NSString *, NSArray<NSString *> *> *)mappings;
- (NSDictionary *)profileMetadata;
- (NSArray<NSDictionary *> *)transcriptionRules;
- (BOOL)selectRule:(NSString *)identifier error:(NSError **)error;
- (BOOL)loadCIN:(NSString *)path error:(NSError **)error;
@end
NS_ASSUME_NONNULL_END
