/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Zone_h
#define gc_Zone_h

#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"

#include "ds/SplayTree.h"
#include "gc/FindSCCs.h"
#include "gc/GCRuntime.h"
#include "js/GCHashTable.h"
#include "js/TracingAPI.h"
#include "vm/MallocProvider.h"
#include "vm/TypeInference.h"

namespace js {

namespace jit {
class JitZone;
} // namespace jit

namespace gc {

struct ZoneComponentFinder : public ComponentFinder<JS::Zone, ZoneComponentFinder>
{
    ZoneComponentFinder(uintptr_t sl, AutoLockForExclusiveAccess& lock)
      : ComponentFinder<JS::Zone, ZoneComponentFinder>(sl), lock(lock)
    {}

    AutoLockForExclusiveAccess& lock;
};

template <typename T>
class ZoneCellIter;

} // namespace gc
} // namespace js

namespace JS {

// A zone is a collection of compartments. Every compartment belongs to exactly
// one zone. In Firefox, there is roughly one zone per tab along with a system
// zone for everything else. Zones mainly serve as boundaries for garbage
// collection. Unlike compartments, they have no special security properties.
//
// Every GC thing belongs to exactly one zone. GC things from the same zone but
// different compartments can share an arena (4k page). GC things from different
// zones cannot be stored in the same arena. The garbage collector is capable of
// collecting one zone at a time; it cannot collect at the granularity of
// compartments.
//
// GC things are tied to zones and compartments as follows:
//
// - JSObjects belong to a compartment and cannot be shared between
//   compartments. If an object needs to point to a JSObject in a different
//   compartment, regardless of zone, it must go through a cross-compartment
//   wrapper. Each compartment keeps track of its outgoing wrappers in a table.
//   JSObjects find their compartment via their ObjectGroup.
//
// - JSStrings do not belong to any particular compartment, but they do belong
//   to a zone. Thus, two different compartments in the same zone can point to a
//   JSString. When a string needs to be wrapped, we copy it if it's in a
//   different zone and do nothing if it's in the same zone. Thus, transferring
//   strings within a zone is very efficient.
//
// - Shapes and base shapes belong to a zone and are shared between compartments
//   in that zone where possible. Accessor shapes store getter and setter
//   JSObjects which belong to a single compartment, so these shapes and all
//   their descendants can't be shared with other compartments.
//
// - Scripts are also compartment-local and cannot be shared. A script points to
//   its compartment.
//
// - ObjectGroup and JitCode objects belong to a compartment and cannot be
//   shared. There is no mechanism to obtain the compartment from a JitCode
//   object.
//
// A zone remains alive as long as any GC things in the zone are alive. A
// compartment remains alive as long as any JSObjects, scripts, shapes, or base
// shapes within it are alive.
//
// We always guarantee that a zone has at least one live compartment by refusing
// to delete the last compartment in a live zone.
struct Zone : public JS::shadow::Zone,
              public js::gc::GraphNodeBase<JS::Zone>,
              public js::MallocProvider<JS::Zone>
{
    explicit Zone(JSRuntime* rt);
	
    ~Zone();
    MOZ_MUST_USE bool init(bool isSystem) { return true; }

    void discardJitCode(js::FreeOp* fop) {}

    void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                size_t* typePool,
                                size_t* baselineStubsOptimized,
                                size_t* uniqueIdMap,
                                size_t* shapeTables);

    void updateMallocCounter(size_t nbytes) {
    }

    // Iterate over all cells in the zone. See the definition of ZoneCellIter
    // in jsgcinlines.h for the possible arguments and documentation.
    template <typename T, typename... Args>
    js::gc::ZoneCellIter<T> cellIter(Args&&... args) {
        return js::gc::ZoneCellIter<T>(const_cast<Zone*>(this), mozilla::Forward<Args>(args)...);
    }

    MOZ_MUST_USE void* onOutOfMemory(js::AllocFunction allocFunc, size_t nbytes,
                                               void* reallocPtr = nullptr) {
        return nullptr;
    }
    void reportAllocationOverflow() { }

    void setPreservingCode(bool preserving) { }
    bool isPreservingCode() const { return true; }

    enum GCState {
        NoGC,
        Mark,
        MarkGray,
        Sweep,
        Finished,
        Compact
    };

    bool isCollecting() const {
        return false;
    }

    bool isGCMarking() {
        return false;
    }

    GCState gcState() const { return NoGC; }
    bool wasGCStarted() const { return false; }
    bool isGCSweeping() { return false; }
    bool isGCFinished() { return true; }
    bool isGCCompacting() { return false; }
    bool isGCSweepingOrCompacting() { return false; }

    // Get a number that is incremented whenever this zone is collected, and
    // possibly at other times too.
    uint64_t gcNumber() { return runtimeFromMainThread()->gc.gcNumber(); }

    const bool* addressOfNeedsIncrementalBarrier() const { return nullptr; }

    js::jit::JitZone* getJitZone(JSContext* cx) { return jitZone_ ? jitZone_ : createJitZone(cx); }
    js::jit::JitZone* jitZone() { return jitZone_; }

    bool isAtomsZone() const { return true; }
    bool isSelfHostingZone() const { return true; }

#ifdef DEBUG
    // For testing purposes, return the index of the zone group which this zone
    // was swept in in the last GC.
    unsigned lastZoneGroupIndex() { return 0; }
#endif

    using DebuggerVector = js::Vector<js::Debugger*, 0, js::SystemAllocPolicy>;

  private:
    js::jit::JitZone* createJitZone(JSContext* cx);

  public:
    bool hasDebuggers() const { return true; }
    DebuggerVector* getDebuggers() const { return nullptr; }
    DebuggerVector* getOrCreateDebuggers(JSContext* cx);

    void clearTables();

    /*
     * When true, skip calling the metadata callback. We use this:
     * - to avoid invoking the callback recursively;
     * - to avoid observing lazy prototype setup (which confuses callbacks that
     *   want to use the types being set up!);
     * - to avoid attaching allocation stacks to allocation stack nodes, which
     *   is silly
     * And so on.
     */
    bool suppressAllocationMetadataBuilder;

#ifndef OMR
    js::gc::ArenaLists arenas;
#endif // OMR

    js::TypeZone types;

    /* Live weakmaps in this zone. */
    mozilla::LinkedList<js::WeakMapBase> gcWeakMapList;

    // The set of compartments in this zone.
    typedef js::Vector<JSCompartment*, 1, js::SystemAllocPolicy> CompartmentVector;
    CompartmentVector compartments;

    // This zone's gray roots.
    typedef js::Vector<js::gc::Cell*, 0, js::SystemAllocPolicy> GrayRootVector;
    GrayRootVector gcGrayRoots;

    // This zone's weak edges found via graph traversal during marking,
    // preserved for re-scanning during sweeping.
    using WeakEdges = js::Vector<js::gc::TenuredCell**, 0, js::SystemAllocPolicy>;
    WeakEdges gcWeakRefs;

    // List of non-ephemeron weak containers to sweep during beginSweepingZoneGroup.
    mozilla::LinkedList<WeakCache<void*>> weakCaches_;
    void registerWeakCache(WeakCache<void*>* cachep) {
        weakCaches_.insertBack(cachep);
    }

    /*
     * Mapping from not yet marked keys to a vector of all values that the key
     * maps to in any live weak map.
     */
    js::gc::WeakKeyTable gcWeakKeys;

    // A set of edges from this zone to other zones.
    //
    // This is used during GC while calculating zone groups to record edges that
    // can't be determined by examining this zone by itself.
    ZoneSet gcZoneGroupEdges;

    // Keep track of all TypeDescr and related objects in this compartment.
    // This is used by the GC to trace them all first when compacting, since the
    // TypedObject trace hook may access these objects.
    using TypeDescrObjectSet = js::GCHashSet<js::HeapPtr<JSObject*>,
                                             js::MovableCellHasher<js::HeapPtr<JSObject*>>,
                                             js::SystemAllocPolicy>;
    JS::WeakCache<TypeDescrObjectSet> typeDescrObjects;


    // Malloc counter to measure memory pressure for GC scheduling. It runs from
    // gcMaxMallocBytes down to zero. This counter should be used only when it's
    // not possible to know the size of a free.
    mozilla::Atomic<ptrdiff_t, mozilla::ReleaseAcquire> gcMallocBytes;

    // GC trigger threshold for allocations on the C heap.
    size_t gcMaxMallocBytes;

    // Whether a GC has been triggered as a result of gcMallocBytes falling
    // below zero.
    //
    // This should be a bool, but Atomic only supports 32-bit and pointer-sized
    // types.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> gcMallocGCTriggered;

    // Track heap usage under this Zone.
    js::gc::HeapUsage usage;

    // Amount of data to allocate before triggering a new incremental slice for
    // the current GC.
    size_t gcDelayBytes;

    // Shared Shape property tree.
    js::PropertyTree propertyTree;

    // Set of all unowned base shapes in the Zone.
    JS::WeakCache<js::BaseShapeSet> baseShapes;

    // Set of initial shapes in the Zone.
    JS::WeakCache<js::InitialShapeSet> initialShapes;

#ifdef JSGC_HASH_TABLE_CHECKS
    void checkInitialShapesTableAfterMovingGC();
    void checkBaseShapeTableAfterMovingGC();
#endif
    void fixupInitialShapeTable();
    void fixupAfterMovingGC();

    // Per-zone data for use by an embedder.
    void* data;

    bool isSystem;

    mozilla::Atomic<bool> usedByExclusiveThread;

    // True when there are active frames.
    bool active;

#ifdef DEBUG
    unsigned gcLastZoneGroupIndex;
#endif

    static js::HashNumber UniqueIdToHash(uint64_t uid) {
        return js::HashNumber(uid >> 32) ^ js::HashNumber(uid & 0xFFFFFFFF);
    }

    // Creates a HashNumber based on getUniqueId. Returns false on OOM.
    MOZ_MUST_USE bool getHashCode(js::gc::Cell* cell, js::HashNumber* hashp) {
        return true;
    }

    // Puts an existing UID in |uidp|, or creates a new UID for this Cell and
    // puts that into |uidp|. Returns false on OOM.
    MOZ_MUST_USE bool getUniqueId(js::gc::Cell* cell, uint64_t* uidp) {
        return true;
    }

    js::HashNumber getHashCodeInfallible(js::gc::Cell* cell) {
        return UniqueIdToHash(getUniqueIdInfallible(cell));
    }

    uint64_t getUniqueIdInfallible(js::gc::Cell* cell) {
        return 0;
    }

    // Return true if this cell has a UID associated with it.
    MOZ_MUST_USE bool hasUniqueId(js::gc::Cell* cell) {
        return true;
    }

    // Transfer an id from another cell. This must only be called on behalf of a
    // moving GC. This method is infallible.
    void transferUniqueId(js::gc::Cell* tgt, js::gc::Cell* src) {
    }

    // Remove any unique id associated with this Cell.
    void removeUniqueId(js::gc::Cell* cell) {
    }

    // When finished parsing off-thread, transfer any UIDs we created in the
    // off-thread zone into the target zone.
    void adoptUniqueIds(JS::Zone* source) {
    }

    JSContext* contextFromMainThread() {
        return nullptr;
    }

#ifdef JSGC_HASH_TABLE_CHECKS
    // Assert that the UniqueId table has been redirected successfully.
    void checkUniqueIdTableAfterMovingGC() {}
#endif
    friend bool js::CurrentThreadCanAccessZone(Zone* zone);
    friend class js::gc::GCRuntime;

  private:
    js::jit::JitZone* jitZone_;
};

} // namespace JS

namespace js {

// Using the atoms zone without holding the exclusive access lock is dangerous
// because worker threads may be using it simultaneously. Therefore, it's
// better to skip the atoms zone when iterating over zones. If you need to
// iterate over the atoms zone, consider taking the exclusive access lock first.
enum ZoneSelector {
    WithAtoms,
    SkipAtoms
};

class ZonesIter
{
    gc::AutoEnterIteration iterMarker;
    JS::Zone** it;
    JS::Zone** end;

  public:
    ZonesIter(JSRuntime* rt, ZoneSelector selector) : iterMarker(&rt->gc) {
        it = rt->gc.zones.begin();
        end = rt->gc.zones.end();

		/*MOZ_ASSERT(*it == *end);

        if (selector == SkipAtoms) {
            MOZ_ASSERT(atAtomsZone(rt));
            it++;
        }*/
    }

    bool atAtomsZone(JSRuntime* rt);

    bool done() const { return it == end; }

    void next() {
        MOZ_ASSERT(!done());
        do {
            it++;
        } while (!done() && (*it)->usedByExclusiveThread);
    }

    JS::Zone* get() const {
        MOZ_ASSERT(!done());
        return *it;
    }

    operator JS::Zone*() const { return get(); }
    JS::Zone* operator->() const { return get(); }
};

struct CompartmentsInZoneIter
{
    explicit CompartmentsInZoneIter(JS::Zone* zone) {
    }

    bool done() const {
        return true;
    }
    void next() {
    }

    JSCompartment* get() const {
        return nullptr;
    }

    operator JSCompartment*() const { return get(); }
    JSCompartment* operator->() const { return get(); }

  private:

    CompartmentsInZoneIter()
    {}

    // This is for the benefit of CompartmentsIterT::comp.
    friend class mozilla::Maybe<CompartmentsInZoneIter>;
};

// This iterator iterates over all the compartments in a given set of zones. The
// set of zones is determined by iterating ZoneIterT.
template<class ZonesIterT>
class CompartmentsIterT
{
  public:
    explicit CompartmentsIterT(JSRuntime* rt)
    {
    }

    CompartmentsIterT(JSRuntime* rt, ZoneSelector selector)
    {
    }

    bool done() const { return true; }

    void next() {
    }

    JSCompartment* get() const {
        return nullptr;
    }

    operator JSCompartment*() const { return get(); }
    JSCompartment* operator->() const { return get(); }
};

typedef CompartmentsIterT<ZonesIter> CompartmentsIter;

/*
 * Allocation policy that uses Zone::pod_malloc and friends, so that memory
 * pressure is accounted for on the zone. This is suitable for memory associated
 * with GC things allocated in the zone.
 *
 * Since it doesn't hold a JSContext (those may not live long enough), it can't
 * report out-of-memory conditions itself; the caller must check for OOM and
 * take the appropriate action.
 *
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class ZoneAllocPolicy
{
  public:
    MOZ_IMPLICIT ZoneAllocPolicy(Zone* zone) {}

    template <typename T>
    T* maybe_pod_malloc(size_t numElems) {
        return nullptr;
    }

    template <typename T>
    T* maybe_pod_calloc(size_t numElems) {
        return nullptr;
    }

    template <typename T>
    T* maybe_pod_realloc(T* p, size_t oldSize, size_t newSize) {
        return nullptr;
    }

    template <typename T>
    T* pod_malloc(size_t numElems) {
        return nullptr;
    }

    template <typename T>
    T* pod_calloc(size_t numElems) {
        return nullptr;
    }

    template <typename T>
    T* pod_realloc(T* p, size_t oldSize, size_t newSize) {
        return nullptr;
    }

    void free_(void* p) { }
    void reportAllocOverflow() const {}

    MOZ_MUST_USE bool checkSimulatedOOM() const {
        return !false;
    }
};

} // namespace js

#endif // gc_Zone_h
