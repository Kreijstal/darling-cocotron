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

typedef NSString *NSVoiceGenderName NS_STRING_ENUM;

APPKIT_EXPORT NSVoiceGenderName const NSVoiceGenderFemale;
APPKIT_EXPORT NSVoiceGenderName const NSVoiceGenderMale;

typedef NSString *NSVoiceAttributeKey NS_STRING_ENUM;

APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceName;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceGender;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceDemoText;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceLocaleIdentifier;

typedef NSString *NSSpeechPropertyKey NS_STRING_ENUM;

APPKIT_EXPORT NSSpeechPropertyKey const NSSpeechRateProperty;
APPKIT_EXPORT NSSpeechPropertyKey const NSSpeechPitchBaseProperty;
APPKIT_EXPORT NSSpeechPropertyKey const NSSpeechVolumeProperty;

@class NSArray, NSDictionary;

@interface NSSpeechSynthesizer : NSObject {
    // Apple's footprint is one word; it holds the pointer to the extras.
    NS_EXTRA_IVARS_BEGIN(NSSpeechSynthesizer)
    NSString *_voice;
    NS_EXTRA_IVARS_END(NSSpeechSynthesizer)
}
+ (NSArray<NSString *> *) availableVoices;
+ (NSString *) defaultVoice;
+ (NSDictionary<NSVoiceAttributeKey, id> *) attributesForVoice: (NSString *) voice;

// There is no speech synthesis engine here, so the voice list is empty and
// per-voice lookups come back empty rather than reporting voices that cannot
// speak. These return objects on purpose: the catch-all stub answers every
// unimplemented selector with a "v@:" signature, so an object-returning method
// left to it would hand back whatever was in the return register.
+ (NSString *) voiceAtIndex: (NSInteger) index;
- (NSDictionary<NSVoiceAttributeKey, id> *) attributesForVoice: (NSString *) voice;
- (instancetype) initWithVoice: (NSString *) voice;
@property (nonatomic, readonly) NSString *voice;
@end

@protocol NSSpeechSynthesizerDelegate <NSObject>

// TODO

@end
