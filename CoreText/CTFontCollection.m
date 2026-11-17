#import <CoreText/CTFontCollection.h>
#import <CoreText/CTFontDescriptor.h>
#import <Foundation/Foundation.h>
#include <stdio.h>

const CFStringRef kCTFontCollectionRemoveDuplicatesOption = CFSTR("CTFontCollectionRemoveDuplicatesOption");
const CFStringRef kCTFontCollectionIncludeDisabledFontsOption = CFSTR("CTFontCollectionIncludeDisabledFontsOption");
const CFStringRef kCTFontCollectionDisallowAutoActivationOption = CFSTR("CTFontCollectionDisallowAutoActivationOption");

// Built-in face list, used for "all fonts" when no installed faces can be
// listed (no fontconfig). Each entry is a font descriptor.
static NSArray *defaultAvailableFontDescriptors(void) {
    static NSArray *s_defaultDescriptors = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        static const char * const fontNames[] = {
            "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
            "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
            "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
            "ArialMT", "Arial-BoldMT", "Arial-ItalicMT", "Arial-BoldItalicMT",
            "Menlo-Regular", "Menlo-Bold", "Menlo-Italic", "Menlo-BoldItalic",
            "Monaco", "Geneva", "Symbol", "Apple Color Emoji",
            "DejaVu Sans", "DejaVu Sans Bold", "DejaVu Sans Oblique", "DejaVu Sans Bold Oblique",
            "DejaVu Serif", "DejaVu Serif Bold", "DejaVu Serif Italic", "DejaVu Serif Bold Italic",
            "DejaVu Sans Mono", "DejaVu Sans Mono Bold", "DejaVu Sans Mono Oblique", "DejaVu Sans Mono Bold Oblique",
            "Liberation Sans", "Liberation Serif", "Liberation Mono",
            "Roboto", "San Francisco"
        };
        static const char * const fontFamilies[] = {
            "Helvetica", "Helvetica", "Helvetica", "Helvetica",
            "Times", "Times", "Times", "Times",
            "Courier", "Courier", "Courier", "Courier",
            "Arial", "Arial", "Arial", "Arial",
            "Menlo", "Menlo", "Menlo", "Menlo",
            "Monaco", "Geneva", "Symbol", "Apple Color Emoji",
            "DejaVu Sans", "DejaVu Sans", "DejaVu Sans", "DejaVu Sans",
            "DejaVu Serif", "DejaVu Serif", "DejaVu Serif", "DejaVu Serif",
            "DejaVu Sans Mono", "DejaVu Sans Mono", "DejaVu Sans Mono", "DejaVu Sans Mono",
            "Liberation Sans", "Liberation Serif", "Liberation Mono",
            "Roboto", "San Francisco"
        };
        NSMutableArray *list = [NSMutableArray array];
        size_t count = sizeof(fontNames) / sizeof(fontNames[0]);
        for (size_t i = 0; i < count; i++) {
            NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
            NSString *postscript = [NSString stringWithUTF8String:fontNames[i]];
            NSString *family = [NSString stringWithUTF8String:fontFamilies[i]];
            attrs[(id)kCTFontNameAttribute] = postscript;
            attrs[(id)kCTFontFamilyNameAttribute] = family;
            attrs[(id)kCTFontDisplayNameAttribute] = postscript;
            attrs[(id)kCTFontPostScriptNameAttribute] = postscript;
            attrs[(id)kCTFontSizeAttribute] = [NSNumber numberWithDouble:12.0];
            CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes((CFDictionaryRef)attrs);
            [list addObject:(id)descriptor];
            CFRelease(descriptor);
        }
        s_defaultDescriptors = [list copy];
    });
    return s_defaultDescriptors;
}

// A collection keeps the query descriptors it was made from; matching runs
// each query against the installed fonts when it is asked for.
@interface KTFontCollection : NSObject <NSCopying> {
    NSArray *_queries;
    NSDictionary *_options;
    NSArray *_exclusions;
}
- initWithQueries: (NSArray *) queries options: (NSDictionary *) options;
- (NSArray *) queries;
- (NSDictionary *) options;
- (NSArray *) exclusions;
- (void) setQueries: (NSArray *) queries;
- (void) setExclusions: (NSArray *) exclusions;
@end

@implementation KTFontCollection

- initWithQueries: (NSArray *) queries options: (NSDictionary *) options {
    self = [super init];
    if (self == nil)
        return nil;
    _queries = [queries copy];
    _options = [options copy];
    return self;
}

- (void) dealloc {
    [_queries release];
    [_options release];
    [_exclusions release];
    [super dealloc];
}

- (CFTypeID) _cfTypeID {
    return CTFontCollectionGetTypeID();
}

- copyWithZone: (NSZone *) zone {
    return [self retain];
}

- (NSArray *) queries {
    return _queries;
}

- (NSDictionary *) options {
    return _options;
}

- (NSArray *) exclusions {
    return _exclusions;
}

- (void) setQueries: (NSArray *) queries {
    NSArray *copy = [queries copy];
    [_queries release];
    _queries = copy;
}

- (void) setExclusions: (NSArray *) exclusions {
    NSArray *copy = [exclusions copy];
    [_exclusions release];
    _exclusions = copy;
}

@end

static BOOL optionIsSet(NSDictionary *options, CFStringRef key) {
    id value = [options objectForKey: (NSString *) key];
    return [value respondsToSelector: @selector(boolValue)] && [value boolValue];
}

// Whether every attribute of the exclusion descriptor is present, with an equal
// value, in the descriptor. An exclusion without attributes excludes nothing.
static BOOL descriptorMatchesAttributes(CTFontDescriptorRef descriptor, CTFontDescriptorRef exclusion) {
    NSDictionary *wanted = [(NSDictionary *) CTFontDescriptorCopyAttributes(exclusion) autorelease];
    if ([wanted count] == 0)
        return NO;
    NSDictionary *actual = [(NSDictionary *) CTFontDescriptorCopyAttributes(descriptor) autorelease];
    for (id key in wanted) {
        id value = [actual objectForKey: key];
        if (value == nil || ![value isEqual: [wanted objectForKey: key]])
            return NO;
    }
    return YES;
}

CFTypeID CTFontCollectionGetTypeID(void) {
    return (CFTypeID) [KTFontCollection self];
}

// The descriptors the collection matches. The options of the call apply when
// the collection itself does not set the option.
static NSArray *copyMatchingDescriptors(KTFontCollection *object, NSDictionary *callOptions)
{
    NSArray *queries = [object queries];
    BOOL allFonts = queries == nil;
    if (allFonts) {
        CTFontDescriptorRef all = CTFontDescriptorCreateWithAttributes(
                (CFDictionaryRef) [NSDictionary dictionary]);
        queries = [NSArray arrayWithObject: (id) all];
        CFRelease(all);
    }

    // Several queries can match the same face. With
    // kCTFontCollectionRemoveDuplicatesOption, faces with the same
    // PostScript name are also reported once.
    BOOL removeDuplicates;
    if ([[object options] objectForKey: (NSString *) kCTFontCollectionRemoveDuplicatesOption] != nil)
        removeDuplicates = optionIsSet([object options], kCTFontCollectionRemoveDuplicatesOption);
    else
        removeDuplicates = optionIsSet(callOptions, kCTFontCollectionRemoveDuplicatesOption);
    NSMutableArray *matches = [NSMutableArray array];
    NSMutableSet *seen = [NSMutableSet set];
    for (id query in queries) {
        CFArrayRef found = CTFontDescriptorCreateMatchingFontDescriptors(
                (CTFontDescriptorRef) query, NULL);
        if (found == NULL)
            continue;
        for (id match in (NSArray *) found) {
            id key = match;
            if (removeDuplicates) {
                CFTypeRef name = CTFontDescriptorCopyAttribute(
                        (CTFontDescriptorRef) match, kCTFontNameAttribute);
                if (name != NULL)
                    key = [(id) name autorelease];
            }
            if ([seen containsObject: key])
                continue;
            [seen addObject: key];
            [matches addObject: match];
        }
        CFRelease(found);
    }

    // With no installed faces to list, "all fonts" is the built-in list.
    if (allFonts && [matches count] == 0) {
        for (id match in defaultAvailableFontDescriptors()) {
            CTFontDescriptorRef descriptor = (CTFontDescriptorRef) match;
            id key = match;
            if (removeDuplicates) {
                CFTypeRef name = CTFontDescriptorCopyAttribute(descriptor, kCTFontNameAttribute);
                if (name != NULL)
                    key = [(id) name autorelease];
            }
            if (![seen containsObject: key]) {
                [seen addObject: key];
                [matches addObject: match];
            }
        }
    }

    // Faces matched by an exclusion descriptor are left out: those it finds
    // among the installed faces, and those that carry all of its attributes.
    for (id exclusion in [object exclusions]) {
        CFArrayRef excluded = CTFontDescriptorCreateMatchingFontDescriptors(
                (CTFontDescriptorRef) exclusion, NULL);
        if (excluded != NULL) {
            [matches removeObjectsInArray: (NSArray *) excluded];
            CFRelease(excluded);
        }
        for (NSUInteger i = [matches count]; i > 0; i--) {
            if (descriptorMatchesAttributes((CTFontDescriptorRef) [matches objectAtIndex: i - 1],
                                            (CTFontDescriptorRef) exclusion))
                [matches removeObjectAtIndex: i - 1];
        }
    }

    return [matches copy];
}

CTFontCollectionRef CTFontCollectionCreateFromAvailableFonts(CFDictionaryRef options)
{
    // Without query descriptors, matching lists every installed face.
    return (CTFontCollectionRef) [[KTFontCollection alloc]
            initWithQueries: nil
                    options: (NSDictionary *) options];
}

CTFontCollectionRef CTFontCollectionCreateWithFontDescriptors(CFArrayRef _Nullable queryDescriptors,
                                                              CFDictionaryRef _Nullable options)
{
    // A collection made from no descriptors is empty, not "all fonts".
    NSArray *queries = queryDescriptors ? (NSArray *) queryDescriptors : [NSArray array];
    return (CTFontCollectionRef) [[KTFontCollection alloc]
            initWithQueries: queries
                    options: (NSDictionary *) options];
}

CTFontCollectionRef CTFontCollectionCreateCopyWithFontDescriptors(CTFontCollectionRef original,
                                                                  CFArrayRef _Nullable queryDescriptors,
                                                                  CFDictionaryRef _Nullable options)
{
    if (original == NULL)
        return NULL;
    KTFontCollection *collection = (KTFontCollection *) original;
    NSMutableArray *queries = [NSMutableArray array];
    if ([collection queries] != nil)
        [queries addObjectsFromArray: [collection queries]];
    if (queryDescriptors != NULL)
        [queries addObjectsFromArray: (NSArray *) queryDescriptors];
    if (options == NULL)
        options = (CFDictionaryRef) [collection options];
    CTFontCollectionRef copy = CTFontCollectionCreateWithFontDescriptors((CFArrayRef) queries, options);
    [(KTFontCollection *) copy setExclusions: [collection exclusions]];
    return copy;
}

CTMutableFontCollectionRef CTFontCollectionCreateMutableCopy(CTFontCollectionRef original)
{
    if (original == NULL)
        return NULL;
    KTFontCollection *source = (KTFontCollection *) original;
    KTFontCollection *copy = [[KTFontCollection alloc] initWithQueries: [source queries]
                                                               options: [source options]];
    [copy setExclusions: [source exclusions]];
    return (CTMutableFontCollectionRef) copy;
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptors(CTFontCollectionRef collection)
{
    return CTFontCollectionCreateMatchingFontDescriptorsWithOptions(collection, NULL);
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsWithOptions(CTFontCollectionRef collection, CFDictionaryRef options)
{
    if (collection == NULL)
        return NULL;
    NSArray *matches = copyMatchingDescriptors((KTFontCollection *) collection,
                                               (NSDictionary *) options);
    if ([matches count] == 0) {
        [matches release];
        return NULL;
    }
    return (CFArrayRef) matches;
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsForFamily(CTFontCollectionRef collection,
                                                                 CFStringRef familyName,
                                                                 CFDictionaryRef options)
{
    if (collection == NULL || familyName == NULL)
        return NULL;
    CFArrayRef all = CTFontCollectionCreateMatchingFontDescriptorsWithOptions(collection, options);
    if (all == NULL)
        return NULL;

    NSMutableArray *matches = [NSMutableArray array];
    for (id match in (NSArray *) all) {
        CFTypeRef family = CTFontDescriptorCopyAttribute(
                (CTFontDescriptorRef) match, kCTFontFamilyNameAttribute);
        if (family != NULL && CFEqual(family, familyName))
            [matches addObject: match];
        if (family != NULL)
            CFRelease(family);
    }
    CFRelease(all);

    if ([matches count] == 0)
        return NULL;
    return (CFArrayRef) [matches copy];
}

CFArrayRef CTFontCollectionCopyQueryDescriptors(CTFontCollectionRef collection)
{
    if (collection == NULL)
        return NULL;
    NSArray *queries = [(KTFontCollection *) collection queries];
    return queries != nil ? (CFArrayRef) [queries copy] : NULL;
}

void CTFontCollectionSetQueryDescriptors(CTMutableFontCollectionRef collection,
                                         CFArrayRef descriptors)
{
    if (collection == NULL)
        return;
    // No descriptors means the collection lists every installed face.
    [(KTFontCollection *) collection setQueries: (NSArray *) descriptors];
}

CFArrayRef CTFontCollectionCopyExclusionDescriptors(CTFontCollectionRef collection)
{
    if (collection == NULL)
        return NULL;
    NSArray *exclusions = [(KTFontCollection *) collection exclusions];
    return exclusions != nil ? (CFArrayRef) [exclusions copy] : NULL;
}

void CTFontCollectionSetExclusionDescriptors(CTMutableFontCollectionRef collection,
                                             CFArrayRef descriptors)
{
    if (collection == NULL)
        return;
    [(KTFontCollection *) collection setExclusions: (NSArray *) descriptors];
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsSortedWithCallback(
        CTFontCollectionRef collection, CTFontCollectionSortDescriptorsCallback sortCallback,
        void *refCon)
{
    CFArrayRef matches = CTFontCollectionCreateMatchingFontDescriptors(collection);
    if (matches == NULL || sortCallback == NULL)
        return matches;
    NSArray *sorted = [(NSArray *) matches
            sortedArrayUsingComparator: ^NSComparisonResult(id first, id second) {
                return (NSComparisonResult) sortCallback((CTFontDescriptorRef) first,
                                                         (CTFontDescriptorRef) second, refCon);
            }];
    CFRelease(matches);
    return (CFArrayRef) [sorted copy];
}

CFArrayRef CTFontCollectionCopyFontAttribute(CTFontCollectionRef collection, CFStringRef attributeName, CTFontCollectionCopyOptions options)
{
    if (!collection || !attributeName) return NULL;
    CFArrayRef tmp = CTFontCollectionCreateMatchingFontDescriptors(collection);
    if (!tmp) return NULL;
    NSArray *descriptors = (NSArray *)tmp;
    NSMutableArray *result = [NSMutableArray array];
    NSMutableSet *seen = (options & kCTFontCollectionCopyUnique) ? [NSMutableSet set] : nil;
    for (id desc in descriptors) {
        id val = [(id) CTFontDescriptorCopyAttribute((CTFontDescriptorRef) desc, attributeName) autorelease];
        if (val) {
            if (seen) {
                if ([seen containsObject:val]) continue;
                [seen addObject:val];
            }
            [result addObject:val];
        }
    }
    if ((options & kCTFontCollectionCopyStandardSort) && result.count > 1) {
        [result sortUsingComparator:^NSComparisonResult(id obj1, id obj2) {
            if ([obj1 respondsToSelector:@selector(compare:)] && [obj2 isKindOfClass:[obj1 class]]) {
                return [obj1 compare:obj2];
            }
            return NSOrderedSame;
        }];
    }
    CFArrayRef ret = (CFArrayRef)[result copy];
    CFRelease(tmp);
    return ret;
}

CFArrayRef CTFontCollectionCopyFontAttributes(CTFontCollectionRef collection, CFSetRef attributeNames, CTFontCollectionCopyOptions options)
{
    if (!collection || !attributeNames) return NULL;
    CFArrayRef tmp = CTFontCollectionCreateMatchingFontDescriptors(collection);
    if (!tmp) return NULL;
    NSArray *descriptors = (NSArray *)tmp;
    NSMutableArray *result = [NSMutableArray array];
    NSMutableSet *seen = (options & kCTFontCollectionCopyUnique) ? [NSMutableSet set] : nil;
    for (id desc in descriptors) {
        NSMutableDictionary *subDict = [NSMutableDictionary dictionary];
        for (id key in (NSSet *)attributeNames) {
            id val = [(id) CTFontDescriptorCopyAttribute((CTFontDescriptorRef) desc, (CFStringRef) key) autorelease];
            if (val) subDict[key] = val;
        }
        if (subDict.count > 0) {
            if (seen) {
                // TODO: Apple deduplicates by font identity (PostScript name), not full dictionary equality.
                id identityKey = [(id) CTFontDescriptorCopyAttribute((CTFontDescriptorRef) desc, kCTFontNameAttribute) autorelease]
                    ?: [(id) CTFontDescriptorCopyAttribute((CTFontDescriptorRef) desc, kCTFontPostScriptNameAttribute) autorelease]
                    ?: subDict;
                if ([seen containsObject:identityKey]) continue;
                [seen addObject:identityKey];
            }
            [result addObject:subDict];
        }
    }
    if ((options & kCTFontCollectionCopyStandardSort) && result.count > 1) {
        [result sortUsingComparator:^NSComparisonResult(NSDictionary *d1, NSDictionary *d2) {
            NSString *n1 = [d1 objectForKey:(id)kCTFontNameAttribute] ?: [d1 objectForKey:(id)kCTFontFamilyNameAttribute];
            NSString *n2 = [d2 objectForKey:(id)kCTFontNameAttribute] ?: [d2 objectForKey:(id)kCTFontFamilyNameAttribute];
            if ([n1 isKindOfClass:[NSString class]] && [n2 isKindOfClass:[NSString class]]) {
                return [n1 compare:n2];
            }
            return NSOrderedSame;
        }];
    }
    CFArrayRef ret = (CFArrayRef)[result copy];
    CFRelease(tmp);
    return ret;
}
