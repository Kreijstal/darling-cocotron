#import <AppKit/NSTitlebarAccessoryViewController.h>
#import <AppKit/NSScrollEdgeEffectStyle.h>

@implementation NSTitlebarAccessoryViewController

@synthesize layoutAttribute = _layoutAttribute;
@synthesize preferredScrollEdgeEffectStyle = _preferredScrollEdgeEffectStyle;
@synthesize automaticallyAdjustsSize = _automaticallyAdjustsSize;
@synthesize hidden = _hidden;
@synthesize fullScreenMinHeight = _fullScreenMinHeight;

- (instancetype) init {
    return [self initWithNibName: nil bundle: nil];
}

- (instancetype) initWithNibName: (NSString *) name bundle: (NSBundle *) bundle {
    if ((self = [super initWithNibName: name bundle: bundle])) {
        _layoutAttribute = NSLayoutAttributeBottom;
        _automaticallyAdjustsSize = YES;
        _hidden = NO;
        _fullScreenMinHeight = 0;
    }
    return self;
}

// The archived layout attribute isn't decoded: the default applies.
- (instancetype) initWithCoder: (NSCoder *) coder {
    if ((self = [super initWithCoder: coder])) {
        _layoutAttribute = NSLayoutAttributeBottom;
        _automaticallyAdjustsSize = [coder decodeBoolForKey: @"automaticallyAdjustsSize"];
        _hidden = [coder decodeBoolForKey: @"hidden"];
        _fullScreenMinHeight = [coder decodeDoubleForKey: @"fullScreenMinHeight"];
    }
    return self;
}

- (void) dealloc {
    [_preferredScrollEdgeEffectStyle release];
    [super dealloc];
}

@end
