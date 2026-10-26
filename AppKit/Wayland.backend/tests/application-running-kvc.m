#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
int main(void) {
 NSAutoreleasePool *pool=[NSAutoreleasePool new];
 NSApplication *app=[NSApplication sharedApplication];
 @try {
  [app setValue:[NSNumber numberWithBool:YES] forKey:@"_running"];
  if (![app isRunning] || ![[app valueForKey:@"_running"] boolValue]) return 2;
  [app setValue:[NSNumber numberWithBool:NO] forKey:@"_running"];
  if ([app isRunning] || [[app valueForKey:@"_running"] boolValue]) return 3;
 } @catch (NSException *exception) { fprintf(stderr,"Running key rejected: %s\n",[[exception name] UTF8String]); return 1; }
 BOOL unknownRejected=NO;
 @try { [app setValue:[NSNumber numberWithBool:YES] forKey:@"darling_nonexistent_running_key"]; }
 @catch (NSException *exception) { unknownRejected=[[exception name] isEqualToString:NSUndefinedKeyException]; }
 if (!unknownRejected) return 4;
 puts("NSApplication running KVC passed"); [pool drain];return 0;
}
