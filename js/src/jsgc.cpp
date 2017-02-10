/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This code implements an incremental mark-and-sweep garbage collector, with
 * most sweeping carried out in the background on a parallel thread.
 *
 * Full vs. zone GC
 * ----------------
 *
 * The collector can collect all zones at once, or a subset. These types of
 * collection are referred to as a full GC and a zone GC respectively.
 *
 * The atoms zone is only collected in a full GC since objects in any zone may
 * have pointers to atoms, and these are not recorded in the cross compartment
 * pointer map. Also, the atoms zone is not collected if any thread has an
 * AutoKeepAtoms instance on the stack, or there are any exclusive threads using
 * the runtime.
 *
 * It is possible for an incremental collection that started out as a full GC to
 * become a zone GC if new zones are created during the course of the
 * collection.
 *
 * Incremental collection
 * ----------------------
 *
 * For a collection to be carried out incrementally the following conditions
 * must be met:
 *  - the collection must be run by calling js::GCSlice() rather than js::GC()
 *  - the GC mode must have been set to JSGC_MODE_INCREMENTAL with
 *    JS_SetGCParameter()
 *  - no thread may have an AutoKeepAtoms instance on the stack
 *
 * The last condition is an engine-internal mechanism to ensure that incremental
 * collection is not carried out without the correct barriers being implemented.
 * For more information see 'Incremental marking' below.
 *
 * If the collection is not incremental, all foreground activity happens inside
 * a single call to GC() or GCSlice(). However the collection is not complete
 * until the background sweeping activity has finished.
 *
 * An incremental collection proceeds as a series of slices, interleaved with
 * mutator activity, i.e. running JavaScript code. Slices are limited by a time
 * budget. The slice finishes as soon as possible after the requested time has
 * passed.
 *
 * Collector states
 * ----------------
 *
 * The collector proceeds through the following states, the current state being
 * held in JSRuntime::gcIncrementalState:
 *
 *  - MarkRoots  - marks the stack and other roots
 *  - Mark       - incrementally marks reachable things
 *  - Sweep      - sweeps zones in groups and continues marking unswept zones
 *  - Finalize   - performs background finalization, concurrent with mutator
 *  - Compact    - incrementally compacts by zone
 *  - Decommit   - performs background decommit and chunk removal
 *
 * The MarkRoots activity always takes place in the first slice. The next two
 * states can take place over one or more slices.
 *
 * In other words an incremental collection proceeds like this:
 *
 * Slice 1:   MarkRoots:  Roots pushed onto the mark stack.
 *            Mark:       The mark stack is processed by popping an element,
 *                        marking it, and pushing its children.
 *
 *          ... JS code runs ...
 *
 * Slice 2:   Mark:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n-1: Mark:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n:   Mark:       Mark stack is completely drained.
 *            Sweep:      Select first group of zones to sweep and sweep them.
 *
 *          ... JS code runs ...
 *
 * Slice n+1: Sweep:      Mark objects in unswept zones that were newly
 *                        identified as alive (see below). Then sweep more zone
 *                        groups.
 *
 *          ... JS code runs ...
 *
 * Slice n+2: Sweep:      Mark objects in unswept zones that were newly
 *                        identified as alive. Then sweep more zone groups.
 *
 *          ... JS code runs ...
 *
 * Slice m:   Sweep:      Sweeping is finished, and background sweeping
 *                        started on the helper thread.
 *
 *          ... JS code runs, remaining sweeping done on background thread ...
 *
 * When background sweeping finishes the GC is complete.
 *
 * Incremental marking
 * -------------------
 *
 * Incremental collection requires close collaboration with the mutator (i.e.,
 * JS code) to guarantee correctness.
 *
 *  - During an incremental GC, if a memory location (except a root) is written
 *    to, then the value it previously held must be marked. Write barriers
 *    ensure this.
 *
 *  - Any object that is allocated during incremental GC must start out marked.
 *
 *  - Roots are marked in the first slice and hence don't need write barriers.
 *    Roots are things like the C stack and the VM stack.
 *
 * The problem that write barriers solve is that between slices the mutator can
 * change the object graph. We must ensure that it cannot do this in such a way
 * that makes us fail to mark a reachable object (marking an unreachable object
 * is tolerable).
 *
 * We use a snapshot-at-the-beginning algorithm to do this. This means that we
 * promise to mark at least everything that is reachable at the beginning of
 * collection. To implement it we mark the old contents of every non-root memory
 * location written to by the mutator while the collection is in progress, using
 * write barriers. This is described in gc/Barrier.h.
 *
 * Incremental sweeping
 * --------------------
 *
 * Sweeping is difficult to do incrementally because object finalizers must be
 * run at the start of sweeping, before any mutator code runs. The reason is
 * that some objects use their finalizers to remove themselves from caches. If
 * mutator code was allowed to run after the start of sweeping, it could observe
 * the state of the cache and create a new reference to an object that was just
 * about to be destroyed.
 *
 * Sweeping all finalizable objects in one go would introduce long pauses, so
 * instead sweeping broken up into groups of zones. Zones which are not yet
 * being swept are still marked, so the issue above does not apply.
 *
 * The order of sweeping is restricted by cross compartment pointers - for
 * example say that object |a| from zone A points to object |b| in zone B and
 * neither object was marked when we transitioned to the Sweep phase. Imagine we
 * sweep B first and then return to the mutator. It's possible that the mutator
 * could cause |a| to become alive through a read barrier (perhaps it was a
 * shape that was accessed via a shape table). Then we would need to mark |b|,
 * which |a| points to, but |b| has already been swept.
 *
 * So if there is such a pointer then marking of zone B must not finish before
 * marking of zone A.  Pointers which form a cycle between zones therefore
 * restrict those zones to being swept at the same time, and these are found
 * using Tarjan's algorithm for finding the strongly connected components of a
 * graph.
 *
 * GC things without finalizers, and things with finalizers that are able to run
 * in the background, are swept on the background thread. This accounts for most
 * of the sweeping work.
 *
 * Reset
 * -----
 *
 * During incremental collection it is possible, although unlikely, for
 * conditions to change such that incremental collection is no longer safe. In
 * this case, the collection is 'reset' by ResetIncrementalGC(). If we are in
 * the mark state, this just stops marking, but if we have started sweeping
 * already, we continue until we have swept the current zone group. Following a
 * reset, a new non-incremental collection is started.
 *
 * Compacting GC
 * -------------
 *
 * Compacting GC happens at the end of a major GC as part of the last slice.
 * There are three parts:
 *
 *  - Arenas are selected for compaction.
 *  - The contents of those arenas are moved to new arenas.
 *  - All references to moved things are updated.
 */

#include "jsgcinlines.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"

#include <ctype.h>
#include <string.h>
#ifndef XP_WIN
# include <sys/mman.h>
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#ifdef XP_WIN
# include "jswin.h"
#endif

#include "gc/FindSCCs.h"
#include "gc/GCInternals.h"
#include "gc/GCTrace.h"
#include "gc/Heap-inl.h"
#include "gc/Marking.h"
#include "gc/Memory.h"
#include "gc/Policy.h"
#include "jit/BaselineJIT.h"
#include "jit/IonCode.h"
#include "jit/JitcodeMap.h"
#include "js/SliceBudget.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Debugger.h"
#include "vm/ProxyObject.h"
#include "vm/Shape.h"
#include "vm/SPSProfiler.h"
#include "vm/String.h"
#include "vm/Symbol.h"
#include "vm/Time.h"
#include "vm/TraceLogging.h"
#include "vm/WrapperObject.h"

#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayLength;
using mozilla::Get;
using mozilla::Maybe;
using mozilla::Swap;

using JS::AutoGCRooter;

/* Increase the IGC marking slice time if we are in highFrequencyGC mode. */
static const int IGC_MARK_SLICE_MULTIPLIER = 2;

const AllocKind gc::slotsToThingKind[] = {
    /*  0 */ AllocKind::OBJECT0,  AllocKind::OBJECT2,  AllocKind::OBJECT2,  AllocKind::OBJECT4,
    /*  4 */ AllocKind::OBJECT4,  AllocKind::OBJECT8,  AllocKind::OBJECT8,  AllocKind::OBJECT8,
    /*  8 */ AllocKind::OBJECT8,  AllocKind::OBJECT12, AllocKind::OBJECT12, AllocKind::OBJECT12,
    /* 12 */ AllocKind::OBJECT12, AllocKind::OBJECT16, AllocKind::OBJECT16, AllocKind::OBJECT16,
    /* 16 */ AllocKind::OBJECT16
};

#ifdef OMR // Sizes

 const uint32_t OmrGcHelper::thingSizes[] = {
 #define EXPAND_THING_SIZE(allocKind, traceKind, type, sizedType) \
     sizeof(sizedType),
 FOR_EACH_ALLOCKIND(EXPAND_THING_SIZE)
 #undef EXPAND_THING_SIZE
 };

#else // OMR Sizes

 const uint32_t Arena::ThingSizes[] = {
 #define EXPAND_THING_SIZE(allocKind, traceKind, type, sizedType) \
     sizeof(sizedType),
 FOR_EACH_ALLOCKIND(EXPAND_THING_SIZE)
 #undef EXPAND_THING_SIZE
 };

 #define COUNT(type) 1

 const uint32_t Arena::ThingsPerArena[] = {
 #define EXPAND_THINGS_PER_ARENA(allocKind, traceKind, type, sizedType) \
     COUNT(sizedType),
 FOR_EACH_ALLOCKIND(EXPAND_THINGS_PER_ARENA)
 #undef EXPAND_THINGS_PER_ARENA
 };

#endif // ! OMR Sizes

#ifndef OMR // Arena
template<>
JSObject*
ArenaCellIterImpl::get<JSObject>() const
{
    MOZ_ASSERT(!done());
    return reinterpret_cast<JSObject*>(getCell());
}

/* static */ void
Arena::staticAsserts()
{
}

template<typename T>
inline size_t
Arena::finalize(FreeOp* fop, AllocKind thingKind, size_t thingSize)
{
    return 0;
}
#endif // ! OMR  Arena

#ifdef JS_GC_ZEAL

void
GCRuntime::getZealBits(uint32_t* zealBits, uint32_t* frequency, uint32_t* scheduled)
{
}

const char* gc::ZealModeHelpText =
    "  Specifies how zealous the garbage collector should be. Some of these modes can\n"
    "  be set simultaneously, by passing multiple level options, e.g. \"2;4\" will activate\n"
    "  both modes 2 and 4. Modes can be specified by name or number.\n"
    "  \n"
    "  Values:\n"
    "    0: (None) Normal amount of collection (resets all modes)\n"
    "    1: (Poke) Collect when roots are added or removed\n"
    "    2: (Alloc) Collect when every N allocations (default: 100)\n"
    "    3: (FrameGC) Collect when the window paints (browser only)\n"
    "    4: (VerifierPre) Verify pre write barriers between instructions\n"
    "    5: (FrameVerifierPre) Verify pre write barriers between paints\n"
    "    6: (StackRooting) Verify stack rooting\n"
    "    7: (GenerationalGC) Collect the nursery every N nursery allocations\n"
    "    8: (IncrementalRootsThenFinish) Incremental GC in two slices: 1) mark roots 2) finish collection\n"
    "    9: (IncrementalMarkAllThenFinish) Incremental GC in two slices: 1) mark all 2) new marking and finish\n"
    "   10: (IncrementalMultipleSlices) Incremental GC in multiple slices\n"
    "   11: (IncrementalMarkingValidator) Verify incremental marking\n"
    "   12: (ElementsBarrier) Always use the individual element post-write barrier, regardless of elements size\n"
    "   13: (CheckHashTablesOnMinorGC) Check internal hashtables on minor GC\n"
    "   14: (Compact) Perform a shrinking collection every N allocations\n"
    "   15: (CheckHeapAfterGC) Walk the heap to check its integrity after every GC\n"
    "   16: (CheckNursery) Check nursery integrity on minor GC\n";

void
GCRuntime::setZeal(uint8_t zeal, uint32_t frequency)
{
}

void
GCRuntime::setNextScheduled(uint32_t count)
{
}

bool
GCRuntime::parseAndSetZeal(const char* str)
{
    return true;
}

#endif

/*
 * Lifetime in number of major GCs for type sets attached to scripts containing
 * observed types.
 */
static const uint64_t JIT_SCRIPT_RELEASE_TYPES_PERIOD = 20;

bool
GCRuntime::init(uint32_t maxbytes, uint32_t maxNurseryBytes)
{
	if (!rootsHash.init(256))
		return false;

    return true;
}

void
GCRuntime::finish()
{
}

bool
GCRuntime::setParameter(JSGCParamKey key, uint32_t value, AutoLockGC& lock)
{
    return true;
}

uint32_t
GCRuntime::getParameter(JSGCParamKey key, const AutoLockGC& lock)
{
    return 0;
}

bool
GCRuntime::addBlackRootsTracer(JSTraceDataOp traceOp, void* data)
{
    return true;
}

void
GCRuntime::removeBlackRootsTracer(JSTraceDataOp traceOp, void* data)
{
}

void
GCRuntime::setGrayRootsTracer(JSTraceDataOp traceOp, void* data)
{
}

void
GCRuntime::setGCCallback(JSGCCallback callback, void* data)
{
}

void
GCRuntime::setObjectsTenuredCallback(JSObjectsTenuredCallback callback,
                                     void* data)
{
}

namespace {

class AutoNotifyGCActivity {
  public:
    explicit AutoNotifyGCActivity(GCRuntime& gc) : gc_(gc) {
    }
    ~AutoNotifyGCActivity() {
    }

  private:
    GCRuntime& gc_;
};

} // (anon)

bool
GCRuntime::addFinalizeCallback(JSFinalizeCallback callback, void* data)
{
    return true;
}

void
GCRuntime::removeFinalizeCallback(JSFinalizeCallback callback)
{
}

bool
GCRuntime::addWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback, void* data)
{
    return true;
}

void
GCRuntime::removeWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback)
{
}

bool
GCRuntime::addWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback, void* data)
{
    return true;
}

void
GCRuntime::removeWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback)
{
}

bool
GCRuntime::addRoot(Value* vp, const char* name)
{
	return rootsHash.put(vp, name);
}

void
GCRuntime::removeRoot(Value* vp)
{
    rootsHash.remove(vp);
}

extern JS_FRIEND_API(bool)
js::AddRawValueRoot(JSContext* cx, Value* vp, const char* name)
{
    return cx->runtime()->gc.addRoot(vp, name);;
}

extern JS_FRIEND_API(void)
js::RemoveRawValueRoot(JSContext* cx, Value* vp)
{
	cx->runtime()->gc.removeRoot(vp);
}

void
GCRuntime::updateMallocCounter(JS::Zone* zone, size_t nbytes)
{
}

void
GCMarker::delayMarkingArena(/*Arena* arena*/)
{
    /*if (arena->hasDelayedMarking) {
        // Arena already scheduled to be marked later
        return;
    }
    arena->setNextDelayedMarking(unmarkedArenaStackTop);
    unmarkedArenaStackTop = arena;
#ifdef DEBUG
    markLaterArenas++;
#endif*/
}

void
GCMarker::delayMarkingChildren(const void* thing)
{
    /*const TenuredCell* cell = TenuredCell::fromPointer(thing);
    cell->arena()->markOverflow = 1;
    delayMarkingArena(cell->arena());*/
}

AutoDisableCompactingGC::AutoDisableCompactingGC(JSContext* cx)
{
}

AutoDisableCompactingGC::~AutoDisableCompactingGC()
{
}

static const AllocKind AllocKindsToRelocate[] = {
    AllocKind::FUNCTION,
    AllocKind::FUNCTION_EXTENDED,
    AllocKind::OBJECT0,
    AllocKind::OBJECT0_BACKGROUND,
    AllocKind::OBJECT2,
    AllocKind::OBJECT2_BACKGROUND,
    AllocKind::OBJECT4,
    AllocKind::OBJECT4_BACKGROUND,
    AllocKind::OBJECT8,
    AllocKind::OBJECT8_BACKGROUND,
    AllocKind::OBJECT12,
    AllocKind::OBJECT12_BACKGROUND,
    AllocKind::OBJECT16,
    AllocKind::OBJECT16_BACKGROUND,
    AllocKind::SCRIPT,
    AllocKind::LAZY_SCRIPT,
    AllocKind::SCOPE,
    AllocKind::SHAPE,
    AllocKind::ACCESSOR_SHAPE,
    AllocKind::BASE_SHAPE,
    AllocKind::FAT_INLINE_STRING,
    AllocKind::STRING,
    AllocKind::EXTERNAL_STRING
};

#ifdef DEBUG
inline bool
PtrIsInRange(const void* ptr, const void* start, size_t length)
{
    return true;
}
#endif

static inline bool
ShouldProtectRelocatedArenas(JS::gcreason::Reason reason)
{
    return false;
}

template <typename T>
static inline void
UpdateCellPointers(MovingTracer* trc, T* cell)
{
}

#ifndef OMR // Arena
template <typename T>
static void
UpdateArenaPointersTyped(MovingTracer* trc, Arena* arena, JS::TraceKind traceKind)
{
}
#endif // ! OMR Arena

namespace js {
namespace gc {

#ifndef OMR // Arena

struct ArenaListSegment
{
    Arena* begin;
    Arena* end;
};

struct ArenasToUpdate
{
    bool done() { return true; }
    ArenaListSegment getArenasToUpdate(AutoLockHelperThreadState& lock, unsigned maxLength);
};

ArenaListSegment
ArenasToUpdate::getArenasToUpdate(AutoLockHelperThreadState& lock, unsigned maxLength)
{
    return { nullptr, nullptr };
}

struct UpdatePointersTask : public GCParallelTask
{
    // Maximum number of arenas to update in one block.
#ifdef DEBUG
    static const unsigned MaxArenasToProcess = 16;
#else
    static const unsigned MaxArenasToProcess = 256;
#endif

    UpdatePointersTask(JSRuntime* rt, ArenasToUpdate* source, AutoLockHelperThreadState& lock)
    {
    }

    ~UpdatePointersTask() override { join(); }
};

#endif // ! OMR Arena

} // namespace gc
} // namespace js

static const size_t MinCellUpdateBackgroundTasks = 2;
static const size_t MaxCellUpdateBackgroundTasks = 8;

// After cells have been relocated any pointers to a cell's old locations must
// be updated to point to the new location.  This happens by iterating through
// all cells in heap and tracing their children (non-recursively) to update
// them.
//
// This is complicated by the fact that updating a GC thing sometimes depends on
// making use of other GC things.  After a moving GC these things may not be in
// a valid state since they may contain pointers which have not been updated
// yet.
//
// The main dependencies are:
//
//   - Updating a JSObject makes use of its shape
//   - Updating a typed object makes use of its type descriptor object
//
// This means we require at least three phases for update:
//
//  1) shapes
//  2) typed object type descriptor objects
//  3) all other objects
//
// Since we want to minimize the number of phases, we put everything else into
// the first phase and label it the 'misc' phase.

#ifndef OMR // Arena
void
ReleaseArenaList(JSRuntime* rt, Arena* arena, const AutoLockGC& lock)
{
}
#endif // ! OMR Arena

SliceBudget::SliceBudget()
  : timeBudget(UnlimitedTimeBudget), workBudget(UnlimitedWorkBudget)
{
}

SliceBudget::SliceBudget(TimeBudget time)
  : timeBudget(time), workBudget(UnlimitedWorkBudget)
{
}

SliceBudget::SliceBudget(WorkBudget work)
  : timeBudget(UnlimitedTimeBudget), workBudget(work)
{
}

int
SliceBudget::describe(char* buffer, size_t maxlen) const
{
	return snprintf(buffer, maxlen, " ");
}

bool
SliceBudget::checkOverBudget()
{
    return false;
}

void
GCRuntime::maybeGC(Zone* zone)
{
}

unsigned
js::GetCPUCount()
{
    return 1;
}

void
GCHelperState::finish()
{
}

void
GCHelperState::work()
{
}

void
GCRuntime::freeUnusedLifoBlocksAfterSweeping(LifoAlloc* lifo)
{
}

void
GCRuntime::freeAllLifoBlocksAfterSweeping(LifoAlloc* lifo)
{
}

void
GCRuntime::freeAllLifoBlocksAfterMinorGC(LifoAlloc* lifo)
{
}

void
GCHelperState::waitBackgroundSweepEnd()
{
}

struct IsAboutToBeFinalizedFunctor {
    template <typename T> bool operator()(Cell** t) {
        mozilla::DebugOnly<const Cell*> prior = *t;
        bool result = IsAboutToBeFinalizedUnbarriered(reinterpret_cast<T**>(t));
        // Sweep should not have to deal with moved pointers, since moving GC
        // handles updating the UID table manually.
        MOZ_ASSERT(*t == prior);
        return result;
    }
};

#ifdef DEBUG
static const char*
AllocKindToAscii(AllocKind kind)
{
    return "";
}
#endif // DEBUG

#ifdef DEBUG
class CompartmentCheckTracer : public JS::CallbackTracer
{
};

namespace {
struct IsDestComparatorFunctor {
    JS::GCCellPtr dst_;
    explicit IsDestComparatorFunctor(JS::GCCellPtr dst) : dst_(dst) {}

    template <typename T> bool operator()(T* t) { return (*t) == dst_.asCell(); }
};
} // namespace (anonymous)

static bool
InCrossCompartmentMap(JSObject* src, JS::GCCellPtr dst)
{
    return false;
}

struct MaybeCompartmentFunctor {
    template <typename T> JSCompartment* operator()(T* t) { return t->maybeCompartment(); }
};
#endif

#ifdef JS_GC_ZEAL

#ifndef OMR // Chunk
struct GCChunkHasher {
    typedef gc::Chunk* Lookup;

    /*
     * Strip zeros for better distribution after multiplying by the golden
     * ratio.
     */
    static HashNumber hash(gc::Chunk* chunk) {
        MOZ_ASSERT(!(uintptr_t(chunk) & gc::ChunkMask));
        return HashNumber(uintptr_t(chunk) >> gc::ChunkShift);
    }

    static bool match(gc::Chunk* k, gc::Chunk* l) {
        MOZ_ASSERT(!(uintptr_t(k) & gc::ChunkMask));
        MOZ_ASSERT(!(uintptr_t(l) & gc::ChunkMask));
        return k == l;
    }
};
#endif // ! OMR Chunk

class js::gc::MarkingValidator
{
  public:
    explicit MarkingValidator(GCRuntime* gc);
    ~MarkingValidator();
    void nonIncrementalMark(AutoLockForExclusiveAccess& lock);
    void validate();
};

js::gc::MarkingValidator::MarkingValidator(GCRuntime* gc)
{}

js::gc::MarkingValidator::~MarkingValidator()
{
}

void
js::gc::MarkingValidator::nonIncrementalMark(AutoLockForExclusiveAccess& lock)
{
}

void
js::gc::MarkingValidator::validate()
{
}

#endif // JS_GC_ZEAL

/*
 * Group zones that must be swept at the same time.
 *
 * If compartment A has an edge to an unmarked object in compartment B, then we
 * must not sweep A in a later slice than we sweep B. That's because a write
 * barrier in A that could lead to the unmarked object in B becoming
 * marked. However, if we had already swept that object, we would be in trouble.
 *
 * If we consider these dependencies as a graph, then all the compartments in
 * any strongly-connected component of this graph must be swept in the same
 * slice.
 *
 * Tarjan's algorithm is used to calculate the components.
 */
namespace {
struct AddOutgoingEdgeFunctor {
    bool needsEdge_;
    ZoneComponentFinder& finder_;

    AddOutgoingEdgeFunctor(bool needsEdge, ZoneComponentFinder& finder)
      : needsEdge_(needsEdge), finder_(finder)
    {}

    template <typename T>
    void operator()(T tp) {
    }
};
} // namespace (anonymous)

void
JSCompartment::findOutgoingEdges(ZoneComponentFinder& finder)
{
}

/*
 * Gray marking:
 *
 * At the end of collection, anything reachable from a gray root that has not
 * otherwise been marked black must be marked gray.
 *
 * This means that when marking things gray we must not allow marking to leave
 * the current compartment group, as that could result in things being marked
 * grey when they might subsequently be marked black.  To achieve this, when we
 * find a cross compartment pointer we don't mark the referent but add it to a
 * singly-linked list of incoming gray pointers that is stored with each
 * compartment.
 *
 * The list head is stored in JSCompartment::gcIncomingGrayPointers and contains
 * cross compartment wrapper objects. The next pointer is stored in the second
 * extra slot of the cross compartment wrapper.
 *
 * The list is created during gray marking when one of the
 * MarkCrossCompartmentXXX functions is called for a pointer that leaves the
 * current compartent group.  This calls DelayCrossCompartmentGrayMarking to
 * push the referring object onto the list.
 *
 * The list is traversed and then unlinked in
 * MarkIncomingCrossCompartmentPointers.
 */

/* static */ unsigned
ProxyObject::grayLinkExtraSlot(JSObject* obj)
{
    return 1;
}

#ifdef DEBUG
static void
AssertNotOnGrayList(JSObject* obj)
{
}
#endif

void
js::NotifyGCNukeWrapper(JSObject* obj)
{
}

enum {
    JS_GC_SWAP_OBJECT_A_REMOVED = 1 << 0,
    JS_GC_SWAP_OBJECT_B_REMOVED = 1 << 1
};

unsigned
js::NotifyGCPreSwap(JSObject* a, JSObject* b)
{
    return 0;
}

void
js::NotifyGCPostSwap(JSObject* a, JSObject* b, unsigned removedFlags)
{
}

class GCSweepTask : public GCParallelTask
{
    virtual void runFromHelperThread(AutoLockHelperThreadState& locked) override {
        AutoSetThreadIsSweeping threadIsSweeping;
        GCParallelTask::runFromHelperThread(locked);
    }
    GCSweepTask(const GCSweepTask&) = delete;

  protected:
    JSRuntime* runtime;

  public:
    explicit GCSweepTask(JSRuntime* rt) : runtime(rt) {}
    GCSweepTask(GCSweepTask&& other)
      : GCParallelTask(mozilla::Move(other)),
        runtime(other.runtime)
    {}
};

// Causes the given WeakCache to be swept when run.
class SweepWeakCacheTask : public GCSweepTask
{
    JS::WeakCache<void*>& cache;

    SweepWeakCacheTask(const SweepWeakCacheTask&) = delete;

  public:
    SweepWeakCacheTask(JSRuntime* rt, JS::WeakCache<void*>& wc) : GCSweepTask(rt), cache(wc) {}
    SweepWeakCacheTask(SweepWeakCacheTask&& other)
      : GCSweepTask(mozilla::Move(other)), cache(other.cache)
    {}

    void run() override {
        cache.sweep();
    }
};

#define MAKE_GC_SWEEP_TASK(name)                                              \
    class name : public GCSweepTask {                                         \
        void run() override;                                                  \
      public:                                                                 \
        explicit name (JSRuntime* rt) : GCSweepTask(rt) {}                    \
    }
MAKE_GC_SWEEP_TASK(SweepAtomsTask);
MAKE_GC_SWEEP_TASK(SweepCCWrappersTask);
MAKE_GC_SWEEP_TASK(SweepBaseShapesTask);
MAKE_GC_SWEEP_TASK(SweepInitialShapesTask);
MAKE_GC_SWEEP_TASK(SweepObjectGroupsTask);
MAKE_GC_SWEEP_TASK(SweepRegExpsTask);
MAKE_GC_SWEEP_TASK(SweepMiscTask);
#undef MAKE_GC_SWEEP_TASK

/* virtual */ void
SweepAtomsTask::run()
{
}

/* virtual */ void
SweepCCWrappersTask::run()
{
}

/* virtual */ void
SweepObjectGroupsTask::run()
{
}

/* virtual */ void
SweepRegExpsTask::run()
{
}

/* virtual */ void
SweepMiscTask::run()
{
}

using WeakCacheTaskVector = mozilla::Vector<SweepWeakCacheTask, 0, SystemAllocPolicy>;

#ifndef OMR // ArenaList
template <typename T, typename... Args>
static bool
SweepArenaList(Arena** arenasToSweep, SliceBudget& sliceBudget, Args... args)
{
    return true;
}
#endif // ! OMR ArenaList

namespace {

class AutoGCSlice {
  public:
    explicit AutoGCSlice(JSRuntime* rt);
    ~AutoGCSlice();
};

} /* anonymous namespace */

AutoGCSlice::AutoGCSlice(JSRuntime* rt)
{
}

AutoGCSlice::~AutoGCSlice()
{
}

namespace {

class AutoScheduleZonesForGC
{
  public:
    explicit AutoScheduleZonesForGC(JSRuntime* rt) {
    }

    ~AutoScheduleZonesForGC() {
    }
};

/*
 * An invariant of our GC/CC interaction is that there must not ever be any
 * black to gray edges in the system. It is possible to violate this with
 * simple compartmental GC. For example, in GC[n], we collect in both
 * compartmentA and compartmentB, and mark both sides of the cross-compartment
 * edge gray. Later in GC[n+1], we only collect compartmentA, but this time
 * mark it black. Now we are violating the invariants and must fix it somehow.
 *
 * To prevent this situation, we explicitly detect the black->gray state when
 * marking cross-compartment edges -- see ShouldMarkCrossCompartment -- adding
 * each violating edges to foundBlackGrayEdges. After we leave the trace
 * session for each GC slice, we "ExposeToActiveJS" on each of these edges
 * (which we cannot do safely from the guts of the GC).
 */
class AutoExposeLiveCrossZoneEdges
{
  public:
    explicit AutoExposeLiveCrossZoneEdges(BlackGrayEdgeVector* edgesPtr) {
    }
    ~AutoExposeLiveCrossZoneEdges() {
    }
};

} /* anonymous namespace */

#ifdef JS_GC_ZEAL
static bool
IsDeterministicGCReason(JS::gcreason::Reason reason)
{
    return true;
}
#endif

js::AutoEnqueuePendingParseTasksAfterGC::~AutoEnqueuePendingParseTasksAfterGC()
{
}

void
GCRuntime::abortGC()
{
}

void
GCRuntime::notifyDidPaint()
{
}

void
GCRuntime::startDebugGC(JSGCInvocationKind gckind, SliceBudget& budget)
{
}

void
GCRuntime::debugGCSlice(SliceBudget& budget)
{
}

/* Schedule a full GC unless a zone will already be collected. */
void
js::PrepareForDebugGC(JSRuntime* rt)
{
}

void
GCRuntime::onOutOfMallocMemory()
{
}

void
GCRuntime::minorGC(JS::gcreason::Reason reason, gcstats::Phase phase)
{
}

bool
GCRuntime::gcIfRequested()
{
    return false;
}

void
js::gc::FinishGC(JSContext* cx)
{
}

AutoPrepareForTracing::AutoPrepareForTracing(JSContext* cx, ZoneSelector selector)
{
    session_.emplace(cx);
}

JSCompartment*
js::NewCompartment(JSContext* cx, Zone* zone, JSPrincipals* principals,
                   const JS::CompartmentOptions& options)
{
	if (!zone) {
		JSRuntime* rt = cx->runtime();
		zone = *rt->gc.zones.begin();
#ifdef OMR
        // OMRTODO: Use multiple zones from a context correctly.
        OmrGcHelper::zone = zone;
#endif
	}
	ScopedJSDeletePtr<JSCompartment> compartment(cx->new_<JSCompartment>(zone, options));
	compartment->init(cx);
	return compartment.forget();
}

void
gc::MergeCompartments(JSCompartment* source, JSCompartment* target)
{
}

void
GCRuntime::setFullCompartmentChecks(bool enabled)
{
}

#ifdef JS_GC_ZEAL
bool
GCRuntime::selectForMarking(JSObject* object)
{
    return true;
}

void
GCRuntime::setDeterministic(bool enabled)
{
}
#endif

#ifdef DEBUG

/* Should only be called manually under gdb */
void PreventGCDuringInteractiveDebug()
{
}

#endif

void
js::ReleaseAllJITCode(FreeOp* fop)
{
}


AutoSuppressGC::AutoSuppressGC(ExclusiveContext* cx)
{
}

AutoSuppressGC::AutoSuppressGC(JSCompartment* comp)
{
}

AutoSuppressGC::AutoSuppressGC(JSContext* cx)
{
}

#ifdef DEBUG
AutoDisableProxyCheck::AutoDisableProxyCheck(JSRuntime* rt)
{
}

AutoDisableProxyCheck::~AutoDisableProxyCheck()
{
}

JS_FRIEND_API(void)
JS::AssertGCThingMustBeTenured(JSObject* obj)
{
}

JS_FRIEND_API(void)
JS::AssertGCThingIsNotAnObjectSubclass(Cell* cell)
{
}

JS_FRIEND_API(void)
js::gc::AssertGCThingHasType(js::gc::Cell* cell, JS::TraceKind kind)
{
}

JS_PUBLIC_API(size_t)
JS::GetGCNumber()
{
    return 0;
}
#endif

#ifdef DEBUG
JS::AutoAssertOnGC::AutoAssertOnGC()
{
}

JS::AutoAssertOnGC::AutoAssertOnGC(JSContext* cx)
{
}

JS::AutoAssertOnGC::~AutoAssertOnGC()
{
}

/* static */ void
JS::AutoAssertOnGC::VerifyIsSafeToGC(JSRuntime* rt)
{
}

JS::AutoAssertNoAlloc::AutoAssertNoAlloc(JSContext* cx)
{
}

void JS::AutoAssertNoAlloc::disallowAlloc(JSRuntime* rt)
{
}

JS::AutoAssertNoAlloc::~AutoAssertNoAlloc()
{
}

AutoAssertNoNurseryAlloc::AutoAssertNoNurseryAlloc(JSRuntime* rt)
{
}

AutoAssertNoNurseryAlloc::~AutoAssertNoNurseryAlloc()
{
}

JS::AutoEnterCycleCollection::AutoEnterCycleCollection(JSContext* cx)
{
}

JS::AutoEnterCycleCollection::~AutoEnterCycleCollection()
{
}
#endif

JS::AutoAssertGCCallback::AutoAssertGCCallback(JSObject* obj)
{
}

JS_FRIEND_API(const char*)
JS::GCTraceKindToAscii(JS::TraceKind kind)
{
    return "";
}

JS::GCCellPtr::GCCellPtr(const Value& v)
{
}

JS::TraceKind
JS::GCCellPtr::outOfLineKind() const
{
    return (JS::TraceKind)0;
}

bool
JS::GCCellPtr::mayBeOwnedByOtherRuntime() const
{
    return false;
}

JS_PUBLIC_API(void)
JS::PrepareZoneForGC(Zone* zone)
{
}

JS_PUBLIC_API(void)
JS::PrepareForFullGC(JSContext* cx)
{
}

JS_PUBLIC_API(void)
JS::PrepareForIncrementalGC(JSContext* cx)
{
}

JS_PUBLIC_API(bool)
JS::IsGCScheduled(JSContext* cx)
{
    return false;
}

JS_PUBLIC_API(void)
JS::SkipZoneForGC(Zone* zone)
{
}

JS_PUBLIC_API(void)
JS::GCForReason(JSContext* cx, JSGCInvocationKind gckind, gcreason::Reason reason)
{
}

JS_PUBLIC_API(void)
JS::StartIncrementalGC(JSContext* cx, JSGCInvocationKind gckind, gcreason::Reason reason, int64_t millis)
{
}

JS_PUBLIC_API(void)
JS::IncrementalGCSlice(JSContext* cx, gcreason::Reason reason, int64_t millis)
{
}

JS_PUBLIC_API(void)
JS::FinishIncrementalGC(JSContext* cx, gcreason::Reason reason)
{
}

JS_PUBLIC_API(void)
JS::AbortIncrementalGC(JSContext* cx)
{
}

char16_t*
JS::GCDescription::formatSliceMessage(JSContext* cx) const
{
    return nullptr;
}

char16_t*
JS::GCDescription::formatSummaryMessage(JSContext* cx) const
{
    return nullptr;
}

JS::dbg::GarbageCollectionEvent::Ptr
JS::GCDescription::toGCEvent(JSContext* cx) const
{
    return JS::dbg::GarbageCollectionEvent::Create(cx, cx->gc.stats, cx->gc.majorGCCount());
}

char16_t*
JS::GCDescription::formatJSON(JSContext* cx, uint64_t timestamp) const
{
    return nullptr;
}

JS_PUBLIC_API(JS::GCSliceCallback)
JS::SetGCSliceCallback(JSContext* cx, GCSliceCallback callback)
{
    return nullptr;
}

JS_PUBLIC_API(JS::DoCycleCollectionCallback)
JS::SetDoCycleCollectionCallback(JSContext* cx, JS::DoCycleCollectionCallback callback)
{
    return nullptr;
}

JS_PUBLIC_API(JS::GCNurseryCollectionCallback)
JS::SetGCNurseryCollectionCallback(JSContext* cx, GCNurseryCollectionCallback callback)
{
    return nullptr;
}

JS_PUBLIC_API(void)
JS::DisableIncrementalGC(JSContext* cx)
{
}

JS_PUBLIC_API(bool)
JS::IsIncrementalGCEnabled(JSContext* cx)
{
    return false;
}

JS_PUBLIC_API(bool)
JS::IsIncrementalGCInProgress(JSContext* cx)
{
    return false;
}

JS_PUBLIC_API(bool)
JS::IsIncrementalBarrierNeeded(JSContext* cx)
{
    return false;
}

struct IncrementalReferenceBarrierFunctor {
    template <typename T> void operator()(T* t) { T::writeBarrierPre(t); }
};

JS_PUBLIC_API(void)
JS::IncrementalReferenceBarrier(GCCellPtr thing)
{
}

JS_PUBLIC_API(void)
JS::IncrementalValueBarrier(const Value& v)
{
}

JS_PUBLIC_API(void)
JS::IncrementalObjectBarrier(JSObject* obj)
{
}

JS_PUBLIC_API(bool)
JS::WasIncrementalGC(JSContext* cx)
{
    return false;
}

JS::AutoDisableGenerationalGC::AutoDisableGenerationalGC(JSRuntime* rt)
{
}

JS::AutoDisableGenerationalGC::~AutoDisableGenerationalGC()
{
}

JS_PUBLIC_API(bool)
JS::IsGenerationalGCEnabled(JSRuntime* rt)
{
	return false;
}

namespace js {
namespace gc {
namespace MemInfo {

#ifdef JS_MORE_DETERMINISTIC
static bool
DummyGetter(JSContext* cx, unsigned argc, Value* vp)
{
    return true;
}
#endif

} /* namespace MemInfo */

JSObject*
NewMemoryInfoObject(JSContext* cx)
{
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    if (!obj)
        return nullptr;

#ifndef OMR
    using namespace MemInfo;
    struct NamedGetter {
        const char* name;
        JSNative getter;
    } getters[] = {
        { "gcBytes", GCBytesGetter },
        { "gcMaxBytes", GCMaxBytesGetter },
        { "mallocBytesRemaining", MallocBytesGetter },
        { "maxMalloc", MaxMallocGetter },
        { "gcIsHighFrequencyMode", GCHighFreqGetter },
        { "gcNumber", GCNumberGetter },
        { "majorGCCount", MajorGCCountGetter },
        { "minorGCCount", MinorGCCountGetter }
    };

    for (auto pair : getters) {
#ifdef JS_MORE_DETERMINISTIC
        JSNative getter = DummyGetter;
#else
        JSNative getter = pair.getter;
#endif
        if (!JS_DefineProperty(cx, obj, pair.name, UndefinedHandleValue,
                               JSPROP_ENUMERATE | JSPROP_SHARED,
                               getter, nullptr))
        {
            return nullptr;
        }
    }
    RootedObject zoneObj(cx, JS_NewObject(cx, nullptr));
    if (!zoneObj)
        return nullptr;

    if (!JS_DefineProperty(cx, obj, "zone", zoneObj, JSPROP_ENUMERATE))
        return nullptr;

    struct NamedZoneGetter {
        const char* name;
        JSNative getter;
    } zoneGetters[] = {
        { "gcBytes", ZoneGCBytesGetter },
        { "gcTriggerBytes", ZoneGCTriggerBytesGetter },
        { "gcAllocTrigger", ZoneGCAllocTriggerGetter },
        { "mallocBytesRemaining", ZoneMallocBytesGetter },
        { "maxMalloc", ZoneMaxMallocGetter },
        { "delayBytes", ZoneGCDelayBytesGetter },
        { "heapGrowthFactor", ZoneGCHeapGrowthFactorGetter },
        { "gcNumber", ZoneGCNumberGetter }
    };

    for (auto pair : zoneGetters) {
#ifdef JS_MORE_DETERMINISTIC
        JSNative getter = DummyGetter;
#else
        JSNative getter = pair.getter;
#endif
        if (!JS_DefineProperty(cx, zoneObj, pair.name, UndefinedHandleValue,
                               JSPROP_ENUMERATE | JSPROP_SHARED,
                               getter, nullptr))
        {
            return nullptr;
        }
    }
#endif // ! OMR
	return obj;
}

const char*
StateName(State state)
{
    return "";
}

void
AutoAssertHeapBusy::checkCondition(JSRuntime *rt)
{
}

void
AutoAssertEmptyNursery::checkCondition(JSRuntime *rt) {
}

AutoEmptyNursery::AutoEmptyNursery(JSRuntime *rt)
{
}

// OMR GC Helper
#ifdef OMR
Zone* OmrGcHelper::zone;
GCRuntime* OmrGcHelper::runtime;
#endif // OMR

} /* namespace gc */
} /* namespace js */

#ifdef DEBUG
void
js::gc::Cell::dump(FILE* fp) const
{
}

// For use in a debugger.
void
js::gc::Cell::dump() const
{
}
#endif
