#import <Foundation/NSObject.h>

@class NSMutableDictionary;

@interface CATransactionGroup : NSObject {
    NSMutableDictionary *_values;
    // NO for the implicit group opened by reading a transaction property, YES for
    // the one +begin pushes. Only the latter makes property changes animated.
    BOOL _explicitlyBegan;
    // Created by CATransaction itself rather than by +begin; committed from
    // the run loop.
    BOOL _implicit;
    // An implicit group whose commit was asked for (by the run loop or by
    // +flush) while explicit transactions were still nested in it.
    BOOL _commitWhenUnnested;
}

- initImplicit: (BOOL) implicit;

- (BOOL) isImplicit;
- (BOOL) commitWhenUnnested;
- (void) setCommitWhenUnnested: (BOOL) value;

- valueForKey: (NSString *) key;
- (void) setValue: value forKey: (NSString *) key;

- (BOOL) isExplicitlyBegan;
- (void) setExplicitlyBegan: (BOOL) value;

@end
