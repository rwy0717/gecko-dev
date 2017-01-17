#if!defined(OMRGLUE_HPP_)
#define OMRGLUE_HPP_

#include <iostream>

#include "CollectorLanguageInterfaceImpl.hpp"
#include "MarkingScheme.hpp"

#include "js/TracingAPI.h"

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

// OMR
#include "omr/gc/base/MarkingScheme.hpp"
#include "omr/gc/base/EnvironmentBase.hpp"

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
} // namespace js

namespace omrjs {

using namespace js;
using namespace JS;

class OMRGCMarker : public JSTracer
{
public:
    explicit OMRGCMarker(JSRuntime* rt, MM_EnvironmentBase* env, MM_MarkingScheme* ms);

    template <typename T>
    bool mark(T* thing) {
        std::cout << "** (" << env_ << ") marking: " << thing << std::endl;
        return ms_->markObject(env_, (omrobjectptr_t)thing);
    }

    // Mark the given GC thing and traverse its children at some point.
    template <typename T> void traverse(T thing);

    // Calls traverse on target after making additional assertions.
    template <typename S, typename T> void traverseEdge(S source, T* target);
    template <typename S, typename T> void traverseEdge(S source, const T& target);

    // Notes a weak graph edge for later sweeping.
    template <typename T> void noteWeakEdge(T* edge);

    static OMRGCMarker* fromTracer(JSTracer* trc) {
        return static_cast<OMRGCMarker*>(trc);
    }

private:
    MM_EnvironmentBase* env_;
    MM_MarkingScheme* ms_;

};

} // namespace omrjs

#endif // OMRGLUE_HPP_