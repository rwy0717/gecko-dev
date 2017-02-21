/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Garbage Collector. */

#ifndef jsgc_h
#define jsgc_h

#include "mozilla/Atomics.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TypeTraits.h"

#include "js/GCAPI.h"
#include "js/SliceBudget.h"
#include "js/Vector.h"
#include "threading/ConditionVariable.h"
#include "threading/Thread.h"
#include "vm/NativeObject.h"

namespace js {

class AutoLockHelperThreadState;
unsigned GetCPUCount();

namespace gcstats {
struct Statistics;
} // namespace gcstats

class Nursery;

namespace gc {

struct FinalizePhase;

#define GCSTATES(D) \
    D(NotActive) \
    D(MarkRoots) \
    D(Mark) \
    D(Sweep) \
    D(Finalize) \
    D(Compact) \
    D(Decommit)
enum class State {
#define MAKE_STATE(name) name,
    GCSTATES(MAKE_STATE)
#undef MAKE_STATE
};

/*
 * Map from C++ type to alloc kind for non-object types. JSObject does not have
 * a 1:1 mapping, so must use Arena::thingSize.
 *
 * The AllocKind is available as MapTypeToFinalizeKind<SomeType>::kind.
 */
template <typename T> struct MapTypeToFinalizeKind {};
#define EXPAND_MAPTYPETOFINALIZEKIND(allocKind, traceKind, type, sizedType) \
    template <> struct MapTypeToFinalizeKind<type> { \
        static const AllocKind kind = AllocKind::allocKind; \
};
FOR_EACH_NONOBJECT_ALLOCKIND(EXPAND_MAPTYPETOFINALIZEKIND)
#undef EXPAND_MAPTYPETOFINALIZEKIND

template <typename T> struct ParticipatesInCC {};
#define EXPAND_PARTICIPATES_IN_CC(_, type, addToCCKind) \
    template <> struct ParticipatesInCC<type> { static const bool value = addToCCKind; };
JS_FOR_EACH_TRACEKIND(EXPAND_PARTICIPATES_IN_CC)
#undef EXPAND_PARTICIPATES_IN_CC

static inline bool
IsNurseryAllocable(AllocKind kind)
{
    return false;
}

static inline bool
IsBackgroundFinalized(AllocKind kind)
{
    static const bool map[] = {
        true,      /* AllocKind::FUNCTION */
        true,      /* AllocKind::FUNCTION_EXTENDED */
        false,     /* AllocKind::OBJECT0 */
        true,      /* AllocKind::OBJECT0_BACKGROUND */
        false,     /* AllocKind::OBJECT2 */
        true,      /* AllocKind::OBJECT2_BACKGROUND */
        false,     /* AllocKind::OBJECT4 */
        true,      /* AllocKind::OBJECT4_BACKGROUND */
        false,     /* AllocKind::OBJECT8 */
        true,      /* AllocKind::OBJECT8_BACKGROUND */
        false,     /* AllocKind::OBJECT12 */
        true,      /* AllocKind::OBJECT12_BACKGROUND */
        false,     /* AllocKind::OBJECT16 */
        true,      /* AllocKind::OBJECT16_BACKGROUND */
        false,     /* AllocKind::SCRIPT */
        true,      /* AllocKind::LAZY_SCRIPT */
        true,      /* AllocKind::SHAPE */
        true,      /* AllocKind::ACCESSOR_SHAPE */
        true,      /* AllocKind::BASE_SHAPE */
        true,      /* AllocKind::OBJECT_GROUP */
        true,      /* AllocKind::FAT_INLINE_STRING */
        true,      /* AllocKind::STRING */
        false,     /* AllocKind::EXTERNAL_STRING */
        true,      /* AllocKind::SYMBOL */
        false,     /* AllocKind::JITCODE */
        true,      /* AllocKind::SCOPE */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT));
    return map[size_t(kind)];
}

static inline bool
CanBeFinalizedInBackground(AllocKind kind, const Class* clasp)
{
    MOZ_ASSERT(IsObjectAllocKind(kind));
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the alloc kind. For example,
     * AllocKind::OBJECT0 calls the finalizer on the main thread,
     * AllocKind::OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * IsBackgroundFinalized is called to prevent recursively incrementing
     * the alloc kind; kind may already be a background finalize kind.
     */
    return (!IsBackgroundFinalized(kind) &&
            (!clasp->hasFinalize() || (clasp->flags & JSCLASS_BACKGROUND_FINALIZE)));
}

/* Capacity for slotsToThingKind */
const size_t SLOTS_TO_THING_KIND_LIMIT = 17;

extern const AllocKind slotsToThingKind[];

/* Get the best kind to use when making an object with the given slot count. */
static inline AllocKind
GetGCObjectKind(size_t numSlots)
{
    if (numSlots >= SLOTS_TO_THING_KIND_LIMIT)
        return AllocKind::OBJECT16;
    return slotsToThingKind[numSlots];
}

/* As for GetGCObjectKind, but for dense array allocation. */
static inline AllocKind
GetGCArrayKind(size_t numElements)
{
    /*
     * Dense arrays can use their fixed slots to hold their elements array
     * (less two Values worth of ObjectElements header), but if more than the
     * maximum number of fixed slots is needed then the fixed slots will be
     * unused.
     */
    JS_STATIC_ASSERT(ObjectElements::VALUES_PER_HEADER == 2);
    if (numElements > NativeObject::MAX_DENSE_ELEMENTS_COUNT ||
        numElements + ObjectElements::VALUES_PER_HEADER >= SLOTS_TO_THING_KIND_LIMIT)
    {
        return AllocKind::OBJECT2;
    }
    return slotsToThingKind[numElements + ObjectElements::VALUES_PER_HEADER];
}

static inline AllocKind
GetGCObjectFixedSlotsKind(size_t numFixedSlots)
{
    MOZ_ASSERT(numFixedSlots < SLOTS_TO_THING_KIND_LIMIT);
    return slotsToThingKind[numFixedSlots];
}

// Get the best kind to use when allocating an object that needs a specific
// number of bytes.
static inline AllocKind
GetGCObjectKindForBytes(size_t nbytes)
{
    MOZ_ASSERT(nbytes <= JSObject::MAX_BYTE_SIZE);

    if (nbytes <= sizeof(NativeObject))
        return AllocKind::OBJECT0;
    nbytes -= sizeof(NativeObject);

    size_t dataSlots = AlignBytes(nbytes, sizeof(Value)) / sizeof(Value);
    MOZ_ASSERT(nbytes <= dataSlots * sizeof(Value));
    return GetGCObjectKind(dataSlots);
}

static inline AllocKind
GetBackgroundAllocKind(AllocKind kind)
{
    MOZ_ASSERT(!IsBackgroundFinalized(kind));
    MOZ_ASSERT(IsObjectAllocKind(kind));
    return AllocKind(size_t(kind) + 1);
}

/* Get the number of fixed slots and initial capacity associated with a kind. */
static inline size_t
GetGCKindSlots(AllocKind thingKind)
{
    /* Using a switch in hopes that thingKind will usually be a compile-time constant. */
    switch (thingKind) {
      case AllocKind::FUNCTION:
      case AllocKind::OBJECT0:
      case AllocKind::OBJECT0_BACKGROUND:
        return 0;
      case AllocKind::FUNCTION_EXTENDED:
      case AllocKind::OBJECT2:
      case AllocKind::OBJECT2_BACKGROUND:
        return 2;
      case AllocKind::OBJECT4:
      case AllocKind::OBJECT4_BACKGROUND:
        return 4;
      case AllocKind::OBJECT8:
      case AllocKind::OBJECT8_BACKGROUND:
        return 8;
      case AllocKind::OBJECT12:
      case AllocKind::OBJECT12_BACKGROUND:
        return 12;
      case AllocKind::OBJECT16:
      case AllocKind::OBJECT16_BACKGROUND:
        return 16;
      default:
        MOZ_CRASH("Bad object alloc kind");
    }
}

static inline size_t
GetGCKindSlots(AllocKind thingKind, const Class* clasp)
{
    size_t nslots = GetGCKindSlots(thingKind);

    /* An object's private data uses the space taken by its last fixed slot. */
    if (clasp->flags & JSCLASS_HAS_PRIVATE) {
        MOZ_ASSERT(nslots > 0);
        nslots--;
    }

    /*
     * Functions have a larger alloc kind than AllocKind::OBJECT to reserve
     * space for the extra fields in JSFunction, but have no fixed slots.
     */
    if (clasp == FunctionClassPtr)
        nslots = 0;

    return nslots;
}

static inline size_t
GetGCKindBytes(AllocKind thingKind)
{
    return sizeof(JSObject_Slots0) + GetGCKindSlots(thingKind) * sizeof(Value);
}

#ifndef OMR // Arenas and segments

// Class to assist in triggering background chunk allocation. This cannot be done
// while holding the GC or worker thread state lock due to lock ordering issues.
// As a result, the triggering is delayed using this class until neither of the
// above locks is held.
class AutoMaybeStartBackgroundAllocation;

/*
 * A single segment of a SortedArenaList. Each segment has a head and a tail,
 * which track the start and end of a segment for O(1) append and concatenation.
 */
struct SortedArenaListSegment
{
    Arena* head;
    Arena** tailp;

    void clear() {
    }

    bool isEmpty() const {
        return true;
    }

    // Appends |arena| to this segment.
    void append(Arena* arena) {
    }

    // Points the tail of this segment at |arena|, which may be null. Note
    // that this does not change the tail itself, but merely which arena
    // follows it. This essentially turns the tail into a cursor (see also the
    // description of ArenaList), but from the perspective of a SortedArenaList
    // this makes no difference.
    void linkTo(Arena* arena) {
    }
};

/*
 * A class that holds arenas in sorted order by appending arenas to specific
 * segments. Each segment has a head and a tail, which can be linked up to
 * other segments to create a contiguous ArenaList.
 */
class SortedArenaList
{
  public:
    explicit SortedArenaList(size_t thingsPerArena = 0) {
    }
};

class ArenaLists
{
  public:
    explicit ArenaLists(JSRuntime* rt) {
    }
	
	const void* addressOfFreeList(AllocKind thingKind) const {
		return nullptr;
	}
};

#endif // ! OMR Arenas and segments

} /* namespace gc */

extern void
TraceRuntime(JSTracer* trc);

extern void
ReleaseAllJITCode(FreeOp* op);

extern void
PrepareForDebugGC(JSRuntime* rt);

/* Functions for managing cross compartment gray pointers. */

extern void
NotifyGCNukeWrapper(JSObject* o);

extern unsigned
NotifyGCPreSwap(JSObject* a, JSObject* b);

extern void
NotifyGCPostSwap(JSObject* a, JSObject* b, unsigned preResult);

/*
 * Helper state for use when JS helper threads sweep and allocate GC thing kinds
 * that can be swept and allocated off the main thread.
 *
 * In non-threadsafe builds, all actual sweeping and allocation is performed
 * on the main thread, but GCHelperState encapsulates this from clients as
 * much as possible.
 */
class GCHelperState
{
  public:
    explicit GCHelperState(JSRuntime* rt)
    { }

    void finish();

    void work();

    /* Must be called without the GC lock taken. */
    void waitBackgroundSweepEnd();
};

// A generic task used to dispatch work to the helper thread system.
// Users should derive from GCParallelTask add what data they need and
// override |run|.
class GCParallelTask
{
	// The state of the parallel computation.
	enum TaskState {
		NotStarted,
		Dispatched,
		Finished,
	} state;

	// Amount of time this task took to execute.
	uint64_t duration_;

  protected:
    // A flag to signal a request for early completion of the off-thread task.
    mozilla::Atomic<bool> cancel_;

    virtual void run() = 0;

  public:
    GCParallelTask() : state(NotStarted), duration_(0) {}
    GCParallelTask(GCParallelTask&& other) : state(NotStarted), duration_(0) {}

    // Derived classes must override this to ensure that join() gets called
    // before members get destructed.
    virtual ~GCParallelTask();

    // Time spent in the most recent invocation of this task.
    int64_t duration() const { return 0; }

    // The simple interface to a parallel task works exactly like pthreads.
    bool start();
    void join();

    // If multiple tasks are to be started or joined at once, it is more
    // efficient to take the helper thread lock once and use these methods.
    bool startWithLockHeld(AutoLockHelperThreadState& locked);
    void joinWithLockHeld(AutoLockHelperThreadState& locked);

    // Instead of dispatching to a helper, run the task on the main thread.
    void runFromMainThread(JSRuntime* rt);

    // Dispatch a cancelation request.
    enum CancelMode { CancelNoWait, CancelAndWait};
    void cancel(CancelMode mode = CancelNoWait) {
    }

    // Check if a task is actively running.
    bool isRunningWithLockHeld(const AutoLockHelperThreadState& locked) const;
    bool isRunning() const;

    // This should be friended to HelperThread, but cannot be because it
    // would introduce several circular dependencies.
  public:
    virtual void runFromHelperThread(AutoLockHelperThreadState& locked);
};

#ifndef OMR // GC Iterators

typedef void (*IterateChunkCallback)(JSRuntime* rt, void* data, gc::Chunk* chunk);
typedef void (*IterateZoneCallback)(JSRuntime* rt, void* data, JS::Zone* zone);
typedef void (*IterateArenaCallback)(JSRuntime* rt, void* data, gc::Arena* arena,
                                     JS::TraceKind traceKind, size_t thingSize);
typedef void (*IterateCellCallback)(JSRuntime* rt, void* data, void* thing,
                                    JS::TraceKind traceKind, size_t thingSize);

/*
 * This function calls |zoneCallback| on every zone, |compartmentCallback| on
 * every compartment, |arenaCallback| on every in-use arena, and |cellCallback|
 * on every in-use cell in the GC heap.
 */
extern void
IterateZonesCompartmentsArenasCells(JSContext* cx, void* data,
                                    IterateZoneCallback zoneCallback,
                                    JSIterateCompartmentCallback compartmentCallback,
                                    IterateArenaCallback arenaCallback,
                                    IterateCellCallback cellCallback);

/*
 * This function is like IterateZonesCompartmentsArenasCells, but does it for a
 * single zone.
 */
extern void
IterateZoneCompartmentsArenasCells(JSContext* cx, Zone* zone, void* data,
                                   IterateZoneCallback zoneCallback,
                                   JSIterateCompartmentCallback compartmentCallback,
                                   IterateArenaCallback arenaCallback,
                                   IterateCellCallback cellCallback);

/*
 * Invoke chunkCallback on every in-use chunk.
 */
extern void
IterateChunks(JSContext* cx, void* data, IterateChunkCallback chunkCallback);

#endif // ! OMR GC Iterators

typedef void (*IterateScriptCallback)(JSRuntime* rt, void* data, JSScript* script);

/*
 * Invoke scriptCallback on every in-use script for
 * the given compartment or for all compartments if it is null.
 */
extern void
IterateScripts(JSContext* cx, JSCompartment* compartment,
               void* data, IterateScriptCallback scriptCallback);

extern void
FinalizeStringRT(JSRuntime* rt, JSString* str);

JSCompartment*
NewCompartment(JSContext* cx, JS::Zone* zone, JSPrincipals* principals,
               const JS::CompartmentOptions& options);

namespace gc {

/*
 * Merge all contents of source into target. This can only be used if source is
 * the only compartment in its zone.
 */
void
MergeCompartments(JSCompartment* source, JSCompartment* target);

/*
 * This structure overlays a Cell in the Nursery and re-purposes its memory
 * for managing the Nursery collection process.
 */
class RelocationOverlay
{
};

// Functions for checking and updating GC thing pointers that might have been
// moved by compacting GC. Overloads are also provided that work with Values.
//
// IsForwarded    - check whether a pointer refers to an GC thing that has been
//                  moved.
//
// Forwarded      - return a pointer to the new location of a GC thing given a
//                  pointer to old location.
//
// MaybeForwarded - used before dereferencing a pointer that may refer to a
//                  moved GC thing without updating it. For JSObjects this will
//                  also update the object's shape pointer if it has been moved
//                  to allow slots to be accessed.

template <typename T>
struct MightBeForwarded
{
    static_assert(mozilla::IsBaseOf<Cell, T>::value,
                  "T must derive from Cell");
    static_assert(!mozilla::IsSame<Cell, T>::value && !mozilla::IsSame<TenuredCell, T>::value,
                  "T must not be Cell or TenuredCell");

    static const bool value = mozilla::IsBaseOf<JSObject, T>::value ||
                              mozilla::IsBaseOf<Shape, T>::value ||
                              mozilla::IsBaseOf<BaseShape, T>::value ||
                              mozilla::IsBaseOf<JSString, T>::value ||
                              mozilla::IsBaseOf<JSScript, T>::value ||
                              mozilla::IsBaseOf<js::LazyScript, T>::value ||
                              mozilla::IsBaseOf<js::Scope, T>::value;
};

template <typename T>
inline bool
IsForwarded(T* t)
{
    return false;
}

struct IsForwardedFunctor : public BoolDefaultAdaptor<Value, false> {
    template <typename T> bool operator()(T* t) { return IsForwarded(t); }
};

inline bool
IsForwarded(const JS::Value& value)
{
    return false;
}

template <typename T>
inline T*
Forwarded(T* t)
{
    return nullptr;
}

struct ForwardedFunctor : public IdentityDefaultAdaptor<Value> {
    template <typename T> inline Value operator()(T* t) {
        return Value();
    }
};

inline Value
Forwarded(const JS::Value& value)
{
    return Value();
}

template <typename T>
inline T
MaybeForwarded(T t)
{
    return t;
}

#ifdef JSGC_HASH_TABLE_CHECKS

template <typename T>
inline void
CheckGCThingAfterMovingGC(T* t)
{
}

template <typename T>
inline void
CheckGCThingAfterMovingGC(const ReadBarriered<T*>& t)
{
}

struct CheckValueAfterMovingGCFunctor : public VoidDefaultAdaptor<Value> {
    template <typename T> void operator()(T* t) { CheckGCThingAfterMovingGC(t); }
};

#endif // JSGC_HASH_TABLE_CHECKS

#define JS_FOR_EACH_ZEAL_MODE(D)               \
            D(Poke, 1)                         \
            D(Alloc, 2)                        \
            D(FrameGC, 3)                      \
            D(VerifierPre, 4)                  \
            D(FrameVerifierPre, 5)             \
            D(StackRooting, 6)                 \
            D(GenerationalGC, 7)               \
            D(IncrementalRootsThenFinish, 8)   \
            D(IncrementalMarkAllThenFinish, 9) \
            D(IncrementalMultipleSlices, 10)   \
            D(IncrementalMarkingValidator, 11) \
            D(ElementsBarrier, 12)             \
            D(CheckHashTablesOnMinorGC, 13)    \
            D(Compact, 14)                     \
            D(CheckHeapAfterGC, 15)            \
            D(CheckNursery, 16)

enum class ZealMode {
#define ZEAL_MODE(name, value) name = value,
    JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#undef ZEAL_MODE
    Limit = 16
};

enum VerifierType {
    PreBarrierVerifier
};

#ifdef JS_GC_ZEAL

extern const char* ZealModeHelpText;

/* Check that write barriers have been used correctly. See jsgc.cpp. */
static void
VerifyBarriers(JSRuntime* rt, VerifierType type)
{
}

static void
MaybeVerifyBarriers(JSContext* cx, bool always = false)
{
}

#else

static inline void
VerifyBarriers(JSRuntime* rt, VerifierType type)
{
}

static inline void
MaybeVerifyBarriers(JSContext* cx, bool always = false)
{
}

#endif

/*
 * Instances of this class set the |JSRuntime::suppressGC| flag for the duration
 * that they are live. Use of this class is highly discouraged. Please carefully
 * read the comment in vm/Runtime.h above |suppressGC| and take all appropriate
 * precautions before instantiating this class.
 */
class MOZ_RAII JS_HAZ_GC_SUPPRESSED AutoSuppressGC
{
  public:
    explicit AutoSuppressGC(ExclusiveContext* cx);
    explicit AutoSuppressGC(JSCompartment* comp);
    explicit AutoSuppressGC(JSContext* cx);

    ~AutoSuppressGC()
    {
    }
};

struct MOZ_RAII AutoAssertNoNurseryAlloc
{
#ifdef DEBUG
    explicit AutoAssertNoNurseryAlloc(JSRuntime* rt);
    ~AutoAssertNoNurseryAlloc();
#else
    explicit AutoAssertNoNurseryAlloc(JSRuntime* rt) {}
#endif
};

/*
 * There are a couple of classes here that serve mostly as "tokens" indicating
 * that a condition holds. Some functions force the caller to possess such a
 * token because they would misbehave if the condition were false, and it is
 * far more clear to make the condition visible at the point where it can be
 * affected rather than just crashing in an assertion down in the place where
 * it is relied upon.
 */

/*
 * Token meaning that the heap is busy and no allocations will be made.
 *
 * This class may be instantiated directly if it is known that the condition is
 * already true, or it can be used as a base class for another RAII class that
 * causes the condition to become true. Such base classes will use the no-arg
 * constructor, establish the condition, then call checkCondition() to assert
 * it and possibly record data needed to re-check the condition during
 * destruction.
 *
 * Ordinarily, you would do something like this with a Maybe<> member that is
 * emplaced during the constructor, but token-requiring functions want to
 * require a reference to a base class instance. That said, you can always pass
 * in the Maybe<> field as the token.
 */
class MOZ_RAII AutoAssertHeapBusy {
  protected:
    JSRuntime* rt;

    // Check that the heap really is busy, and record the rt for the check in
    // the destructor.
    void checkCondition(JSRuntime *rt);

    AutoAssertHeapBusy() : rt(nullptr) {
    }

  public:
    explicit AutoAssertHeapBusy(JSRuntime* rt) {
    }

    ~AutoAssertHeapBusy() {
    }
};

/*
 * A class that serves as a token that the nursery is empty. It descends from
 * AutoAssertHeapBusy, which means that it additionally requires the heap to be
 * busy (which is not necessarily linked, but turns out to be true in practice
 * for all users and simplifies the usage of these classes.)
 */
class MOZ_RAII AutoAssertEmptyNursery
{
  protected:
    JSRuntime* rt;

    mozilla::Maybe<AutoAssertNoNurseryAlloc> noAlloc;

    // Check that the nursery is empty.
    void checkCondition(JSRuntime *rt);

    // For subclasses that need to empty the nursery in their constructors.
    AutoAssertEmptyNursery() : rt(nullptr) {
    }

  public:
    explicit AutoAssertEmptyNursery(JSRuntime* rt) {
    }

    AutoAssertEmptyNursery(const AutoAssertEmptyNursery& other) : AutoAssertEmptyNursery(other.rt)
    {
    }
};

/*
 * Evict the nursery upon construction. Serves as a token indicating that the
 * nursery is empty. (See AutoAssertEmptyNursery, above.)
 *
 * Note that this is very improper subclass of AutoAssertHeapBusy, in that the
 * heap is *not* busy within the scope of an AutoEmptyNursery. I will most
 * likely fix this by removing AutoAssertHeapBusy, but that is currently
 * waiting on jonco's review.
 */
class MOZ_RAII AutoEmptyNursery : public AutoAssertEmptyNursery
{
  public:
    explicit AutoEmptyNursery(JSRuntime *rt);
};

} /* namespace gc */

#ifdef DEBUG
/* Use this to avoid assertions when manipulating the wrapper map. */
class MOZ_RAII AutoDisableProxyCheck
{
  public:
    explicit AutoDisableProxyCheck(JSRuntime* rt);
    ~AutoDisableProxyCheck();
};
#else
struct MOZ_RAII AutoDisableProxyCheck
{
    explicit AutoDisableProxyCheck(JSRuntime* rt) {}
};
#endif

struct MOZ_RAII AutoDisableCompactingGC
{
    explicit AutoDisableCompactingGC(JSContext* cx);
    ~AutoDisableCompactingGC();

  private:
    gc::GCRuntime& gc;
};

// This is the same as IsInsideNursery, but not inlined.
inline bool
UninlinedIsInsideNursery(const gc::Cell* cell) {
	return true;
}

} /* namespace js */

#endif /* jsgc_h */
