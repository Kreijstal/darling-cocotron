#import <Foundation/Foundation.h>

@interface _NSFileSystemDataSource : NSObject {
    NSMutableSet *_cachedPaths;
    NSFileManager *_fileManager;
    NSArray *_rootItems;
    NSURL *_rootURL;
}

// The directory listed at the top level; nil lists "/".
- (void) setRootURL: (NSURL *) url;
- (NSURL *) rootURL;

@end
