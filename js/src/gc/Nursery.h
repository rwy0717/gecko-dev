/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Nursery_h
#define gc_Nursery_h

#include "mozilla/EnumeratedArray.h"

#include "jsalloc.h"
#include "jspubtd.h"

#include "ds/BitArray.h"
#include "gc/Heap.h"
#include "gc/Memory.h"
#include "js/Class.h"
#include "js/GCAPI.h"
#include "js/HashTable.h"
#include "js/HeapAPI.h"
#include "js/Value.h"
#include "js/Vector.h"
#include "vm/SharedMem.h"

#include "omrgc.h"

namespace JS {
struct Zone;
} // namespace JS

namespace js {

class ObjectElements;
class NativeObject;
class Nursery;
class HeapSlot;

inline void SetGCZeal(JSRuntime*, uint8_t, uint32_t) {}

namespace gc {
struct Cell;
class MinorCollectionTracer;
class RelocationOverlay;
struct TenureCountCache;
} /* namespace gc */

namespace jit {
class MacroAssembler;
} // namespace jit

class TenuringTracer : public JSTracer
{
    TenuringTracer(JSRuntime* rt, Nursery* nursery) : JSTracer(rt, (JSTracer::TracerKindTag)0, (WeakMapTraceKind)0) {}

  public:

    // Returns true if the pointer was updated.
    template <typename T> void traverse(T** thingp) {}
    template <typename T> void traverse(T* thingp) {}

    // The store buffers need to be able to call these directly.
    void traceObject(JSObject* src) {}
};

/*
 * Classes with JSCLASS_SKIP_NURSERY_FINALIZE or Wrapper classes with
 * CROSS_COMPARTMENT flags will not have their finalizer called if they are
 * nursery allocated and not promoted to the tenured heap. The finalizers for
 * these classes must do nothing except free data which was allocated via
 * Nursery::allocateBuffer.
 */
inline bool
CanNurseryAllocateFinalizedClass(const js::Class* const clasp)
{
    return true;
}

class Nursery
{
  public:

    explicit Nursery(JSRuntime* rt) {}
    ~Nursery() {}

    MOZ_MUST_USE bool init(uint32_t maxNurseryBytes, AutoLockGC& lock) { return true; }

    bool exists() const { return false; }
    size_t nurserySize() const { return 0; }

    void enable();
    void disable();
    bool isEnabled() const { return true; }

    /* Return true if no allocations have been made since the last collection. */
    bool isEmpty() const { return true; }

    /*
     * Check whether an arbitrary pointer is within the nursery. This is
     * slower than IsInsideNursery(Cell*), but works on all types of pointers.
     */
    MOZ_ALWAYS_INLINE bool isInside(gc::Cell* cellp) const = delete;
    MOZ_ALWAYS_INLINE bool isInside(const void* p) const {
        return false;
    }
    template<typename T>
    bool isInside(const SharedMem<T>& p) const {
        return true;
    }

    /*
     * Allocate and return a pointer to a new GC object with its |slots|
     * pointer pre-filled. Returns nullptr if the Nursery is full.
     */
    JSObject* allocateObject(JSContext* cx, size_t size, size_t numDynamic, const js::Class* clasp);

    /* Allocate a buffer for a given zone, using the nursery if possible. */
    void* allocateBuffer(JS::Zone* zone, uint32_t nbytes) { return OMR_GC_Allocate(Nursery::omrVMThread, 0, nbytes, 0); }

    /*
     * Allocate a buffer for a given object, using the nursery if possible and
     * obj is in the nursery.
     */
    void* allocateBuffer(JSObject* obj, uint32_t nbytes) { return OMR_GC_Allocate(Nursery::omrVMThread, 0, nbytes, 0); }

    /* Free an object buffer. */
    void freeBuffer(void* buffer) {}

    /* The maximum number of bytes allowed to reside in nursery buffers. */
    static const size_t MaxNurseryBufferSize = 1024;

    /* Do a minor collection. */
    void collect(JSRuntime* rt, JS::gcreason::Reason reason) {}

    /*
     * Check if the thing at |*ref| in the Nursery has been forwarded. If so,
     * sets |*ref| to the new location of the object and returns true. Otherwise
     * returns false and leaves |*ref| unset.
     */
    MOZ_ALWAYS_INLINE MOZ_MUST_USE bool getForwardedPointer(JSObject** ref) const { return true; }

    /* Forward a slots/elements pointer stored in an Ion frame. */
    void forwardBufferPointer(HeapSlot** pSlotsElems) {}

    void maybeSetForwardingPointer(JSTracer* trc, void* oldData, void* newData, bool direct) {
    }

    /* Mark a malloced buffer as no longer needing to be freed. */
    void removeMallocedBuffer(void* buffer) {
    }

    using SweepThunk = void (*)(void *data);

    MOZ_MUST_USE bool queueDictionaryModeObjectToSweep(NativeObject* obj) { return true; }

    size_t sizeOfHeapCommitted() const {
        return 0;
    }
    size_t sizeOfMallocedBuffers(mozilla::MallocSizeOf mallocSizeOf) const {
        return 0;
    }

  private:
    /*
     * The start and end pointers are stored under the runtime so that we can
     * inline the isInsideNursery check into embedder code. Use the start()
     * and heapEnd() functions to access these values.
     */
    JSRuntime* runtime_;

    void* addressOfCurrentEnd() const {
        return nullptr;
    }

	void* addressOfPosition() const { return nullptr; }

    friend class jit::MacroAssembler;
};

} /* namespace js */

#endif /* gc_Nursery_h */
