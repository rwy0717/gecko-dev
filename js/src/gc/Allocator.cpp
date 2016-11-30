/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Allocator.h"

#include "jscntxt.h"

#include "gc/GCInternals.h"
#include "gc/GCTrace.h"
#include "gc/Nursery.h"
#include "jit/JitCompartment.h"
#include "vm/Runtime.h"
#include "vm/String.h"

#include "jsobjinlines.h"

#include "gc/Heap-inl.h"

using namespace js;
using namespace gc;

// OMRTODO: Allocate space for slots!!

template <typename T, AllowGC allowGC = CanGC>
T*
js::Allocate(ExclusiveContext* cx) {
	JSContext* ncx = cx->asJSContext();
	JSRuntime* rt = ncx->runtime();
	return (T*)rt->gc.nursery.allocateObject(ncx, sizeof(T), 0, nullptr);
}
template JSObject* js::Allocate<JSObject, NoGC>(ExclusiveContext* cx, gc::AllocKind kind,
                                                size_t nDynamicSlots, gc::InitialHeap heap,
                                                const Class* clasp);
template JSObject* js::Allocate<JSObject, CanGC>(ExclusiveContext* cx, gc::AllocKind kind,
                                                 size_t nDynamicSlots, gc::InitialHeap heap,
                                                 const Class* clasp);

template <typename T, AllowGC allowGC = CanGC>
JSObject*
js::Allocate(ExclusiveContext* cx, gc::AllocKind kind, size_t nDynamicSlots, gc::InitialHeap heap,
         const Class* clasp) {
	JSContext* ncx = cx->asJSContext();
	JSRuntime* rt = ncx->runtime();
	return rt->gc.nursery.allocateObject(ncx, Arena::thingSize(kind), nDynamicSlots, clasp);
}

#define DECL_ALLOCATOR_INSTANCES(allocKind, traceKind, type, sizedType) \
    template type* js::Allocate<type, NoGC>(ExclusiveContext* cx);\
    template type* js::Allocate<type, CanGC>(ExclusiveContext* cx);
FOR_EACH_NONOBJECT_ALLOCKIND(DECL_ALLOCATOR_INSTANCES)
#undef DECL_ALLOCATOR_INSTANCES