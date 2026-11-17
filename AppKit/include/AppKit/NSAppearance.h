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

#import <AppKit/AppKitExport.h>
#import <AppKit/AppKitIvarLayout.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>

typedef NSString* NSAppearanceName NS_EXTENSIBLE_STRING_ENUM;

APPKIT_EXPORT const NSAppearanceName NSAppearanceNameAqua;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameDarkAqua;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameSystem;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameTouchBar;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameLightContent;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameVibrantDark;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameVibrantLight;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameAccessibilityHighContrastAqua;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameAccessibilityHighContrastDarkAqua;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameAccessibilityHighContrastSystem;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameAccessibilityHighContrastVibrantLight;
APPKIT_EXPORT const NSAppearanceName NSAppearanceNameAccessibilityHighContrastVibrantDark;

APPKIT_EXPORT const NSAppearanceName NSAppearanceNameControlStrip; // Undocumented

// Undocumented: whether the macOS 26 system design is in use.
APPKIT_EXPORT BOOL NSSolariumEnabled(void);

@interface NSAppearance : NSObject <NSSecureCoding> {
    NSAppearanceName _name;
    NS_LEGACY_IVAR_PAD(_NSAppearanceReserved, 24)
}

// One shared appearance per known name; nil for any other name.
+ (NSAppearance *) appearanceNamed: (NSAppearanceName) name;
// The appearance -performAsCurrentDrawingAppearance: installed on this thread,
// else the process-wide one +setCurrentAppearance: sets, else the application's
// effective appearance (Aqua by default).
+ (NSAppearance *) currentDrawingAppearance NS_SWIFT_NAME(currentDrawing());
+ (NSAppearance *) currentAppearance;
+ (void) setCurrentAppearance: (NSAppearance *) appearance;
- (void) performAsCurrentDrawingAppearance: (void (NS_NOESCAPE ^)(void)) block
        NS_SWIFT_NAME(performAsCurrentDrawingAppearance(_:));
@property (readonly, copy) NSAppearanceName name;
- (NSAppearanceName) bestMatchFromAppearancesWithNames: (NSArray *) appearances;

@end

@class NSColor;

@interface NSAppearance (NSAppearanceColorAdjustment)
// Private AppKit: the color blended a fixed fraction towards black (darker)
// or white.
+ (NSColor *) colorByAdjustingLightnessOfColor: (NSColor *) color darker: (BOOL) darker;
@end

@protocol NSAppearanceCustomization <NSObject>

@required
@property (strong, nullable) NSAppearance *appearance;
@property (readonly, strong, nonnull) NSAppearance *effectiveAppearance;

@end
