#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <unistd.h>

static void Pump(double seconds) {
    NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while ([until timeIntervalSinceNow] > 0) {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES];
        if (event) [NSApp sendEvent:event];
    }
}
static NSString *SourceID(TISInputSourceRef source) {
    return (__bridge NSString *)TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
}
static void Require(BOOL condition, NSString *message) {
    if (!condition) @throw [NSException exceptionWithName:@"HostCheck" reason:message userInfo:nil];
}
static void Key(NSTextView *view, NSString *text, unsigned short code, NSEventModifierFlags flags) {
    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:flags
        timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:view.window.windowNumber context:nil
        characters:text charactersIgnoringModifiers:text isARepeat:NO keyCode:code];
    [view keyDown:event];
    Pump(0.25);
    printf("key=%u text=%s marked=%d\n", code, view.string.UTF8String, view.hasMarkedText);
    fflush(stdout);
}
int main(int argc, const char **argv) {
    if (argc > 2) { freopen(argv[2], "w", stdout); dup2(fileno(stdout), fileno(stderr)); setvbuf(stdout, NULL, _IONBF, 0); }
    @autoreleasepool {
        NSApplication *app = NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [app finishLaunching];
        TISInputSourceRef previous = TISCopyCurrentKeyboardInputSource();
        int result = 0;
        @try {
            NSArray *sources = CFBridgingRelease(TISCreateInputSourceList(NULL, true));
            printf("source-count=%lu\n", (unsigned long)sources.count);
            TISInputSourceRef source = NULL;
            for (id item in sources) {
                TISInputSourceRef candidate = (__bridge TISInputSourceRef)item;
                if ([SourceID(candidate) containsString:@"PhoneticIPA"]) {
                    printf("registered=%s enabled=%s selectable=%s\n", SourceID(candidate).UTF8String,
                        [(__bridge id)TISGetInputSourceProperty(candidate, kTISPropertyInputSourceIsEnabled) description].UTF8String,
                        [(__bridge id)TISGetInputSourceProperty(candidate, kTISPropertyInputSourceIsSelectCapable) description].UTF8String);
                }
                if ([SourceID(candidate) isEqualToString:@"org.nephelium.inputmethod.PhoneticIPA.Hans"]) source = candidate;
            }
            Require(source != NULL, @"Registered input source not found");
            Require(TISEnableInputSource(source) == noErr, @"Enable failed");
            NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 200, 780, 360)
                styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
            window.title = @"国际音标输入法 · 宿主验证";
            NSTextView *view = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 780, 360)];
            view.font = [NSFont systemFontOfSize:30];
            window.contentView = view;
            [window makeKeyAndOrderFront:nil];
            [window makeFirstResponder:view];
            [app activateIgnoringOtherApps:YES];
            [view.inputContext activate];
            Pump(1);
            printf("host-active=%d key-window=%d first-responder=%d source-visible=%d\n", app.active, window.keyWindow, window.firstResponder == view,
                [view.inputContext.keyboardInputSources containsObject:@"org.nephelium.inputmethod.PhoneticIPA.Hans"]);
            OSStatus baseline = TISSelectInputSource(previous);
            OSStatus selection = TISSelectInputSource(source);
            printf("TISSelectInputSource=%d baseline-current-source=%d\n", (int)selection, (int)baseline);
            if (selection != noErr) view.inputContext.selectedKeyboardInputSource = SourceID(source);
            Pump(3);
            TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
            printf("active=%s\n", SourceID(current).UTF8String);
            BOOL selected = [SourceID(current) isEqualToString:@"org.nephelium.inputmethod.PhoneticIPA.Hans"];
            CFRelease(current);
            Require(selected, @"Input source was not selected");
            // The host owns only an NSTextView. Its normal keyDown path crosses InputMethodKit.
            Key(view, @"y", 16, 0); Key(view, @"a", 0, 0); Key(view, @"n", 45, 0);
            Key(view, @"w", 13, 0); Key(view, @"u", 32, 0);
            Require(view.hasMarkedText, @"No IMK marked text reached the host");
            Key(view, @"1", 18, 0);
            Require([view.string isEqualToString:@"厌恶"] && !view.hasMarkedText, @"Hanzi commit differs");
            Key(view, @"y", 16, 0); Key(view, @"a", 0, 0); Key(view, @"n", 45, 0);
            Key(view, @"w", 13, 0); Key(view, @"u", 32, 0);
            Key(view, @"\r", 36, NSEventModifierFlagControl);
            Require([view.string hasSuffix:@"wu˥˩"] && !view.hasMarkedText && ![view.string containsString:@"\n"], @"IPA commit differs or Control-Return leaked a newline");
            NSString *prefix = view.string;
            Key(view, @" ", 49, NSEventModifierFlagShift);
            Key(view, @"a", 0, 0); Key(view, @"1", 18, 0);
            Require([view.string isEqualToString:[prefix stringByAppendingString:@"a"]], @"CIN commit differs");
            Key(view, @"@", 19, NSEventModifierFlagShift);
            Key(view, @"!", 18, NSEventModifierFlagShift);
            Key(view, @"$", 21, NSEventModifierFlagShift);
            Key(view, @" ", 49, 0);
            Require([view.string hasSuffix:@"a˨˩˦"] && !view.hasMarkedText, @"Tone commit differs");
            if (argc > 1) {
                NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
                [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
                [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:@(argv[1]) atomically:YES];
            }
            printf("Mac host: marked text, Hanzi, IPA, CIN and Shift tones passed\n");
            [window orderOut:nil];
        } @catch (NSException *exception) {
            fprintf(stderr, "%s\n", exception.reason.UTF8String); result = 1;
        } @finally {
            if (previous) { TISSelectInputSource(previous); CFRelease(previous); }
        }
        return result;
    }
}
