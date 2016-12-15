/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgcinlines_h
#define jsgcinlines_h

#include "jsgc.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"

#include "gc/GCTrace.h"
#include "gc/Zone.h"

namespace js {
namespace gc {

inline void
MakeAccessibleAfterMovingGC(void* anyp) {}

inline void
MakeAccessibleAfterMovingGC(JSObject* obj) {
}

static inline AllocKind
GetGCObjectKind(const Class* clasp)
{
     if (clasp == FunctionClassPtr)
         return AllocKind::FUNCTION;
     uint32_t nslots = JSCLASS_RESERVED_SLOTS(clasp);
     if (clasp->flags & JSCLASS_HAS_PRIVATE)
         nslots++;
     return GetGCObjectKind(nslots);
}

inline void
GCRuntime::poke()
{
}

#ifndef OMR // Arenas

class ArenaIter
{
  public:
    ArenaIter() {}

    ArenaIter(JS::Zone* zone, AllocKind kind) { init(zone, kind); }

    void init(JS::Zone* zone, AllocKind kind) {
    }

    bool done() const {
        return true;
    }

    Arena* get() const {
        return nullptr;
    }

    void next() {
    }
};

class ArenaCellIterImpl
{
  public:
    ArenaCellIterImpl() {}

    explicit ArenaCellIterImpl(Arena* arena) { init(arena); }

    void init(Arena* arena) {
    }

    // Use this to move from an Arena of a particular kind to another Arena of
    // the same kind.
    void reset(Arena* arena) {
    }

    bool done() const {
        return true;
    }

    TenuredCell* getCell() const {
        return nullptr;
    }

    template<typename T> T* get() const {
        return nullptr;
    }

    void next() {
    }
};

template<>
JSObject*
ArenaCellIterImpl::get<JSObject>() const;

class ArenaCellIterUnderGC : public ArenaCellIterImpl
{
  public:
    explicit ArenaCellIterUnderGC(Arena* arena) {
    }
};

class ArenaCellIterUnderFinalize : public ArenaCellIterImpl
{
  public:
    explicit ArenaCellIterUnderFinalize(Arena* arena) {}
};

#endif // ! OMR Arenas

template <typename T>
class ZoneCellIter;

template <>
class ZoneCellIter<TenuredCell> {

  protected:
    // For use when a subclass wants to insert some setup before init().
    ZoneCellIter() {}

    void init(JS::Zone* zone, AllocKind kind) {
    }

  public:
    ZoneCellIter(JS::Zone* zone, AllocKind kind) {
    }

    ZoneCellIter(JS::Zone* zone, AllocKind kind, const js::gc::AutoAssertEmptyNursery&) {
    }

    bool done() const {
        return true;
    }

    template<typename T>
    T* get() const {
        return nullptr;
    }

    TenuredCell* getCell() const {
        return nullptr;
    }

    void next() {
    }
};

// Iterator over the cells in a Zone, where the GC type (JSString, JSObject) is
// known, for a single AllocKind. Example usages:
//
//   for (auto obj = zone->cellIter<JSObject>(AllocKind::OBJECT0); !obj.done(); obj.next())
//       ...
//
//   for (auto script = zone->cellIter<JSScript>(); !script.done(); script.next())
//       f(script->code());
//
// As this code demonstrates, you can use 'script' as if it were a JSScript*.
// Its actual type is ZoneCellIter<JSScript>, but for most purposes it will
// autoconvert to JSScript*.
//
// Note that in the JSScript case, ZoneCellIter is able to infer the AllocKind
// from the type 'JSScript', whereas in the JSObject case, the kind must be
// given (because there are multiple AllocKinds for objects).
//
// Also, the static rooting hazard analysis knows that the JSScript case will
// not GC during construction. The JSObject case needs to GC, or more precisely
// to empty the nursery and clear out the store buffer, so that it can see all
// objects to iterate over (the nursery is not iterable) and remove the
// possibility of having pointers from the store buffer to data hanging off
// stuff we're iterating over that we are going to delete. (The latter should
// not be a problem, since such instances should be using RelocatablePtr do
// remove themselves from the store buffer on deletion, but currently for
// subtle reasons that isn't good enough.)
//
// If the iterator is used within a GC, then there is no need to evict the
// nursery (again). You may select a variant that will skip the eviction either
// by specializing on a GCType that is never allocated in the nursery, or
// explicitly by passing in a trailing AutoAssertEmptyNursery argument.
//
template <typename GCType>
class ZoneCellIter : public ZoneCellIter<TenuredCell> {
  public:
    // Non-nursery allocated (equivalent to having an entry in
    // MapTypeToFinalizeKind). The template declaration here is to discard this
    // constructor overload if MapTypeToFinalizeKind<GCType>::kind does not
    // exist. Note that there will be no remaining overloads that will work,
    // which makes sense given that you haven't specified which of the
    // AllocKinds to use for GCType.
    //
    // If we later add a nursery allocable GCType with a single AllocKind, we
    // will want to add an overload of this constructor that does the right
    // thing (ie, it empties the nursery before iterating.)
    explicit ZoneCellIter(JS::Zone* zone) : ZoneCellIter<TenuredCell>() {
    }

    // Non-nursery allocated, nursery is known to be empty: same behavior as above.
    ZoneCellIter(JS::Zone* zone, const js::gc::AutoAssertEmptyNursery&) : ZoneCellIter(zone) {
    }

    // Arbitrary kind, which will be assumed to be nursery allocable (and
    // therefore the nursery will be emptied before iterating.)
    ZoneCellIter(JS::Zone* zone, AllocKind kind) : ZoneCellIter<TenuredCell>(zone, kind) {
    }

    // Arbitrary kind, which will be assumed to be nursery allocable, but the
    // nursery is known to be empty already: same behavior as non-nursery types.
    ZoneCellIter(JS::Zone* zone, AllocKind kind, const js::gc::AutoAssertEmptyNursery& empty)
      : ZoneCellIter<TenuredCell>(zone, kind, empty)
    {
    }

    GCType* get() const { return nullptr; }
    operator GCType*() const { return nullptr; }
    GCType* operator ->() const { return nullptr; }
};

class GCZonesIter
{
  public:
    explicit GCZonesIter(JSRuntime* rt, ZoneSelector selector = WithAtoms) {
    }

    bool done() const { return true; }

    void next() {
    }

    JS::Zone* get() const {
        return nullptr;
    }

    operator JS::Zone*() const { return get(); }
    JS::Zone* operator->() const { return get(); }
};

typedef CompartmentsIterT<GCZonesIter> GCCompartmentsIter;

/* Iterates over all zones in the current zone group. */
class GCZoneGroupIter {
  public:
    explicit GCZoneGroupIter(JSRuntime* rt) {
    }

    bool done() const { return true; }

    void next() {
    }

    JS::Zone* get() const {
        return nullptr;
    }

    operator JS::Zone*() const { return get(); }
    JS::Zone* operator->() const { return get(); }
};

typedef CompartmentsIterT<GCZoneGroupIter> GCCompartmentGroupIter;

} /* namespace gc */
} /* namespace js */

#endif /* jsgcinlines_h */
