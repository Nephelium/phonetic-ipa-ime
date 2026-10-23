#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <unistd.h>

static BOOL Foreground;
static NSTextView *View;
static NSWindow *Window;
static NSRunningApplication *PreviousApp;
static TISInputSourceRef PreviousSource;
static NSString *Target, *Screenshot;
static NSMutableArray<void (^)(void)> *Steps;
static NSUInteger StepIndex;
static int Result;

static NSString *SourceID(TISInputSourceRef source) {
    return source ? (__bridge NSString *)TISGetInputSourceProperty(source, kTISPropertyInputSourceID) : @"";
}
static void Require(BOOL condition, NSString *message) {
    if (!condition) @throw [NSException exceptionWithName:@"HostCheck" reason:message userInfo:nil];
}
static void Finish(void) {
    [Window orderOut:nil];
    if (Foreground) [PreviousApp activateWithOptions:0];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        OSStatus restored = PreviousSource ? TISSelectInputSource(PreviousSource) : -1;
        BOOL same = PreviousApp.processIdentifier == NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier;
        TISInputSourceRef current=TISCopyCurrentKeyboardInputSource();
        BOOL sourceSame=[SourceID(current) isEqualToString:SourceID(PreviousSource)];
        if (current) CFRelease(current);
        printf("restore-source=%d frontmost-unchanged=%d source-restored=%d\n", (int)restored, same, sourceSame);
        if (PreviousSource) CFRelease(PreviousSource);
        BOOL passed=Result==0 && restored==noErr && same && sourceSame;
        if (passed) printf("%s\n", [Target containsString:@"PhoneticIPA"]
            ? "Mac host: marked text, Hanzi, IPA, CIN and Shift tones passed"
            : "System IME control: marked text reached the host");
        exit(passed ? 0 : 1);
    });
}
static void Check(BOOL (^condition)(void), NSString *message) {
    [Steps addObject:^{ Require(condition(), message); printf("PASS %s\n", message.UTF8String); }];
}
static void Key(NSString *text, unsigned short code, NSEventModifierFlags flags) {
    [Steps addObject:^{
        Require(!Foreground || NSApp.active, @"Test host lost focus; stopping input");
        Require(!Foreground || (NSApp.keyWindow==Window && Window.firstResponder==View), @"Test window lost keyboard focus; stopping input");
        CGEventRef down=CGEventCreateKeyboardEvent(NULL,code,true);
        CGEventRef up=CGEventCreateKeyboardEvent(NULL,code,false);
        CGEventSetFlags(down,(CGEventFlags)flags); CGEventSetFlags(up,(CGEventFlags)flags);
        // Keep the Quartz event backing needed by the system text input context.
        // Deliver only to our own window, without global event injection.
        [Window sendEvent:[NSEvent eventWithCGEvent:down]];
        [Window sendEvent:[NSEvent eventWithCGEvent:up]];
        CFRelease(down); CFRelease(up);
        printf("queued-key=%u\n", code);
    }];
}
static void Word(void) {
    Key(@"y",16,0); Key(@"a",0,0); Key(@"n",45,0); Key(@"w",13,0); Key(@"u",32,0);
}
static void Next(void) {
    @try {
        if (StepIndex == Steps.count) { Finish(); return; }
        Steps[StepIndex++]();
        printf("host-text=%s marked=%d\n", View.string.UTF8String, View.hasMarkedText);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ Next(); });
    } @catch (NSException *exception) {
        fprintf(stderr,"FAIL %s\n", exception.reason.UTF8String); Result=1; Finish();
    }
}
int main(int argc, const char **argv) {
    if (argc > 2) { freopen(argv[2],"w",stdout); dup2(fileno(stdout),fileno(stderr)); setvbuf(stdout,NULL,_IONBF,0); }
    @autoreleasepool {
        NSApplication *app=NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        PreviousApp=NSWorkspace.sharedWorkspace.frontmostApplication;
        PreviousSource=TISCopyCurrentKeyboardInputSource();
        Foreground=argc>4 && strcmp(argv[4],"--foreground")==0;
        Screenshot=argc>1 ? @(argv[1]) : nil;
        Target=argc>3 ? @(argv[3]) : @"org.nephelium.inputmethod.PhoneticIPA.Hans";
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 200*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
            @try {
                NSArray *sources=CFBridgingRelease(TISCreateInputSourceList(NULL,true));
                TISInputSourceRef source=NULL;
                for(id item in sources) {
                    TISInputSourceRef candidate=(__bridge TISInputSourceRef)item;
                    if([SourceID(candidate) isEqualToString:Target]) source=candidate;
                }
                Require(source!=NULL,@"Registered input source found");
                Window=[[NSWindow alloc] initWithContentRect:NSMakeRect(Foreground?100:-20000,Foreground?200:-20000,780,360)
                    styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
                Window.title=@"国际音标输入法 · 宿主验证";
                View=[[NSTextView alloc] initWithFrame:NSMakeRect(0,0,780,360)];
                View.font=[NSFont systemFontOfSize:30]; Window.contentView=View;
                [Window makeFirstResponder:View];
                if(Foreground) { [Window makeKeyAndOrderFront:nil]; [app activateIgnoringOtherApps:YES]; }
                OSStatus selected=TISSelectInputSource(source);
                printf("TISSelectInputSource=%d\n",(int)selected);
                Require(selected==noErr,@"Input source selection succeeded");
                View.inputContext.selectedKeyboardInputSource=Target;
                [View.inputContext activate];
                Steps=[NSMutableArray array];
                Check(^BOOL{ TISInputSourceRef current=TISCopyCurrentKeyboardInputSource(); BOOL match=[SourceID(current) isEqualToString:Target]; CFRelease(current); return match; },@"Target input source active");
                Word();
                Check(^BOOL{ return View.hasMarkedText; },@"IMK marked text reached the host");
                if(![Target containsString:@"PhoneticIPA"]) {
                    [Steps addObject:^{ [View.inputContext discardMarkedText]; }];
                } else {
                    Key(@"1",18,0);
                    Check(^BOOL{ return [View.string isEqualToString:@"厌恶"] && !View.hasMarkedText; },@"Hanzi commit equals 厌恶");
                    Word(); Key(@"\r",36,NSEventModifierFlagControl);
                    Check(^BOOL{ return [View.string hasSuffix:@"wu˥˩"] && !View.hasMarkedText && ![View.string containsString:@"\n"]; },@"IPA commit has word-level reading and no leaked newline");
                    Key(@" ",49,NSEventModifierFlagShift); Key(@"a",0,0); Key(@"1",18,0);
                    Check(^BOOL{ return [View.string hasSuffix:@"wu˥˩a"] && !View.hasMarkedText; },@"CIN a commit");
                    Key(@"@",19,NSEventModifierFlagShift); Key(@"!",18,NSEventModifierFlagShift); Key(@"$",21,NSEventModifierFlagShift); Key(@" ",49,0);
                    Check(^BOOL{ return [View.string hasSuffix:@"a˨˩˦"] && !View.hasMarkedText; },@"Shift 214 tone commit");
                    Key(@"\\",42,0); Key(@"1",18,0);
                    Key(@"/",44,0); Key(@"1",18,0);
                    Key(@"0",29,0); Key(@"1",18,0); Key(@"6",22,0);
                    Check(^BOOL{ return [View.string hasSuffix:@"a˨˩˦˥/∅6"] && !View.hasMarkedText; },@"Backslash tones, slash, preserved zero mapping and idle digit input");
                    Key(@" ",49,NSEventModifierFlagShift);
                    Key(@"h",4,0); Key(@"a",0,0); Key(@"o",31,0); Key(@"x",7,0); Key(@"u",32,0); Key(@"e",14,0);
                    Key(@"\r",36,NSEventModifierFlagControl);
                    Check(^BOOL{ return View.hasMarkedText && [View.string hasSuffix:@"haoxue"]; },@"Ambiguous word waits for explicit reading");
                    Key(@"2",19,0);
                    Check(^BOOL{ return View.hasMarkedText && [View.string hasSuffix:@"haoxue"]; },@"Choosing a reading does not commit prematurely");
                    Key(@"\r",36,NSEventModifierFlagControl);
                    Check(^BOOL{ return [View.string hasSuffix:@"xau˥˩ ɕye˧˥"] && !View.hasMarkedText; },@"Explicit second word reading committed");
                    [Steps addObject:^{
                        if(Screenshot) { NSBitmapImageRep *rep=[View bitmapImageRepForCachingDisplayInRect:View.bounds]; [View cacheDisplayInRect:View.bounds toBitmapImageRep:rep]; [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:Screenshot atomically:YES]; }
                        printf("Input assertions completed\n");
                    }];
                }
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW,3*NSEC_PER_SEC),dispatch_get_main_queue(),^{ Next(); });
            } @catch(NSException *exception) { fprintf(stderr,"FAIL %s\n",exception.reason.UTF8String); Result=1; Finish(); }
        });
        [app run]; return 1;
    }
}
