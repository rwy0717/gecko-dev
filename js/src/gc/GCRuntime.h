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

template<typename F>
using CallbackVector = Vector<Callback<F>, 4, SystemAllocPolicy>;

typedef HashMap<Value*, const char*, DefaultHasher<Value*>, SystemAllocPolicy> RootedValueMap;

class GCRuntime
{
  public:
    explicit GCRuntime(JSRuntime* rt)
        : number(0),
		rt(rt),
		nursery(rt),
#ifndef OMR
        storeBuffer(rt, nursery),
#endif // OMR
        stats(rt),
        marker(rt),
        usage(nullptr)
        { }
    
    MOZ_MUST_USE bool init(uint32_t maxbytes, uint32_t maxNurseryBytes);
	void finishRoots();
    void finish();

    MOZ_MUST_USE bool addRoot(Value* vp, const char* name);
    void removeRoot(Value* vp);

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

    void traceRuntime(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeForMinorGC(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeForMajorGC(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeAtoms(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeCommon(JSTracer* trc, TraceOrMarkRuntime traceOrMark,
                            AutoLockForExclusiveAccess& lock);

#ifdef JS_GC_ZEAL
    const void* addressOfZealModeBits() { return nullptr; }
    void getZealBits(uint32_t* zealBits, uint32_t* frequency, uint32_t* nextScheduled);
    void setZeal(uint8_t zeal, uint32_t frequency);
    bool parseAndSetZeal(const char* str);
    void setNextScheduled(uint32_t count);
	bool selectForMarking(JSObject* object);
	void setDeterministic(bool enable);
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
    bool isAllocAllowed() { return true; }
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

	JS::Zone* getCurrentZoneGroup() {
        // OMRTODO: Implement and work with zones correctly
         return systemZone;
    }

	uint64_t number;
    uint64_t gcNumber() const { return number; }
	void incGcNumber() { ++ number; }

    uint64_t minorGCCount() const { return number; }

    uint64_t majorGCCount() const { return number; }

	bool isFullGc() const { return false; }

	bool areGrayBitsValid() const { return true; }

	bool fullGCForAtomsRequested() const { return false; }

#ifdef JS_GC_ZEAL
    bool isVerifyPreBarriersEnabled() const { return false; }
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

    void bufferGrayRoots();
    void maybeDoCycleCollection();
    bool drainMarkStack(SliceBudget& sliceBudget, gcstats::Phase phase);
    template <class CompartmentIterT> void markWeakReferences(gcstats::Phase phase);
    void markWeakReferencesInCurrentGroup(gcstats::Phase phase);
    template <class ZoneIterT, class CompartmentIterT> void markGrayReferences(gcstats::Phase phase);
    void markBufferedGrayRoots(JS::Zone* zone);
    void markGrayReferencesInCurrentGroup(gcstats::Phase phase);
    void markAllWeakReferences(gcstats::Phase phase);
    void markAllGrayReferences(gcstats::Phase phase);

  public:
    JSRuntime* rt;

    /* Embedders can use this zone however they wish. */
    JS::Zone* systemZone;

    /* List of compartments and zones (protected by the GC lock). */
    ZoneVector zones;

    Nursery nursery;

#ifndef OMR // Writebariers
    StoreBuffer storeBuffer;
#endif // ! OMR Writebarriers

    gcstats::Statistics stats;

    GCMarker marker;

    /* Track heap usage for this runtime. */
    HeapUsage usage;

    /*
     * The trace operations to trace embedding-specific GC roots. One is for
     * tracing through black roots and the other is for tracing through gray
     * roots. The black/gray distinction is only relevant to the cycle
     * collector.
     */
    CallbackVector<JSTraceDataOp> blackRootTracers;
    Callback<JSTraceDataOp> grayRootTracer;

	js::Mutex lock;
	
	bool hasZealMode(ZealMode mode) { return false; }
	bool upcomingZealousGC() { return false; }

  private:
    // Gray marking must be done after all black marking is complete. However,
    // we do not have write barriers on XPConnect roots. Therefore, XPConnect
    // roots must be accumulated in the first slice of incremental GC. We
    // accumulate these roots in each zone's gcGrayRoots vector and then mark
    // them later, after black marking is complete for each compartment. This
    // accumulation can fail, but in that case we switch to non-incremental GC.
    enum class GrayBufferState {
        Unused,
        Okay,
        Failed
    };
    GrayBufferState grayBufferState;
    bool hasBufferedGrayRoots() const { return grayBufferState == GrayBufferState::Okay; }

    // Clear each zone's gray buffers, but do not change the current state.
    void resetBufferedGrayRoots() const;

    // Reset the gray buffering state to Unused.
    void clearBufferedGrayRoots() {
        grayBufferState = GrayBufferState::Unused;
        resetBufferedGrayRoots();
    }

    /*
     * The gray bits can become invalid if UnmarkGray overflows the stack. A
     * full GC will reset this bit, since it fills in all the gray bits.
     */
    bool grayBitsValid;

    RootedValueMap rootsHash;
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
