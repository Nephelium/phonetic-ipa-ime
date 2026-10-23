#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

static NSString *const BundleID = @"org.nephelium.inputmethod.PhoneticIPA";
static NSString *const ModeID = @"org.nephelium.inputmethod.PhoneticIPA.Hans";

static NSArray *Sources(void) {
    return CFBridgingRelease(TISCreateInputSourceList((__bridge CFDictionaryRef)@{
        (__bridge NSString *)kTISPropertyBundleID: BundleID
    }, true));
}

static TISInputSourceRef Find(NSArray *sources, NSString *identifier) {
    for (id item in sources) {
        TISInputSourceRef source = (__bridge TISInputSourceRef)item;
        if ([(__bridge NSString *)TISGetInputSourceProperty(source, kTISPropertyInputSourceID) isEqualToString:identifier]) return source;
    }
    return NULL;
}

static BOOL Enabled(TISInputSourceRef source) {
    return source && [(__bridge NSNumber *)TISGetInputSourceProperty(source, kTISPropertyInputSourceIsEnabled) boolValue];
}

// Only our two entries may be appended. All existing entries, order and metadata survive.
static NSArray *EnabledListWithOurs(id value) {
    if (![value isKindOfClass:NSArray.class]) return nil;
    NSMutableArray *result=[value mutableCopy];
    for (id entry in result) if (![entry isKindOfClass:NSDictionary.class]) return nil;
    NSArray *required=@[
        @{ @"Bundle ID":BundleID, @"InputSourceKind":@"Keyboard Input Method" },
        @{ @"Bundle ID":BundleID, @"InputSourceKind":@"Input Mode", @"Input Mode":ModeID }
    ];
    for (NSDictionary *wanted in required) {
        BOOL found=NO;
        for (NSDictionary *entry in result) {
            BOOL matches=YES;
            for (NSString *key in wanted) if (![wanted[key] isEqual:entry[key]]) matches=NO;
            if (matches) { found=YES; break; }
        }
        if (!found) [result addObject:wanted];
    }
    return result;
}

static BOOL RepairEnabledList(NSString *backup) {
    CFStringRef domain=CFSTR("com.apple.HIToolbox"), key=CFSTR("AppleEnabledInputSources");
    CFPreferencesAppSynchronize(domain);
    id before=CFBridgingRelease(CFPreferencesCopyAppValue(key,domain));
    NSArray *after=EnabledListWithOurs(before);
    if (!after) { fprintf(stderr,"Unexpected enabled source list; no preferences changed\n"); return NO; }
    if ([after isEqual:before]) {
        CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),kTISNotifyEnabledKeyboardInputSourcesChanged,NULL,NULL,true);
        printf("enabled-list-preserved=%lu appended=0\n",(unsigned long)after.count);
        return YES;
    }
    NSError *error=nil;
    NSData *data=[NSPropertyListSerialization dataWithPropertyList:before format:NSPropertyListXMLFormat_v1_0 options:0 error:&error];
    if (!data || ![data writeToFile:backup options:NSDataWritingWithoutOverwriting error:&error]) {
        fprintf(stderr,"Could not create a new enabled-source backup\n"); return NO;
    }
    // Refuse to overwrite a concurrent System Settings change.
    CFPreferencesAppSynchronize(domain);
    id latest=CFBridgingRelease(CFPreferencesCopyAppValue(key,domain));
    if (![latest isEqual:before]) { fprintf(stderr,"Enabled source list changed concurrently; retry installation\n"); return NO; }
    CFPreferencesSetAppValue(key,(__bridge CFArrayRef)after,domain);
    if (!CFPreferencesAppSynchronize(domain)) return NO;
    id readback=CFBridgingRelease(CFPreferencesCopyAppValue(key,domain));
    if (![readback isEqual:after]) { fprintf(stderr,"Enabled source list readback differs\n"); return NO; }
    CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),kTISNotifyEnabledKeyboardInputSourcesChanged,NULL,NULL,true);
    printf("enabled-list-preserved=%lu appended=%lu verified=1\n",(unsigned long)[before count],(unsigned long)(after.count-[before count]));
    return YES;
}

#ifdef IPA_REGISTRATION_TESTS
int main(void) { @autoreleasepool {
    NSDictionary *foreign=@{@"Bundle ID":@"other.inputmethod", @"InputSourceKind":@"Input Mode", @"Input Mode":@"other.mode", @"extra":@123};
    NSArray *original=@[foreign];
    NSArray *merged=EnabledListWithOurs(original);
    NSCAssert(merged.count==3 && [merged[0] isEqual:foreign],@"Preserve existing entries");
    NSCAssert([EnabledListWithOurs(merged) isEqual:merged],@"Idempotent merge");
    NSMutableDictionary *parent=[merged[1] mutableCopy]; parent[@"extra"]=@YES;
    NSArray *existing=@[foreign,parent,merged[2]];
    NSCAssert([EnabledListWithOurs(existing) isEqual:existing],@"Keep existing metadata");
    NSCAssert(EnabledListWithOurs(@"invalid")==nil && EnabledListWithOurs(@[@1])==nil,@"Reject malformed list");
    NSArray *partial=EnabledListWithOurs(@[foreign,merged[2]]);
    NSCAssert(partial.count==3 && [partial[1] isEqual:merged[2]],@"Repair missing parent without duplicating mode");
    printf("Registration enabled-list preservation tests passed\n"); return 0;
} }
#else
int main(int argc, const char **argv) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
        [NSApp finishLaunching];
        BOOL checkOnly = argc == 3 && strcmp(argv[2], "--check") == 0;
        BOOL repair = argc == 4 && strcmp(argv[2], "--repair-enabled-list") == 0;
        if (argc != 2 && !checkOnly && !repair) { fprintf(stderr, "Usage: RegisterInputSource /path/to/PhoneticIPA.app [--check | --repair-enabled-list /new/backup.plist]\n"); return 2; }
        NSURL *url = [NSURL fileURLWithPath:@(argv[1]) isDirectory:YES];
        NSBundle *bundle = [NSBundle bundleWithURL:url];
        if (![bundle.bundleIdentifier isEqualToString:BundleID]) { fprintf(stderr, "Unexpected input method bundle ID\n"); return 2; }
        OSStatus status = noErr;
        if (!checkOnly) {
            status = LSRegisterURL((__bridge CFURLRef)url, true);
            printf("launchservices-register=%d\n", (int)status);
            if (status != noErr) return 1;
            status = TISRegisterInputSource((__bridge CFURLRef)url);
            printf("register=%d\n", (int)status);
            if (status != noErr) return 1;
        }
        NSArray *sources = Sources();
        for (id item in sources) {
            TISInputSourceRef source = (__bridge TISInputSourceRef)item;
            printf("source=%s\n", [(__bridge id)TISGetInputSourceProperty(source, kTISPropertyInputSourceID) description].UTF8String);
            for (id property in @[(__bridge id)kTISPropertyInputSourceType, (__bridge id)kTISPropertyInputSourceIsEnableCapable, (__bridge id)kTISPropertyBundleID]) {
                printf("  %s=%s\n", [property description].UTF8String, [(__bridge id)TISGetInputSourceProperty(source, (__bridge CFStringRef)property) description].UTF8String);
            }
        }
        TISInputSourceRef parent = Find(sources, BundleID);
        printf("parent-before=%d mode-before=%d\n", Enabled(parent), Enabled(Find(sources, ModeID)));
        if (checkOnly) {
            BOOL ready = Enabled(parent) && Enabled(Find(sources, ModeID));
            printf("ready=%d\n", ready);
            return ready ? 0 : 1;
        }
        if (!parent) { fprintf(stderr, "Parent input method was not registered\n"); return 1; }
        // TextInputSources.h: a mode's parent must be enabled before enabling the mode.
        status = TISEnableInputSource(parent);
        printf("enable-parent=%d\n", (int)status);
        if (status != noErr) return 1;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
        sources = Sources();
        TISInputSourceRef mode = Find(sources, ModeID);
        if (!mode) { fprintf(stderr, "Input mode was not registered\n"); return 1; }
        status = TISEnableInputSource(mode);
        printf("enable-mode=%d\n", (int)status);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
        if (repair && !RepairEnabledList(@(argv[3]))) return 1;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
        sources = Sources();
        BOOL ready = status == noErr && Enabled(Find(sources, BundleID)) && Enabled(Find(sources, ModeID));
        printf("parent-after=%d mode-after=%d\n", Enabled(Find(sources, BundleID)), Enabled(Find(sources, ModeID)));
        if (status == noErr) printf("registration-complete=1\n");
        // Installation enables this input source without selecting it for the current app.
        return ready ? 0 : 1;
    }
}

#endif
