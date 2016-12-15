/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Nursery_inl_h
#define gc_Nursery_inl_h

#include "gc/Nursery.h"

#include "jscntxt.h"

#include "gc/Heap.h"
#include "gc/Zone.h"
#include "js/TracingAPI.h"
#include "vm/Runtime.h"

#include "omrgc.h"

namespace js {

// The allocation methods below will not run the garbage collector. If the
// nursery cannot accomodate the allocation, the malloc heap will be used
// instead.

// OMRTODO: Use nursery.allocateBuffer
template <typename T>
static inline T*
AllocateObjectBuffer(ExclusiveContext* cx, uint32_t count)
{
	return (T *)OMR_GC_AllocateNoGC(Nursery::omrVMThread, 0, sizeof(T) * count, 0);
}

template <typename T>
static inline T*
AllocateObjectBuffer(ExclusiveContext* cx, JSObject* obj, uint32_t count)
{
    return AllocateObjectBuffer<T>(cx, count);
}

// If this returns null then the old buffer will be left alone.
template <typename T>
static inline T*
ReallocateObjectBuffer(ExclusiveContext* cx, JSObject* obj, T* oldBuffer,
                       uint32_t oldCount, uint32_t newCount)
{
	T *newBuffer = (T *)AllocateObjectBuffer<T>(cx, newCount);
	memcpy(newBuffer, oldBuffer, sizeof(T) * oldCount);
    return newBuffer;
}

} // namespace js

#endif /* gc_Nursery_inl_h */
