/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCRuntime_h
#define gc_GCRuntime_h

#include "mozilla/Atomics.h"
#include "mozilla/EnumSet.h"

#include "jsfriendapi.h"
#include "jsgc.h"

#include "gc/Heap.h"
#include "gc/Nursery.h"
#include "gc/Statistics.h"
#include "gc/StoreBuffer.h"
#include "gc/Tracer.h"
#include "js/GCAnnotations.h"

namespace js {

class AutoLockGC;
class AutoLockHelperThreadState;
class VerifyPreTracer;

namespace gc {

typedef Vector<JS::Zone*, 4, SystemAllocPolicy> ZoneVector;
using BlackGrayEdgeVector = Vector<TenuredCell*, 0, SystemAllocPolicy>;

class AutoMaybeStartBackgroundAllocation;
class MarkingValidator;
class AutoTraceSession;
struct MovingTracer;

template<typename F>
struct Callback {
    F op;
    void* data;

    Callback()
      : op(nullptr), data(nullptr)
    {}
    Callback(F op, void* data)
      : op(op), data(data)
    {}
};

class GCRuntime
{
  public:
    explicit GCRuntime(JSRuntime* rt) : nursery(rt), storeBuffer(rt, nursery), stats(rt), marker(rt), usage(nullptr) {}
    MOZ_MUST_USE bool init(uint32_t maxbytes, uint32_t maxNurseryBytes);
	void finishRoots() {}
    void finish();

    MOZ_MUST_USE bool setParameter(JSGCParamKey key, uint32_t value, AutoLockGC& lock);
    uint32_t getParameter(JSGCParamKey key, const AutoLockGC& lock);

	void maybeGC(Zone* zone);
    void minorGC(JS::gcreason::Reason reason,
                 gcstats::Phase phase = gcstats::PHASE_MINOR_GC) JS_HAZ_GC_CALL;
    void evictNursery(JS::gcreason::Reason reason = JS::gcreason::EVICT_NURSERY) {
        minorGC(reason, gcstats::PHASE_EVICT_NURSERY);
    }
    // The return value indicates whether a major GC was performed.
    bool gcIfRequested();
    void gc(JSGCInvocationKind gckind, JS::gcreason::Reason reason) {}
	void abortGC();
    void startDebugGC(JSGCInvocationKind gckind, SliceBudget& budget);
    void debugGCSlice(SliceBudget& budget);

    inline void poke();

	enum TraceOrMarkRuntime {
        TraceRuntime,
        MarkRuntime
    };

	void notifyDidPaint();
	void onOutOfMallocMemory();

#ifdef JS_GC_ZEAL
    const void* addressOfZealModeBits() { return &zealModeBits; }
    void getZealBits(uint32_t* zealBits, uint32_t* frequency, uint32_t* nextScheduled) {}
    void setZeal(uint8_t zeal, uint32_t frequency) {}
    bool parseAndSetZeal(const char* str) { return true; }
    void setNextScheduled(uint32_t count) {}
	bool selectForMarking(JSObject* object) { return true; }
	void setDeterministic(bool enable) {}
#endif

#ifdef DEBUG
    bool shutdownCollectedEverything() const {
        return true;
    }
#endif

  public:
    // Internal public interface
    State state() const { return (State)0; }
    bool isHeapCompacting() const { return false; }
    bool isForegroundSweeping() const { return false; }
    void waitBackgroundSweepEnd() { }

    void lockGC() {
    }

    void unlockGC() {
    }

#ifdef DEBUG
    bool isAllocAllowed() { true; }
    void disallowAlloc() { }

	bool isStrictProxyCheckingEnabled() { return false; }
#endif // DEBUG

    void setAlwaysPreserveCode() { }

	bool isIncrementalGCInProgress() const { return false; }

	void setGrayRootsTracer(JSTraceDataOp traceOp, void* data);
	MOZ_MUST_USE bool addBlackRootsTracer(JSTraceDataOp traceOp, void* data);
	void removeBlackRootsTracer(JSTraceDataOp traceOp, void* data);

	void updateMallocCounter(JS::Zone* zone, size_t nbytes);

	void setGCCallback(JSGCCallback callback, void* data);
    void setObjectsTenuredCallback(JSObjectsTenuredCallback callback,
                                   void* data);

	MOZ_MUST_USE bool addFinalizeCallback(JSFinalizeCallback callback, void* data);
	void removeFinalizeCallback(JSFinalizeCallback func);
	MOZ_MUST_USE bool addWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback,
                                                      void* data);
	void removeWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback);
    MOZ_MUST_USE bool addWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback,
                                                        void* data);
    void removeWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback);

    void setFullCompartmentChecks(bool enable);

	JS::Zone* getCurrentZoneGroup() { return nullptr; }

    uint64_t gcNumber() const { return 0; }

    uint64_t minorGCCount() const { return 0; }

    uint64_t majorGCCount() const { return 0; }

	bool isFullGc() const { return true; }

	bool areGrayBitsValid() const { return true; }

	bool fullGCForAtomsRequested() const { return true; }

#ifdef JS_GC_ZEAL
    bool isVerifyPreBarriersEnabled() const { return true; }
#else
    bool isVerifyPreBarriersEnabled() const { return false; }
#endif

    // Free certain LifoAlloc blocks when it is safe to do so.
    void freeUnusedLifoBlocksAfterSweeping(LifoAlloc* lifo);
    void freeAllLifoBlocksAfterSweeping(LifoAlloc* lifo);
    void freeAllLifoBlocksAfterMinorGC(LifoAlloc* lifo);

    // Queue a thunk to run after the next minor GC.
    void callAfterMinorGC(void (*thunk)(void* data), void* data) {
    }
	
	void triggerFullGCForAtoms() {
	}

  public:
    JSRuntime* rt;

    /* Embedders can use this zone however they wish. */
    JS::Zone* systemZone;

    /* List of compartments and zones (protected by the GC lock). */
    ZoneVector zones;

    Nursery nursery;
    StoreBuffer storeBuffer;

    gcstats::Statistics stats;

    GCMarker marker;

    /* Track heap usage for this runtime. */
    HeapUsage usage;
	
	js::Mutex lock;
	
	bool hasZealMode(ZealMode mode) { return false; }
	bool upcomingZealousGC() { return false; }
};

/* Prevent compartments and zones from being collected during iteration. */
class MOZ_RAII AutoEnterIteration {

  public:
    explicit AutoEnterIteration(GCRuntime* gc_) {
    }

    ~AutoEnterIteration() {
    }
};

} /* namespace gc */

} /* namespace js */

#endif
