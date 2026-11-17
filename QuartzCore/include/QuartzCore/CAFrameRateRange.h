#import <QuartzCore/CABase.h>
#include <stdbool.h>

// A range of frame rates, in frames per second, that an animation or display
// link is able to run at, together with the rate it prefers.
// The three-float layout is ABI: callers built against the macOS SDK pass it
// by value, and Swift mangles it as So16CAFrameRateRangeV.
struct CAFrameRateRange {
    float minimum;
    float maximum;
    float preferred;
};

typedef struct CAFrameRateRange CAFrameRateRange;

// {0, 0, 0}: no preference, the system chooses the frame rate.
CA_EXTERN const CAFrameRateRange CAFrameRateRangeDefault;

CA_EXTERN CAFrameRateRange CAFrameRateRangeMake(float minimum, float maximum, float preferred);

// Field-wise comparison of all three members.
CA_EXTERN bool CAFrameRateRangeIsEqualToRange(CAFrameRateRange range, CAFrameRateRange other);
