/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/AppKitExport.h>
#import <Foundation/Foundation.h>

@class NSLayoutManager, NSFont;

enum {
    NSTextStorageEditedCharacters = 0x01,
    NSTextStorageEditedAttributes = 0x02,
};

APPKIT_EXPORT NSString *const NSTextStorageWillProcessEditingNotification;
APPKIT_EXPORT NSString *const NSTextStorageDidProcessEditingNotification;

@interface NSTextStorage : NSMutableAttributedString <NSCoding> {
@public
    id _delegate;
    NSMutableArray *_layoutManagers;
    NSInteger _changeInLength;
    NSInteger _editedDelta;
    unsigned _editedMask;
    NSRange _editedRange;
    int _beginEditing;
    struct {
        unsigned int _isUnarchiving:1;
        unsigned int _wrappedBySuper:1;
        unsigned int _reserved:30;
    } _flags;
    void *_sideData;
}

- delegate;
- (NSArray *) layoutManagers;

- (int) changeInLength;
- (unsigned) editedMask;
- (NSRange) editedRange;

- (void) setDelegate: delegate;
- (void) addLayoutManager: (NSLayoutManager *) layoutManager;
- (void) removeLayoutManager: (NSLayoutManager *) layoutManager;

- (void) processEditing;

- (void) edited: (unsigned) editedMask
                 range: (NSRange) range
        changeInLength: (int) delta;

- (void) setFont: (NSFont *) font;
- (void) ensureAttributesAreFixedInRange: (NSRange) range;
- (void) ensureAttributesAreFixedInRange: (NSRange) range;

@end

@protocol NSTextStorageDelegate <NSObject>
- (void) textStorageWillProcessEditing: (NSNotification *) note;
- (void) textStorageDidProcessEditing: (NSNotification *) note;
@end
