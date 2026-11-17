#import <AppKit/NSEvent.h>
#import <Foundation/NSString.h>
#import <Foundation/NSValue.h>
#import <objc/runtime.h>
#include <AppKit/NSTextInput.h>
#include <AppKit/NSTextInput_Internal.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSKeyboardBinding.h>
#import <AppKit/NSKeyboardBindingManager.h>
#import <AppKit/NSTextInputClient.h>

// The NSTextInputClient methods a context sends.
@interface NSObject (NSTextInputContextClient)
- (void) insertText: (id) string replacementRange: (NSRange) range;
- (void) doCommandBySelector: (SEL) selector;
- (void) unmarkText;
@end

NSString *const NSTextInputContextKeyboardSelectionDidChangeNotification = @"NSTextInputContextKeyboardSelectionDidChangeNotification";

NSString *const NSTextInputReplacementRangeAttributeName = @"NSTextInputReplacementRangeAttributeName";

@implementation NSTextInputContext

+ (NSTextInputContext *) currentInputContext {
    NSResponder *responder = [[NSApp keyWindow] firstResponder];

    if ([responder respondsToSelector: @selector(inputContext)])
        return [(id) responder inputContext];
    return nil;
}

// The client is not retained: it owns its context.
- (instancetype) initWithClient: (id<NSTextInputClient>) client {
    if ((self = [super init]) != nil)
        _client = client;
    return self;
}

- (id<NSTextInputClient>) client {
    return _client;
}

- (void) activate {
}

- (void) deactivate {
}

- (void) invalidateCharacterCoordinates {
}

// Without an input method, the only marked text is the client's own.
- (void) discardMarkedText {
    if ([_client respondsToSelector: @selector(unmarkText)])
        [_client unmarkText];
}

static BOOL isInsertableText(NSString *text) {
    NSUInteger length = [text length];

    if (length == 0)
        return NO;
    for (NSUInteger i = 0; i < length; i++) {
        unichar c = [text characterAtIndex: i];

        // Control characters and the function key range are not text.
        if (c < 0x20 || c == 0x7f || (c >= 0xf700 && c <= 0xf8ff))
            return NO;
    }
    return YES;
}

// FLTK's FLView keeps its own key handling behind a need_handle flag: events
// that are not plain text (shortcuts, editing and navigation keys) are left to
// it, which the flag asks for. Returns YES when the client is such a view.
static BOOL leaveKeyToFLTKClient(id client, NSEvent *event) {
    Ivar ivar = class_getInstanceVariable([client class], "need_handle");
    if (ivar == NULL)
        return NO;

    BOOL needHandle = ([event modifierFlags] & (NSCommandKeyMask | NSControlKeyMask)) != 0;
    if (!needHandle) {
        NSString *characters = [event characters];
        if ([characters length] == 0)
            characters = [event charactersIgnoringModifiers];
        if ([characters length] > 0) {
            unichar c = [characters characterAtIndex: 0];
            needHandle = (c == 0x08 || c == 0x7F || c == '\r' || c == '\n' || c == '\t' || c == 0x1B ||
                          (c >= 0xF700 && c <= 0xF8FF));
        }
    }
    if (needHandle)
        *(char *) ((char *) client + ivar_getOffset(ivar)) = 1;
    return needHandle;
}

- (BOOL) handleEvent: (NSEvent *) event {
    if (event == nil || _client == nil)
        return NO;
    if ([event type] == NSKeyDown && leaveKeyToFLTKClient(_client, event))
        return NO;

    // Command key equivalents are not text input.
    if ([event type] != NSKeyDown ||
        ([event modifierFlags] & NSCommandKeyMask))
        return NO;

    NSKeyboardBinding *binding = [[NSKeyboardBindingManager
            defaultKeyBindingManager]
            keyBindingWithString: [event charactersIgnoringModifiers]
                   modifierFlags: [event modifierFlags]];
    NSArray *selectorNames = [binding selectorNames];

    if (selectorNames != nil) {
        if (![_client respondsToSelector: @selector(doCommandBySelector:)])
            return NO;
        for (NSString *name in selectorNames)
            [_client doCommandBySelector: NSSelectorFromString(name)];
        return YES;
    }

    NSString *text = [event characters];
    if (([event modifierFlags] & NSControlKeyMask) || !isInsertableText(text))
        return NO;
    if (![_client respondsToSelector: @selector(insertText:replacementRange:)])
        return NO;
    [_client insertText: text replacementRange: NSMakeRange(NSNotFound, 0)];
    return YES;
}

+ (NSString *) localizedNameForInputSource: (NSTextInputSourceIdentifier) inputSourceIdentifier {
    return @"U.S.";
}

- (NSTextInputSourceIdentifier) selectedKeyboardInputSource {
    return @"com.apple.keylayout.US";
}

- (void) setSelectedKeyboardInputSource: (NSTextInputSourceIdentifier) inputSourceIdentifier {
}

- (NSArray *) keyboardInputSources {
    return [NSArray arrayWithObject: @"com.apple.keylayout.US"];
}

@end
