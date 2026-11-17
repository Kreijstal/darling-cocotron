#import "DarlingLayout.h"
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSLayoutGuide.h>
#import <Foundation/NSException.h>
#include "third_party/kiwi/kiwi.h"
#include <map>
#include <cmath>
#include <objc/message.h>

// UIKit's (iOS) attributes past AppKit's: an edge or center measured from a view's layout
// margins (-[UIView layoutMargins]; UIView is an NSView here).
enum {
    DarlingLayoutAttributeLeftMargin = 13,
    DarlingLayoutAttributeRightMargin,
    DarlingLayoutAttributeTopMargin,
    DarlingLayoutAttributeBottomMargin,
    DarlingLayoutAttributeLeadingMargin,
    DarlingLayoutAttributeTrailingMargin,
    DarlingLayoutAttributeCenterXWithinMargins,
    DarlingLayoutAttributeCenterYWithinMargins,
};

// Layout guides take part as rectangles with no view; their frame is set here.
@interface NSLayoutGuide (DarlingLayout)
- (void)setFrame:(NSRect)frame;
@end

namespace {
struct Geometry { kiwi::Variable x, y, width, height; };
class Layout {
    NSView *root;
    // Fitting solves for the root's own size instead of pinning it, with the
    // vertical axis running downwards whatever the root's flippedness is.
    bool fitting;
    kiwi::Solver solver;
    // Views and layout guides, each a rectangle in the root's coordinates.
    std::map<id, Geometry> geometry;
    std::vector<NSLayoutGuide *> guides;
    std::vector<NSView *> traversal;
    NSMutableArray *constraints;
    std::map<double, std::vector<kiwi::Constraint>, std::greater<double>> optional;

    void collect(NSView *view) {
        geometry.emplace(view, Geometry());
        traversal.push_back(view);
        [constraints addObjectsFromArray:[view constraints]];
        for (NSLayoutGuide *guide in [view layoutGuides]) {
            geometry.emplace(guide, Geometry());
            guides.push_back(guide);
        }
        for (NSView *child in [view subviews]) collect(child);
    }

    // -[UIView layoutMargins] (a UIEdgeInsets); layout guides and AppKit views have none.
    struct Margins { CGFloat top, left, bottom, right; };
    static Margins margins(id item) {
        SEL selector = @selector(layoutMargins);
        if ([item isKindOfClass:[NSLayoutGuide class]] || ![item respondsToSelector:selector])
            return Margins{0, 0, 0, 0};
        return ((Margins (*)(id, SEL))objc_msgSend)(item, selector);
    }

    bool contains(id item) {
        return item != nil && geometry.find(item) != geometry.end();
    }

    bool rootFlipped() const { return fitting || [root isFlipped]; }

    // Maps a rectangle in the root's coordinates to the solver's, where y runs
    // downwards in fitting mode (the root's height is a variable there).
    NSRect solverRect(NSRect rect) const {
        if (fitting && ![root isFlipped])
            rect.origin.y = NSMaxY([root bounds]) - NSMaxY(rect);
        return rect;
    }

    kiwi::Expression attribute(id item, NSInteger attr) {
        auto found = geometry.find(item);
        if (found == geometry.end())
            [NSException raise:NSInvalidArgumentException format:@"Layout item is outside the view tree: %@", item];
        Geometry &g = found->second;
        NSView *view = [item isKindOfClass:[NSLayoutGuide class]] ? [(NSLayoutGuide *)item owningView] : (NSView *)item;
        bool rtl = [view userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft;
        switch (attr) {
        case NSLayoutAttributeLeading: attr = rtl ? NSLayoutAttributeRight : NSLayoutAttributeLeft; break;
        case NSLayoutAttributeTrailing: attr = rtl ? NSLayoutAttributeLeft : NSLayoutAttributeRight; break;
        case DarlingLayoutAttributeLeadingMargin: attr = rtl ? DarlingLayoutAttributeRightMargin : DarlingLayoutAttributeLeftMargin; break;
        case DarlingLayoutAttributeTrailingMargin: attr = rtl ? DarlingLayoutAttributeLeftMargin : DarlingLayoutAttributeRightMargin; break;
        }
        if (attr >= DarlingLayoutAttributeLeftMargin && attr <= DarlingLayoutAttributeCenterYWithinMargins) {
            Margins m = margins(item);
            switch (attr) {
            case DarlingLayoutAttributeLeftMargin: return attribute(item, NSLayoutAttributeLeft) + m.left;
            case DarlingLayoutAttributeRightMargin: return attribute(item, NSLayoutAttributeRight) - m.right;
            case DarlingLayoutAttributeTopMargin: return attribute(item, NSLayoutAttributeTop) + m.top;
            case DarlingLayoutAttributeBottomMargin: return attribute(item, NSLayoutAttributeBottom) - m.bottom;
            case DarlingLayoutAttributeCenterXWithinMargins:
                return attribute(item, NSLayoutAttributeCenterX) + (m.left - m.right) * 0.5;
            default:
                return attribute(item, NSLayoutAttributeCenterY) + (m.top - m.bottom) * 0.5;
            }
        }
        // Auto Layout y values increase downwards in AppKit too, so an unflipped
        // root's y is measured from its top edge.
        bool flipped = rootFlipped();
        double top = NSMaxY([root bounds]);
        // The edges and sizes layout constraints refer to are those of the view's
        // alignment rectangle: the frame less the alignment insets (layout guides
        // have none). Insets name the visual sides, so top and bottom are the
        // same in any coordinate system.
        NSEdgeInsets insets = {0, 0, 0, 0};
        if (![item isKindOfClass:[NSLayoutGuide class]])
            insets = [view alignmentRectInsets];
        kiwi::Expression left = g.x + insets.left;
        kiwi::Expression right = g.x + g.width - insets.right;
        kiwi::Expression above = flipped ? g.y + insets.top : top - (g.y + g.height) + insets.top;
        kiwi::Expression below = flipped ? g.y + g.height - insets.bottom : top - g.y - insets.bottom;
        switch (attr) {
        case NSLayoutAttributeLeft: return left;
        case NSLayoutAttributeRight: return right;
        case NSLayoutAttributeTop: return above;
        case NSLayoutAttributeBottom: return below;
        case NSLayoutAttributeWidth: return g.width - (insets.left + insets.right);
        case NSLayoutAttributeHeight: return g.height - (insets.top + insets.bottom);
        case NSLayoutAttributeCenterX: return (left + right) * 0.5;
        case NSLayoutAttributeCenterY: return (above + below) * 0.5;
        case NSLayoutAttributeFirstBaseline:
            return above + [view firstBaselineOffsetFromTop];
        case NSLayoutAttributeLastBaseline:
            return below - [view lastBaselineOffsetFromBottom];
        default:
            [NSException raise:NSInvalidArgumentException format:@"Unsupported layout attribute: %ld", (long)attr];
            return kiwi::Expression();
        }
    }

    void add(const kiwi::Constraint &constraint) {
        if (constraint.strength() == kiwi::strength::required)
            solver.addConstraint(constraint);
        else
            optional[constraint.strength()].push_back(constraint);
    }

    void solvePriorities() {
        for (auto &tier : optional) {
            std::vector<kiwi::Constraint> soft, frozen;
            for (const auto &constraint : tier.second) {
                soft.push_back(constraint | 1.0);
                solver.addConstraint(soft.back());
            }
            solver.updateVariables();
            for (const auto &constraint : tier.second) {
                const auto &expression = constraint.expression();
                double residual = expression.value();
                bool satisfied = kiwi::impl::nearZero(residual) ||
                    (constraint.op() == kiwi::OP_LE && residual < 0) ||
                    (constraint.op() == kiwi::OP_GE && residual > 0);
                if (satisfied)
                    frozen.push_back(constraint | kiwi::strength::required);
                else
                    frozen.push_back(expression == residual);
            }
            for (const auto &constraint : soft) solver.removeConstraint(constraint);
            for (const auto &constraint : frozen) solver.addConstraint(constraint);
        }
        solver.updateVariables();
    }

    void intrinsic(NSView *view, Geometry &g) {
        NSSize size = [view intrinsicContentSize];
        NSEdgeInsets insets = [view alignmentRectInsets];
        // The intrinsic size is that of the alignment rectangle.
        kiwi::Expression dimensions[] = {g.width - (insets.left + insets.right),
                                         g.height - (insets.top + insets.bottom)};
        double values[] = {size.width, size.height};
        for (int axis = 0; axis < 2; ++axis) {
            if (values[axis] == NSViewNoIntrinsicMetric) continue;
            if (!std::isfinite(values[axis]) || values[axis] < 0)
                [NSException raise:NSInvalidArgumentException format:@"Invalid intrinsic size for %@", view];
            double hugging = [view contentHuggingPriorityForOrientation:(NSLayoutConstraintOrientation)axis];
            double compression = [view contentCompressionResistancePriorityForOrientation:(NSLayoutConstraintOrientation)axis];
            add((dimensions[axis] <= values[axis]) | hugging);
            add((dimensions[axis] >= values[axis]) | compression);
        }
    }

public:
    explicit Layout(NSView *view, bool fittingSolve = false)
        : root(view), fitting(fittingSolve), constraints([[NSMutableArray alloc] init]) {}
    ~Layout() { [constraints release]; }
    // The size of the root that satisfies the constraints of its subtree with
    // the least room: the root is not pinned to its frame and is pulled
    // towards zero size at NSLayoutPriorityFittingSizeCompression, so anything
    // of higher priority (required constraints, intrinsic sizes that resist
    // compression) keeps it open. No view is touched.
    NSSize fittingSize() {
        collect(root);
        buildConstraints();
        Geometry &g = geometry.at(root);
        add(g.x == 0.0);
        add(g.y == 0.0);
        add((g.width == 0.0) | (double)NSLayoutPriorityFittingSizeCompression);
        add((g.height == 0.0) | (double)NSLayoutPriorityFittingSizeCompression);
        solvePriorities();
        return NSMakeSize(g.width.value(), g.height.value());
    }

    void run() {
        collect(root);
        if ([constraints count] == 0) return;
        buildConstraints();
        solvePriorities();
        applyResults();
    }

private:
    void buildConstraints() {
        for (auto &entry : geometry) {
            Geometry &g = entry.second;
            if ([entry.first isKindOfClass:[NSLayoutGuide class]]) {
                add(g.width >= 0.0); add(g.height >= 0.0);
                continue;
            }
            NSView *view = entry.first;
            if (view == root && fitting) {
                add(g.width >= 0.0); add(g.height >= 0.0);
                intrinsic(view, g);
                continue;
            }
            NSRect frame = view == root ? [root bounds] : solverRect([view convertRect:[view bounds] toView:root]);
            if (view == root) {
                add(g.x == frame.origin.x); add(g.y == frame.origin.y);
                add(g.width == frame.size.width); add(g.height == frame.size.height);
            } else if ([view translatesAutoresizingMaskIntoConstraints]) {
                NSUInteger mask = [view autoresizingMask];
                auto p_found = geometry.find([view superview]);
                // Full resizing mask covers container-filling views.
                // TODO: handle partial autoresizing masks (e.g. single-axis flexible, margins)
                if (!fitting && p_found != geometry.end() && (mask & (NSViewWidthSizable | NSViewHeightSizable)) == (NSViewWidthSizable | NSViewHeightSizable)) {
                    Geometry &p = p_found->second;
                    NSRect curFrame = [view frame];
                    NSView *parentView = [view superview];
                    NSRect pBounds = [parentView bounds];
                    CGFloat leftMargin = curFrame.origin.x - pBounds.origin.x;
                    CGFloat rightMargin = (pBounds.origin.x + pBounds.size.width) - (curFrame.origin.x + curFrame.size.width);
                    CGFloat bottomMargin, topMargin;
                    if ([parentView isFlipped]) {
                        topMargin = curFrame.origin.y - pBounds.origin.y;
                        bottomMargin = (pBounds.origin.y + pBounds.size.height) - (curFrame.origin.y + curFrame.size.height);
                    } else {
                        bottomMargin = curFrame.origin.y - pBounds.origin.y;
                        topMargin = (pBounds.origin.y + pBounds.size.height) - (curFrame.origin.y + curFrame.size.height);
                    }

                    add(g.x == p.x + leftMargin);
                    add(g.y == p.y + bottomMargin);
                    add(g.width == p.width - (leftMargin + rightMargin));
                    add(g.height == p.height - (bottomMargin + topMargin));
                } else {
                    add(g.x == frame.origin.x); add(g.y == frame.origin.y);
                    add(g.width == frame.size.width); add(g.height == frame.size.height);
                }
            } else {
                add(g.width >= 0.0); add(g.height >= 0.0);
                intrinsic(view, g);
            }
        }
        for (NSLayoutConstraint *constraint in constraints) {
            if (![constraint isActive]) continue;
            // An item not (yet) in this tree: the constraint waits for a
            // layout that includes it, as iOS lays out once views are in place.
            if (!contains([constraint firstItem]) || ([constraint secondItem] && !contains([constraint secondItem])))
                continue;
            double priority = [constraint priority];
            if (!std::isfinite(priority) || priority <= 0 || priority > NSLayoutPriorityRequired)
                [NSException raise:NSInvalidArgumentException format:@"Invalid layout priority: %@", constraint];
            double strength = priority == NSLayoutPriorityRequired ? kiwi::strength::required : priority;
            kiwi::Expression expression = attribute([constraint firstItem], [constraint firstAttribute]);
            if ([constraint secondItem])
                expression = expression - attribute([constraint secondItem], [constraint secondAttribute]) * [constraint multiplier];
            expression = expression - [constraint constant];
            switch ([constraint relation]) {
            case NSLayoutRelationEqual: add((expression == 0.0) | strength); break;
            case NSLayoutRelationLessThanOrEqual: add((expression <= 0.0) | strength); break;
            case NSLayoutRelationGreaterThanOrEqual: add((expression >= 0.0) | strength); break;
            default: [NSException raise:NSInvalidArgumentException format:@"Invalid constraint relation: %@", constraint];
            }
        }
    }

    void applyResults() {
        // Convert every result before changing any ancestor frame.
        NSMutableArray *views = [NSMutableArray array];
        NSMutableArray *frames = [NSMutableArray array];
        for (NSView *view : traversal) {
            if (view == root) continue;
            if ([view translatesAutoresizingMaskIntoConstraints]) {
                NSUInteger mask = [view autoresizingMask];
                if ((mask & (NSViewWidthSizable | NSViewHeightSizable)) != (NSViewWidthSizable | NSViewHeightSizable))
                    continue;
            }
            auto g_it = geometry.find(view);
            if (g_it == geometry.end()) continue;
            Geometry &g = g_it->second;
            NSView *parent = [view superview];
            if (!parent) continue;
            auto p_it = geometry.find(parent);
            if (p_it == geometry.end()) continue;
            Geometry &p = p_it->second;
            NSRect absolute = NSMakeRect(g.x.value(), g.y.value(), g.width.value(), g.height.value());
            NSRect relative = NSMakeRect(absolute.origin.x - p.x.value(), absolute.origin.y - p.y.value(), absolute.size.width, absolute.size.height);
            if ([parent isFlipped] != [root isFlipped])
                relative.origin.y = p.height.value() - relative.origin.y - relative.size.height;
            relative.origin.x += [parent bounds].origin.x;
            relative.origin.y += [parent bounds].origin.y;
            [views addObject:view]; [frames addObject:[NSValue valueWithRect:relative]];
        }
        for (NSUInteger i = 0; i < [views count]; ++i)
            [[views objectAtIndex:i] setFrame:[[frames objectAtIndex:i] rectValue]];
        // A guide's frame is in its owning view's bounds.
        for (NSLayoutGuide *guide : guides) {
            NSView *owner = [guide owningView];
            auto g_it = geometry.find(guide), p_it = geometry.find(owner);
            if (owner == nil || g_it == geometry.end() || p_it == geometry.end()) continue;
            Geometry &g = g_it->second, &p = p_it->second;
            NSRect relative = NSMakeRect(g.x.value() - p.x.value(), g.y.value() - p.y.value(), g.width.value(), g.height.value());
            if ([owner isFlipped] != [root isFlipped])
                relative.origin.y = p.height.value() - relative.origin.y - relative.size.height;
            relative.origin.x += [owner bounds].origin.x;
            relative.origin.y += [owner bounds].origin.y;
            [guide setFrame:relative];
        }
    }
};
}

NSSize DarlingLayoutFittingSize(NSView *root) {
    try { return Layout(root, true).fittingSize(); }
    catch (const std::exception &error) {
        NSLog(@"Unable to simultaneously satisfy constraints: %s", error.what());
        return NSZeroSize;
    }
}

void DarlingLayoutSubtree(NSView *root) {
    try { Layout(root).run(); }
    catch (const std::exception &error) {
        NSLog(@"Unable to simultaneously satisfy constraints: %s", error.what());
    }
}
