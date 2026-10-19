// Host MODEL of the nextDrawable pool protocol in QuartzCore/CAMetalLayer.mm and
// CALayerContext.m. QuartzCore cannot be compiled on the host (it needs Vulkan, GL
// and the Darling runtime), so this is a C state machine with the same rules, not the
// Objective-C itself. It pins the design:
//   1. A slot returns to the pool only inside a render pass, and a pass releases the
//      PREVIOUSLY presented drawable (-prepareRender, CAMetalLayer.mm: didPresent on
//      _lastPresentedDrawable, then _lastPresentedDrawable = the one just taken), so the
//      first pass after an empty _lastPresentedDrawable frees nothing.
//   2. nextDrawable on the main thread runs a pass itself when presents are queued.
//   3. Only the main thread reads the unretained context pointer.
//   4. A pass never nests, whichever entry point asks for it.
//   5. If a pass takes no present, the caller waits instead of looping.
// Build and run with tests/quartzcore-nextdrawable-pool.sh.
#include <stdbool.h>
#include <stdio.h>

#define POOL 3

static int failures;
#define CHECK(c, msg) do { if (!(c)) { failures++; printf("FAIL line %d: %s\n", __LINE__, msg); } } while (0)

typedef struct {
    int usable;        // free slots
    int queued;        // presented, waiting for a pass to composite them
    bool hasLast;      // _lastPresentedDrawable is set
    bool context;      // the unretained layer->context pointer is valid
    bool hasWindow;    // renderLayer can attach (else it returns early)
    bool inProgress;   // CALayerContext._renderInProgress
    int depth, maxDepth;
    int passes;
    bool badContextRead;
    bool nestedFromHandler; // the presented handler calls nextDrawable again
    bool nestedRender;      // app code in the pass causes a display pass (NSView path)
    int waits;
} Model;

static int nextDrawable(Model *m, bool mainThread);

// CALayerContext -renderLayer: with the guard. Taking a queued present frees its slot.
static void renderLayer(Model *m) {
#ifndef MUT_NO_GUARD
    if (m->inProgress)
        return;
#endif
    m->inProgress = true;
    m->depth++;
    if (m->depth > m->maxDepth)
        m->maxDepth = m->depth;
    if (m->hasWindow) {
        // -prepareRender: take the first queued present, release the previous one.
        bool took = m->queued > 0;
        if (took) {
            m->passes++;
            m->queued--;
        }
#ifdef MUT_FREE_TAKEN
        if (took)
            m->usable++;
#else
        if (m->hasLast)
            m->usable++;
        m->hasLast = took;
#endif
        if (m->nestedFromHandler) { // app code inside the pass
            m->nestedFromHandler = false;
            nextDrawable(m, true);
        }
        if (m->nestedRender) {
            m->nestedRender = false;
            renderLayer(m);
        }
    }
    m->depth--;
    m->inProgress = false;
}

static bool renderPendingPresents(Model *m, bool mainThread) {
    if (m->inProgress || !mainThread)
        return false;
    renderLayer(m);
    return true;
}

// Returns the number of times the caller had to wait; -1 for the 1 s timeout.
static int nextDrawable(Model *m, bool mainThread) {
    int waited = 0;
    while (m->usable == 0) {
#ifndef MUT_NO_MAIN_CHECK
        if (mainThread && m->queued > 0) {
#else
        if (m->queued > 0) {
#endif
            if (!m->context)
                m->badContextRead = true; // would message freed memory
            int before = m->queued;
            bool rendered = renderPendingPresents(m, mainThread);
            if (rendered && m->queued < before)
                continue;
        }
        // The real code blocks on the condition; the timer pass on the main thread
        // would free a slot. The model has one thread, so it counts the wait and ends it.
        waited++;
        m->waits++;
        return -1;
    }
    m->usable--;
    return waited;
}

static Model fresh(void) {
    Model m = {0};
    m.context = true;
    m.hasWindow = true;
    return m;
}

int main(void) {
    // 1+2: the whole pool is presented; the main thread finds a slot without waiting.
    Model m = fresh();
    m.queued = POOL;
    CHECK(nextDrawable(&m, true) == 0, "main thread frees a slot by running a pass");
    // The first pass only composites the oldest present: nothing was presented
    // before it, so no slot is released. The second pass releases that one.
    CHECK(m.passes == 2, "two passes for the first slot, as -prepareRender releases the previous drawable");
    CHECK(m.queued == POOL - 2, "two presents composited");

    // Each further call needs one more pass. After the last present is composited the
    // slot it shows is released only by the next pass, which the main-thread timer
    // runs; nextDrawable has nothing queued to run and waits for it.
    m = fresh(); m.queued = POOL;
    CHECK(nextDrawable(&m, true) == 0, "first drawable");
    CHECK(nextDrawable(&m, true) == 0, "second drawable, one pass");
    CHECK(nextDrawable(&m, true) == -1, "third: nothing queued, the timer pass releases the last one");
    CHECK(m.queued == 0 && m.usable == 0, "all presents composited");
    renderLayer(&m); // the timer
    CHECK(nextDrawable(&m, true) == 0, "after the timer pass the last slot is back");

    // 3: another thread does not touch the context, even if it has been freed.
    m = fresh(); m.queued = POOL; m.context = false;
    CHECK(nextDrawable(&m, false) == -1, "worker thread waits for the main thread timer");
    CHECK(!m.badContextRead, "worker thread never reads the context");

    // 4: app code in the presented handler asks for a drawable again.
    m = fresh(); m.queued = POOL; m.nestedFromHandler = true;
    nextDrawable(&m, true);
    CHECK(m.maxDepth == 1, "a pass never nests");

    // Layout or a delegate inside the pass triggers a display pass (NSView.m path).
    m = fresh(); m.queued = POOL; m.nestedRender = true;
    nextDrawable(&m, true);
    CHECK(m.maxDepth == 1, "a direct render inside a pass does not nest");

    // A pass already running (the NSView display path) refuses a second one.
    m = fresh(); m.queued = POOL; m.inProgress = true;
    CHECK(nextDrawable(&m, true) == -1, "refused while a pass runs: waits");
    CHECK(m.depth == 0 && m.passes == 0, "no pass started inside a pass");

    // 5: no window yet: the pass takes nothing, so the caller waits instead of spinning.
    m = fresh(); m.queued = POOL; m.hasWindow = false;
    CHECK(nextDrawable(&m, true) == -1, "pass took nothing: falls through to the wait");
    CHECK(m.passes == 0, "no pass without a window");

    // Nothing queued and the app holds the whole pool: wait, as on macOS.
    m = fresh();
    CHECK(nextDrawable(&m, true) == -1, "nothing queued: waits");

    if (failures)
        return 1;
    printf("PASS\n");
    return 0;
}
