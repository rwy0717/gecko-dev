/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_StoreBuffer_h
#define gc_StoreBuffer_h

#include "mozilla/Attributes.h"
#include "mozilla/ReentrancyGuard.h"

#include <algorithm>

#include "jsalloc.h"

#include "ds/LifoAlloc.h"
#include "gc/Nursery.h"
#include "js/MemoryMetrics.h"

namespace js {
namespace gc {

class ArenaCellSet;

/*
 * BufferableRef represents an abstract reference for use in the generational
 * GC's remembered set. Entries in the store buffer that cannot be represented
 * with the simple pointer-to-a-pointer scheme must derive from this class and
 * use the generic store buffer interface.
 */
class BufferableRef
{
  public:
    virtual void trace(JSTracer* trc) = 0;
    bool maybeInRememberedSet(const Nursery&) const { return true; }
};

typedef HashSet<void*, PointerHasher<void*, 3>, SystemAllocPolicy> EdgeSet;

/*
 * The StoreBuffer observes all writes that occur in the system and performs
 * efficient filtering of them to derive a remembered set for nursery GC.
 */
class StoreBuffer
{
    friend class mozilla::ReentrancyGuard;

    template <typename Buffer, typename Edge>
    void unput(Buffer& buffer, const Edge& edge) {
    }

    template <typename Buffer, typename Edge>
    void put(Buffer& buffer, const Edge& edge) {
    }

  public:
    explicit StoreBuffer(JSRuntime* rt, const Nursery& nursery)
    {
    }

    MOZ_MUST_USE bool enable() { return true; }
    void disable() {}
    bool isEnabled() const { return false; }

    void clear() {}

    bool cancelIonCompilations() const { return false; }

    /* Insert a single edge into the buffer/remembered set. */
    void putValue(JS::Value* vp) { }
    void unputValue(JS::Value* vp) {  }
    void putCell(Cell** cellp) {  }
    void unputCell(Cell** cellp) {  }
    void putSlot(NativeObject* obj, int kind, int32_t start, int32_t count) {
    }
    inline void putWholeCell(Cell* cell);

    /* Insert an entry into the generic buffer. */
    template <typename T>
    void putGeneric(const T& t) { }

    void setShouldCancelIonCompilations() {
    }

    /* Methods to trace the source of all edges in the store buffer. */
    void traceValues(TenuringTracer& mover)            {}
    void traceCells(TenuringTracer& mover)             {}
    void traceSlots(TenuringTracer& mover)             {}
    void traceGenericEntries(JSTracer *trc)            {}

    void traceWholeCells(TenuringTracer& mover) {}
    void traceWholeCell(TenuringTracer& mover, JS::TraceKind kind, Cell* cell) {}

    /* For use by our owned buffers and for testing. */
    void setAboutToOverflow() {}

    void addToWholeCellBuffer(ArenaCellSet* set) {}

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::GCSizes* sizes) {}
};

// A set of cells in an arena used to implement the whole cell store buffer.
class ArenaCellSet
{
    friend class StoreBuffer;

  public:
    explicit ArenaCellSet(Arena* arena) {}

    bool hasCell(const TenuredCell* cell) const {
        return false;
    }

    void putCell(const TenuredCell* cell) {
    }

    bool isEmpty() const {
        return true;
    }

    bool hasCell(size_t cellIndex) const;

    void putCell(size_t cellIndex);

    void check() const;

    // Sentinel object used for all empty sets.
    static ArenaCellSet Empty;

    static size_t getCellIndex(const TenuredCell* cell);
    static void getWordIndexAndMask(size_t cellIndex, size_t* wordp, uint32_t* maskp);

    // Attempt to trigger a minor GC if free space in the nursery (where these
    // objects are allocated) falls below this threshold.
    static const size_t NurseryFreeThresholdBytes = 64 * 1024;

    static size_t offsetOfArena() {
        return 0;
    }
    static size_t offsetOfBits() {
        return 0;
    }
};

inline ArenaCellSet* AllocateWholeCellSet(Arena* arena) { return nullptr; }

} /* namespace gc */
} /* namespace js */

#endif /* gc_StoreBuffer_h */
