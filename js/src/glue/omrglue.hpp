#if!defined(OMRGLUE_HPP_)
#define OMRGLUE_HPP_

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
    explicit OMRGCMarker(JSRuntime* rt);
    MOZ_MUST_USE bool init(JSGCMode gcMode);

    void start();
    void stop();
    void reset();

    // Mark the given GC thing and traverse its children at some point.
    template <typename T> void traverse(T thing);

    // Calls traverse on target after making additional assertions.
    template <typename S, typename T> void traverseEdge(S source, T* target);
    template <typename S, typename T> void traverseEdge(S source, const T& target);

    // Notes a weak graph edge for later sweeping.
    template <typename T> void noteWeakEdge(T* edge);

    static OMRGCMarker* fromTracer(JSTracer* trc) {
        // MOZ_ASSERT(trc->isMarkingTracer());
        return static_cast<OMRGCMarker*>(trc);
    }

private:
    // We may not have concrete types yet, so this has to be outside the header.
    template <typename T>
    void dispatchToTraceChildren(T* thing);

    // Mark the given GC thing, but do not trace its children. Return true
    // if the thing became marked.
    template <typename T>
    bool mark(T* thing);

};

} // namespace omrjs

#endif // OMRGLUE_HPP_