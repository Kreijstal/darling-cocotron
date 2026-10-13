/*
 This file is part of Darling.

 Copyright (C) 2019 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#import <AppKit/NSPopoverTouchBarItem.h>

static inline NSMethodSignature *stubTouchBarSignature(SEL aSelector) {
    const char *selName = sel_getName(aSelector);
    int colons = 0;
    for (const char *p = selName; *p; p++) {
        if (*p == ':') colons++;
    }
    char types[64] = "@@:";
    if (strncmp(selName, "set", 3) == 0 && colons > 0) {
        types[0] = 'v';
    }
    for (int i = 0; i < colons && i < 55; i++) {
        strcat(types, "@");
    }
    return [NSMethodSignature signatureWithObjCTypes: types];
}

@implementation NSPopoverTouchBarItem

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return stubTouchBarSignature(aSelector);
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}

@end
