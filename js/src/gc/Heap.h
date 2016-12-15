/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Heap_h
#define gc_Heap_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/PodOperations.h"

#include <stddef.h>
#include <stdint.h>

#include "jsfriendapi.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "jsutil.h"

#include "ds/BitArray.h"
#include "gc/Memory.h"
#include "js/GCAPI.h"
#include "js/HeapAPI.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"

struct JSRuntime;

namespace JS {
namespace shadow {
struct Runtime;
} // namespace shadow
} // namespace JS

namespace js {

class AutoLockGC;
class FreeOp;

extern void
TraceManuallyBarrieredGenericPointerEdge(JSTracer* trc, gc::Cell** thingp, const char* name);

namespace gc {

#ifndef OMR
class Arena;
class ArenaCellSet;
struct Chunk;
#endif // ! OMR

/*
 * This flag allows an allocation site to request a specific heap based upon the
 * estimated lifetime or lifetime requirements of objects allocated from that
 * site.
 */
enum InitialHeap {
    DefaultHeap,
    TenuredHeap
};

/* The GC allocation kinds. */
// FIXME: uint8_t would make more sense for the underlying type, but causes
// miscompilations in GCC (fixed in 4.8.5 and 4.9.3). See also bug 1143966.
enum class AllocKind : uintptr_t {
    FIRST,
    OBJECT_FIRST = FIRST,
    FUNCTION = FIRST,
    FUNCTION_EXTENDED,
    OBJECT0,
    OBJECT0_BACKGROUND,
    OBJECT2,
    OBJECT2_BACKGROUND,
    OBJECT4,
    OBJECT4_BACKGROUND,
    OBJECT8,
    OBJECT8_BACKGROUND,
    OBJECT12,
    OBJECT12_BACKGROUND,
    OBJECT16,
    OBJECT16_BACKGROUND,
    OBJECT_LIMIT,
    OBJECT_LAST = OBJECT_LIMIT - 1,
    SCRIPT,
    LAZY_SCRIPT,
    SHAPE,
    ACCESSOR_SHAPE,
    BASE_SHAPE,
    OBJECT_GROUP,
    FAT_INLINE_STRING,
    STRING,
    EXTERNAL_STRING,
    SYMBOL,
    JITCODE,
    SCOPE,
    LIMIT,
    LAST = LIMIT - 1
};

// Macro to enumerate the different allocation kinds supplying information about
// the trace kind, C++ type and allocation size.
#define FOR_EACH_OBJECT_ALLOCKIND(D) \
 /* AllocKind              TraceKind    TypeName           SizedType */ \
    D(FUNCTION,            Object,      JSObject,          JSFunction) \
    D(FUNCTION_EXTENDED,   Object,      JSObject,          FunctionExtended) \
    D(OBJECT0,             Object,      JSObject,          JSObject_Slots0) \
    D(OBJECT0_BACKGROUND,  Object,      JSObject,          JSObject_Slots0) \
    D(OBJECT2,             Object,      JSObject,          JSObject_Slots2) \
    D(OBJECT2_BACKGROUND,  Object,      JSObject,          JSObject_Slots2) \
    D(OBJECT4,             Object,      JSObject,          JSObject_Slots4) \
    D(OBJECT4_BACKGROUND,  Object,      JSObject,          JSObject_Slots4) \
    D(OBJECT8,             Object,      JSObject,          JSObject_Slots8) \
    D(OBJECT8_BACKGROUND,  Object,      JSObject,          JSObject_Slots8) \
    D(OBJECT12,            Object,      JSObject,          JSObject_Slots12) \
    D(OBJECT12_BACKGROUND, Object,      JSObject,          JSObject_Slots12) \
    D(OBJECT16,            Object,      JSObject,          JSObject_Slots16) \
    D(OBJECT16_BACKGROUND, Object,      JSObject,          JSObject_Slots16)

#define FOR_EACH_NONOBJECT_ALLOCKIND(D) \
 /* AllocKind              TraceKind    TypeName           SizedType */ \
    D(SCRIPT,              Script,      JSScript,          JSScript) \
    D(LAZY_SCRIPT,         LazyScript,  js::LazyScript,    js::LazyScript) \
    D(SHAPE,               Shape,       js::Shape,         js::Shape) \
    D(ACCESSOR_SHAPE,      Shape,       js::AccessorShape, js::AccessorShape) \
    D(BASE_SHAPE,          BaseShape,   js::BaseShape,     js::BaseShape) \
    D(OBJECT_GROUP,        ObjectGroup, js::ObjectGroup,   js::ObjectGroup) \
    D(FAT_INLINE_STRING,   String,      JSFatInlineString, JSFatInlineString) \
    D(STRING,              String,      JSString,          JSString) \
    D(EXTERNAL_STRING,     String,      JSExternalString,  JSExternalString) \
    D(SYMBOL,              Symbol,      JS::Symbol,        JS::Symbol) \
    D(JITCODE,             JitCode,     js::jit::JitCode,  js::jit::JitCode) \
    D(SCOPE,               Scope,       js::Scope,         js::Scope)

#define FOR_EACH_ALLOCKIND(D) \
    FOR_EACH_OBJECT_ALLOCKIND(D) \
    FOR_EACH_NONOBJECT_ALLOCKIND(D)

static_assert(int(AllocKind::FIRST) == 0, "Various places depend on AllocKind starting at 0, "
                                          "please audit them carefully!");
static_assert(int(AllocKind::OBJECT_FIRST) == 0, "Various places depend on AllocKind::OBJECT_FIRST "
                                                 "being 0, please audit them carefully!");

inline bool
IsObjectAllocKind(AllocKind kind)
{
    return kind >= AllocKind::OBJECT_FIRST && kind <= AllocKind::OBJECT_LAST;
}

inline bool
IsShapeAllocKind(AllocKind kind)
{
    return kind == AllocKind::SHAPE || kind == AllocKind::ACCESSOR_SHAPE;
}

class TenuredCell;

// A GC cell is the base class for all GC things.
struct Cell
{
  public:
    using Flags = uintptr_t;

  public:
    MOZ_ALWAYS_INLINE bool isTenured() const { return !IsInsideNursery(this); }
    MOZ_ALWAYS_INLINE const TenuredCell& asTenured() const;
    MOZ_ALWAYS_INLINE TenuredCell& asTenured();

    inline JSRuntime* runtimeFromMainThread() const;
    inline JS::shadow::Runtime* shadowRuntimeFromMainThread() const;
    inline JS::Zone* zone() const;

    // Note: Unrestricted access to the runtime of a GC thing from an arbitrary
    // thread can easily lead to races. Use this method very carefully.
    inline JSRuntime* runtimeFromAnyThread() const;
    inline JS::shadow::Runtime* shadowRuntimeFromAnyThread() const;
#ifdef OMR
        inline JS::Zone* zoneFromAnyThread() const;
#endif // OMR
    // May be overridden by GC thing kinds that have a compartment pointer.
    inline JSCompartment* maybeCompartment() const { return nullptr; }

#ifndef OMR // Writebarriers
    inline StoreBuffer* storeBuffer() const;
#endif // ! OMR Writebarriers

    inline JS::TraceKind getTraceKind() const;

    inline AllocKind getAllocKind() const { return (AllocKind)flags_; }

    inline void setAllocKind(AllocKind allocKind) { flags_ = (Flags)allocKind; }

    static MOZ_ALWAYS_INLINE bool needWriteBarrierPre(JS::Zone* zone);

#ifdef DEBUG
    inline bool isAligned() const;
    void dump(FILE* fp) const;
    void dump() const;
#endif

  protected:
    inline uintptr_t address() const;
#ifndef OMR
    inline Chunk* chunk() const;
#endif // ! OMR

  public:
    Flags flags_;

} JS_HAZ_GC_THING;

// A GC TenuredCell gets behaviors that are valid for things in the Tenured
// heap, such as access to the arena and mark bits.
class TenuredCell : public Cell
{
  public:
    // Construct a TenuredCell from a void*, making various sanity assertions.
    static MOZ_ALWAYS_INLINE TenuredCell* fromPointer(void* ptr);
    static MOZ_ALWAYS_INLINE const TenuredCell* fromPointer(const void* ptr);

    // Mark bit management.
    MOZ_ALWAYS_INLINE bool isMarked(uint32_t color = BLACK) const;
    // The return value indicates if the cell went from unmarked to marked.
    MOZ_ALWAYS_INLINE bool markIfUnmarked(uint32_t color = BLACK) const;
    MOZ_ALWAYS_INLINE void unmark(uint32_t color) const;
    MOZ_ALWAYS_INLINE void copyMarkBitsFrom(const TenuredCell* src);

    // Note: this is in TenuredCell because JSObject subclasses are sometimes
    // used tagged.
    static MOZ_ALWAYS_INLINE bool isNullLike(const Cell* thing) { return !thing; }

    // Access to the arena.

#ifndef OMR // Disable Arenas
    inline Arena* arena() const;
#endif // ! OMR

    inline JS::TraceKind getTraceKind() const;

#ifndef OMR // Disable Arenas
    inline JS::Zone* zone() const;
    inline JS::Zone* zoneFromAnyThread() const;
    inline bool isInsideZone(JS::Zone* zone) const;

    MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZone() const {
        return JS::shadow::Zone::asShadowZone(zone());
    }
    MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZoneFromAnyThread() const {
        return JS::shadow::Zone::asShadowZone(zoneFromAnyThread());
    }
#endif // ! OMR Arenas

    static MOZ_ALWAYS_INLINE void readBarrier(TenuredCell* thing);
    static MOZ_ALWAYS_INLINE void writeBarrierPre(TenuredCell* thing);

    static MOZ_ALWAYS_INLINE void writeBarrierPost(void* cellp, TenuredCell* prior,
                                                   TenuredCell* next);

    // Default implementation for kinds that don't require fixup.
    void fixupAfterMovingGC() {}

#ifdef DEBUG
    inline bool isAligned() const;
#endif
};

/* Cells are aligned to CellShift, so the largest tagged null pointer is: */
const uintptr_t LargestTaggedNullCellPointer = (1 << CellShift) - 1;

class FreeSpan
{
  public:
    static size_t offsetOfFirst() {
        return 0;
    }

    static size_t offsetOfLast() {
        return 0;
    }
};

#ifdef OMR // Arena replacement helpers
// OMRTODO: Move to object model
class OmrGcHelper {
public:

    static JS_FRIEND_DATA(const uint32_t) thingSizes[];

    static size_t thingSize(AllocKind kind) {
        return thingSizes[size_t(kind)];
    }

    static JS::Zone* zone;
    static GCRuntime* runtime;
};
#endif // ! OMR Arena replacemnt helpers

#ifndef OMR // Arenas

/*
 * Arenas are the allocation units of the tenured heap in the GC. An arena
 * is 4kiB in size and 4kiB-aligned. It starts with several header fields
 * followed by some bytes of padding. The remainder of the arena is filled
 * with GC things of a particular AllocKind. The padding ensures that the
 * GC thing array ends exactly at the end of the arena:
 *
 * <----------------------------------------------> = ArenaSize bytes
 * +---------------+---------+----+----+-----+----+
 * | header fields | padding | T0 | T1 | ... | Tn |
 * +---------------+---------+----+----+-----+----+
 * <-------------------------> = first thing offset
 */
class Arena
{
    static JS_FRIEND_DATA(const uint32_t) ThingSizes[];
    static JS_FRIEND_DATA(const uint32_t) FirstThingOffsets[];
    static JS_FRIEND_DATA(const uint32_t) ThingsPerArena[];

  public:
    /*
     * The zone that this Arena is contained within, when allocated. The offset
     * of this field must match the ArenaZoneOffset stored in js/HeapAPI.h,
     * as is statically asserted below.
     */
    JS::Zone* zone;

    /*
     * Arena::next has two purposes: when unallocated, it points to the next
     * available Arena. When allocated, it points to the next Arena in the same
     * zone and with the same alloc kind.
     */
    Arena* next;

  private:
    /*
     * One of the AllocKind constants or AllocKind::LIMIT when the arena does
     * not contain any GC things and is on the list of empty arenas in the GC
     * chunk.
     *
     * We use 8 bits for the alloc kind so the compiler can use byte-level
     * memory instructions to access it.
     */
    size_t allocKind : 8;

  public:
    /*
     * When collecting we sometimes need to keep an auxillary list of arenas,
     * for which we use the following fields. This happens for several reasons:
     *
     * When recursive marking uses too much stack, the marking is delayed and
     * the corresponding arenas are put into a stack. To distinguish the bottom
     * of the stack from the arenas not present in the stack we use the
     * markOverflow flag to tag arenas on the stack.
     *
     * Delayed marking is also used for arenas that we allocate into during an
     * incremental GC. In this case, we intend to mark all the objects in the
     * arena, and it's faster to do this marking in bulk.
     *
     * When sweeping we keep track of which arenas have been allocated since
     * the end of the mark phase. This allows us to tell whether a pointer to
     * an unmarked object is yet to be finalized or has already been
     * reallocated. We set the allocatedDuringIncremental flag for this and
     * clear it at the end of the sweep phase.
     *
     * To minimize the size of the header fields we record the next linkage as
     * address() >> ArenaShift and pack it with the allocKind and the flags.
     */
    size_t hasDelayedMarking : 1;
    size_t allocatedDuringIncremental : 1;
    size_t markOverflow : 1;
    size_t auxNextLink : JS_BITS_PER_WORD - 8 - 1 - 1 - 1;

    /*
     * If non-null, points to an ArenaCellSet that represents the set of cells
     * in this arena that are in the nursery's store buffer.
     */
    ArenaCellSet* bufferedCells;

    /*
     * The size of data should be |ArenaSize - offsetof(data)|, but the offset
     * is not yet known to the compiler, so we do it by hand. |firstFreeSpan|
     * takes up 8 bytes on 64-bit due to alignment requirements; the rest are
     * obvious. This constant is stored in js/HeapAPI.h.
     */
    uint8_t data[ArenaSize - ArenaHeaderSize];

    void init(JS::Zone* zoneArg, AllocKind kind);

    uintptr_t address() const {
        checkAddress();
        return uintptr_t(this);
    }

    inline void checkAddress() const;

    inline Chunk* chunk() const;

    bool allocated() const {
        return true;
    }

    AllocKind getAllocKind() const {
        return AllocKind(allocKind);
    }

    static size_t thingSize(AllocKind kind) { return ThingSizes[size_t(kind)]; }
    static size_t thingsPerArena(AllocKind kind) { return ThingsPerArena[size_t(kind)]; }
    static size_t thingsSpan(AllocKind kind) { return thingsPerArena(kind) * thingSize(kind); }

    bool isEmpty() const {
        return true;
    }

    static bool isAligned(uintptr_t thing, size_t thingSize) {
        return true;
    }

    template <typename T>
    size_t finalize(FreeOp* fop, AllocKind thingKind, size_t thingSize);

    static void staticAsserts();
};

/*
 * Calculating ArenasPerChunk:
 *
 * In order to figure out how many Arenas will fit in a chunk, we need to know
 * how much extra space is available after we allocate the header data. This
 * is a problem because the header size depends on the number of arenas in the
 * chunk. The two dependent fields are bitmap and decommittedArenas.
 *
 * For the mark bitmap, we know that each arena will use a fixed number of full
 * bytes: ArenaBitmapBytes. The full size of the header data is this number
 * multiplied by the eventual number of arenas we have in the header. We,
 * conceptually, distribute this header data among the individual arenas and do
 * not include it in the header. This way we do not have to worry about its
 * variable size: it gets attached to the variable number we are computing.
 *
 * For the decommitted arena bitmap, we only have 1 bit per arena, so this
 * technique will not work. Instead, we observe that we do not have enough
 * header info to fill 8 full arenas: it is currently 4 on 64bit, less on
 * 32bit. Thus, with current numbers, we need 64 bytes for decommittedArenas.
 * This will not become 63 bytes unless we double the data required in the
 * header. Therefore, we just compute the number of bytes required to track
 * every possible arena and do not worry about slop bits, since there are too
 * few to usefully allocate.
 *
 * To actually compute the number of arenas we can allocate in a chunk, we
 * divide the amount of available space less the header info (not including
 * the mark bitmap which is distributed into the arena size) by the size of
 * the arena (with the mark bitmap bytes it uses).
 */
const size_t BytesPerArenaWithHeader = 0;
const size_t ChunkDecommitBitmapBytes = 0;
const size_t ChunkBytesAvailable = 0;
const size_t ArenasPerChunk = 0;

typedef BitArray<ArenasPerChunk> PerArenaBitmap;

/*
 * Chunks contain arenas and associated data structures (mark bitmap, delayed
 * marking state).
 */
struct Chunk
{
    PerArenaBitmap  decommittedArenas;

    uintptr_t address() const {
        uintptr_t addr = reinterpret_cast<uintptr_t>(this);
        MOZ_ASSERT(!(addr & ChunkMask));
        return addr;
    }

    bool unused() const {
        return true;
    }

    static Chunk* allocate(JSRuntime* rt);
    void init(JSRuntime* rt);

  public:
    /* Unlink and return the freeArenasHead. */
    Arena* fetchNextFreeArena(JSRuntime* rt);
};

#endif // ! OMR Arenas

/*
 * Tracks the used sizes for owned heap data and automatically maintains the
 * memory usage relationship between GCRuntime and Zones.
 */
class HeapUsage
{
  public:
    explicit HeapUsage(HeapUsage* parent)
    {}

    size_t gcBytes() const { return 0; }
};

#ifndef OMR // Arenas

inline void
Arena::checkAddress() const
{
}

inline Chunk*
Arena::chunk() const
{
	return nullptr;
}

#endif // ! OMR Arenas

MOZ_ALWAYS_INLINE const TenuredCell&
Cell::asTenured() const
{
    return *static_cast<const TenuredCell*>(this);
}

MOZ_ALWAYS_INLINE TenuredCell&
Cell::asTenured()
{
    return *static_cast<TenuredCell*>(this);
}

// OMRTOO: Getting Runtime with context

inline JSRuntime*
Cell::runtimeFromMainThread() const
{
    return nullptr;
}

inline JS::shadow::Runtime*
Cell::shadowRuntimeFromMainThread() const
{
    return nullptr;
}

inline JSRuntime*
Cell::runtimeFromAnyThread() const
{
    return nullptr;
}
inline JS::Zone*
Cell::zoneFromAnyThread() const
{
    // OMRTODO: Proper zones
    return OmrGcHelper::zone;
}

inline JS::Zone*
Cell::zone() const
{
    // OMRTODO: Use multiple zones obtained from a thread context
    return OmrGcHelper::zone;
}

inline JS::shadow::Runtime*
Cell::shadowRuntimeFromAnyThread() const
{
    return nullptr;
}

inline uintptr_t
Cell::address() const
{
	return 0;
}

#ifndef OMR // Disable Chunks, write barriers

Chunk*
Cell::chunk() const
{
    return nullptr;
}

inline StoreBuffer*
Cell::storeBuffer() const
{
    return nullptr;
}

#endif // ! OMR

inline JS::TraceKind
Cell::getTraceKind() const
{
    return JS::TraceKind::Object;
}

/* static */ MOZ_ALWAYS_INLINE bool
Cell::needWriteBarrierPre(JS::Zone* zone) {
    return false;
}

/* static */ MOZ_ALWAYS_INLINE TenuredCell*
TenuredCell::fromPointer(void* ptr)
{
    return nullptr;
}

/* static */ MOZ_ALWAYS_INLINE const TenuredCell*
TenuredCell::fromPointer(const void* ptr)
{
    return nullptr;
}

bool
TenuredCell::isMarked(uint32_t color /* = BLACK */) const
{
	return true;
}

bool
TenuredCell::markIfUnmarked(uint32_t color /* = BLACK */) const
{
	return true;
}

void
TenuredCell::unmark(uint32_t color) const
{
}

void
TenuredCell::copyMarkBitsFrom(const TenuredCell* src)
{
}

#ifndef OMR // Disable Arenas
inline Arena*
TenuredCell::arena() const
{
    return NULL;
}
#endif // ! OMR

// OMRTODO: What is a trace kind?
JS::TraceKind
TenuredCell::getTraceKind() const
{
    return (JS::TraceKind)0;
}

#ifndef OMR // Disable getting zone without context

JS::Zone*
TenuredCell::zone() const
{
    JS::Zone* zone = arena()->zone;
    MOZ_ASSERT(CurrentThreadCanAccessZone(zone));
    return zone;
}

JS::Zone*
TenuredCell::zoneFromAnyThread() const
{
    return arena()->zone;
}

bool
TenuredCell::isInsideZone(JS::Zone* zone) const
{
    return true;
}

#endif // ! OMR

// OMRTODO: Implement write barriers

/* static */ MOZ_ALWAYS_INLINE void
TenuredCell::readBarrier(TenuredCell* thing)
{
}

void
AssertSafeToSkipBarrier(TenuredCell* thing);

/* static */ MOZ_ALWAYS_INLINE void
TenuredCell::writeBarrierPre(TenuredCell* thing)
{
}

static MOZ_ALWAYS_INLINE void
AssertValidToSkipBarrier(TenuredCell* thing)
{
}

/* static */ MOZ_ALWAYS_INLINE void
TenuredCell::writeBarrierPost(void* cellp, TenuredCell* prior, TenuredCell* next)
{
}

#ifdef DEBUG
bool
Cell::isAligned() const
{
    return true;
}

bool
TenuredCell::isAligned() const
{
    return true;
}
#endif // DEBUG

#ifndef OMR
static const int32_t ChunkLocationOffsetFromLastByte =
    int32_t(gc::ChunkLocationOffset) - int32_t(gc::ChunkMask);
#endif // ! OMR

} /* namespace gc */
} /* namespace js */

#endif /* gc_Heap_h */
