
#ifndef gc_Marking_h
#define gc_Marking_h

#include "mozilla/HashFunctions.h"
#include "mozilla/Move.h"

#include "jsfriendapi.h"

#include "ds/OrderedHashTable.h"
#include "gc/Heap.h"
#include "gc/Tracer.h"
#include "js/GCAPI.h"
#include "js/HeapAPI.h"
#include "js/SliceBudget.h"
#include "js/TracingAPI.h"
#include "vm/TaggedProto.h"

class JSLinearString;
class JSRope;
namespace js {
class BaseShape;
class GCMarker;
class LazyScript;
class NativeObject;
class ObjectGroup;
class WeakMapBase;
namespace gc {
class Arena;
} // namespace gc
namespace jit {
class JitCode;
} // namespace jit

static const size_t INCREMENTAL_MARK_STACK_BASE_CAPACITY = 32768;

namespace gc {

struct WeakKeyTableHashPolicy {
    typedef JS::GCCellPtr Lookup;
    static HashNumber hash(const Lookup& v) { return mozilla::HashGeneric(v.asCell()); }
    static bool match(const JS::GCCellPtr& k, const Lookup& l) { return k == l; }
    static bool isEmpty(const JS::GCCellPtr& v) { return !v; }
    static void makeEmpty(JS::GCCellPtr* vp) { *vp = nullptr; }
};

struct WeakMarkable {
    WeakMapBase* weakmap;
    JS::GCCellPtr key;

    WeakMarkable(WeakMapBase* weakmapArg, JS::GCCellPtr keyArg)
      : weakmap(weakmapArg), key(keyArg) {}
};

using WeakEntryVector = Vector<WeakMarkable, 2, js::SystemAllocPolicy>;

using WeakKeyTable = OrderedHashMap<JS::GCCellPtr,
                                    WeakEntryVector,
                                    WeakKeyTableHashPolicy,
                                    js::SystemAllocPolicy>;
} /* namespace gc */

class GCMarker : public JSTracer
{
  public:
    explicit GCMarker(JSRuntime* rt) : JSTracer::JSTracer(rt, (JSTracer::TracerKindTag)0, (WeakMapTraceKind)0) {}
    MOZ_MUST_USE bool init(JSGCMode gcMode) { return true; }

    void start() {}
    void stop() {}
    void reset() {}

    // Mark the given GC thing and traverse its children at some point.
    template <typename T> void traverse(T thing) {}

    void abortLinearWeakMarking() {
    }
	
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const { return 0; }

#ifdef DEBUG
    bool shouldCheckCompartments() { return false; }
#endif

    void markEphemeronValues(gc::Cell* markedCell, gc::WeakEntryVector& entry);
};

namespace gc {

template <typename T>
bool
IsMarkedUnbarriered(T* thingp) {
	return true;
}

template <typename T>
bool
IsMarked(WriteBarrieredBase<T>* thingp) {
	return true;
}

template <typename T>
bool
IsAboutToBeFinalizedUnbarriered(T* thingp) {
	return true;
}

template <typename T>
bool
IsAboutToBeFinalized(WriteBarrieredBase<T>* thingp) {
	return true;
}

template <typename T>
bool
IsAboutToBeFinalized(ReadBarrieredBase<T>* thingp) {
	return true;
}

inline Cell*
ToMarkable(const Value& v)
{
    return nullptr;
}

inline Cell*
ToMarkable(Cell* cell)
{
    return cell;
}

// Return true if the pointer is nullptr, or if it is a tagged pointer to
// nullptr.
MOZ_ALWAYS_INLINE bool
IsNullTaggedPointer(void* p)
{
    return uintptr_t(p) <= LargestTaggedNullCellPointer;
}

} /* namespace gc */

// The return value indicates if anything was unmarked.
inline bool
UnmarkGrayShapeRecursively(Shape* shape) {
	return true;
}

} /* namespace js */

#endif /* gc_marking_h */