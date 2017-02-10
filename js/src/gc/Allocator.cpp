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
#include "vm/Symbol.h"

#include "jsobjinlines.h"

#include "gc/Heap-inl.h"

using namespace js;
using namespace gc;

namespace js {

template <>
Shape*
Allocate<Shape, CanGC>(ExclusiveContext* cx) {
	return Allocate<Shape, CanGC>(cx, gc::AllocKind::SHAPE);
}
template <>
Shape*
Allocate<Shape, NoGC>(ExclusiveContext* cx) {
	return Allocate<Shape, NoGC>(cx, gc::AllocKind::SHAPE);
}

template <>
AccessorShape*
Allocate<AccessorShape, CanGC>(ExclusiveContext* cx) {
	return Allocate<AccessorShape, CanGC>(cx, gc::AllocKind::ACCESSOR_SHAPE);
}
template <>
AccessorShape*
Allocate<AccessorShape, NoGC>(ExclusiveContext* cx) {
	return Allocate<AccessorShape, NoGC>(cx, gc::AllocKind::ACCESSOR_SHAPE);
}

template <>
BaseShape*
Allocate<BaseShape, CanGC>(ExclusiveContext* cx) {
	return Allocate<BaseShape, CanGC>(cx, gc::AllocKind::BASE_SHAPE);
}
template <>
BaseShape*
Allocate<BaseShape, NoGC>(ExclusiveContext* cx) {
	return Allocate<BaseShape, NoGC>(cx, gc::AllocKind::BASE_SHAPE);
}

template <>
JSScript*
Allocate<JSScript, CanGC>(ExclusiveContext* cx) {
	return Allocate<JSScript, CanGC>(cx, gc::AllocKind::SCRIPT);
}
template <>
JSScript*
Allocate<JSScript, NoGC>(ExclusiveContext* cx) {
	return Allocate<JSScript, NoGC>(cx, gc::AllocKind::SCRIPT);
}

template <>
JS::Symbol*
Allocate<JS::Symbol, CanGC>(ExclusiveContext* cx) {
	return Allocate<JS::Symbol, CanGC>(cx, gc::AllocKind::SYMBOL);
}
template <>
JS::Symbol*
Allocate<JS::Symbol, NoGC>(ExclusiveContext* cx) {
	return Allocate<JS::Symbol, NoGC>(cx, gc::AllocKind::SYMBOL);
}

template <>
JSString*
Allocate<JSString, CanGC>(ExclusiveContext* cx) {
	return Allocate<JSString, CanGC>(cx, gc::AllocKind::STRING);
}
template <>
JSString*
Allocate<JSString, NoGC>(ExclusiveContext* cx) {
	return Allocate<JSString, NoGC>(cx, gc::AllocKind::STRING);
}

template <>
JSFatInlineString*
Allocate<JSFatInlineString, CanGC>(ExclusiveContext* cx) {
	return Allocate<JSFatInlineString, CanGC>(cx, gc::AllocKind::FAT_INLINE_STRING);
}
template <>
JSFatInlineString*
Allocate<JSFatInlineString, NoGC>(ExclusiveContext* cx) {
	return Allocate<JSFatInlineString, NoGC>(cx, gc::AllocKind::FAT_INLINE_STRING);
}

template <>
JSExternalString*
Allocate<JSExternalString, CanGC>(ExclusiveContext* cx) {
	return Allocate<JSExternalString, CanGC>(cx, gc::AllocKind::EXTERNAL_STRING);
}
template <>
JSExternalString*
Allocate<JSExternalString, NoGC>(ExclusiveContext* cx) {
	return Allocate<JSExternalString, NoGC>(cx, gc::AllocKind::EXTERNAL_STRING);
}

template <>
js::ObjectGroup*
Allocate<js::ObjectGroup, CanGC>(ExclusiveContext* cx) {
	return Allocate<js::ObjectGroup, CanGC>(cx, gc::AllocKind::OBJECT_GROUP);
}
template <>
js::ObjectGroup*
Allocate<js::ObjectGroup, NoGC>(ExclusiveContext* cx) {
	return Allocate<js::ObjectGroup, NoGC>(cx, gc::AllocKind::OBJECT_GROUP);
}

template <typename T, AllowGC allowGC /* = CanGC */>
T*
Allocate(ExclusiveContext* cx) {
	return Allocate<T, allowGC>(cx, gc::AllocKind::FIRST);
}

template <typename T, AllowGC allowGC /* = CanGC */>
T*
Allocate(ExclusiveContext* cx, gc::AllocKind kind) {
	JSContext* ncx = cx->asJSContext();
	JSRuntime* rt = ncx->runtime();
	Cell* obj = rt->gc.nursery.allocateObject(ncx, sizeof(T), 0, nullptr, allowGC == CanGC);
	obj->setAllocKind(kind);
	return (T*)obj;
}

} // namespace js

template JSObject* js::Allocate<JSObject, NoGC>(ExclusiveContext* cx, gc::AllocKind kind,
                                                size_t nDynamicSlots, gc::InitialHeap heap,
                                                const Class* clasp);
template JSObject* js::Allocate<JSObject, CanGC>(ExclusiveContext* cx, gc::AllocKind kind,
                                                 size_t nDynamicSlots, gc::InitialHeap heap,
                                                 const Class* clasp);

template <typename T, AllowGC allowGC /* = CanGC */>
JSObject*
js::Allocate(ExclusiveContext* cx, gc::AllocKind kind, size_t nDynamicSlots, gc::InitialHeap heap,
         const Class* clasp) {
	JSContext* ncx = cx->asJSContext();
	JSRuntime* rt = ncx->runtime();
	JSObject* obj = rt->gc.nursery.allocateObject(ncx, OmrGcHelper::thingSize(kind), nDynamicSlots, clasp, allowGC == CanGC);
	obj->setAllocKind(kind);
	return obj;
}

#define DECL_ALLOCATOR_INSTANCES(allocKind, traceKind, type, sizedType) \
    template type* js::Allocate<type, NoGC>(ExclusiveContext* cx);\
    template type* js::Allocate<type, CanGC>(ExclusiveContext* cx);\
    template type* js::Allocate<type, NoGC>(ExclusiveContext* cx, gc::AllocKind);\
    template type* js::Allocate<type, CanGC>(ExclusiveContext* cx, gc::AllocKind);
FOR_EACH_NONOBJECT_ALLOCKIND(DECL_ALLOCATOR_INSTANCES)
#undef DECL_ALLOCATOR_INSTANCES