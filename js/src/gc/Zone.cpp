/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone.h"

#include "jsgc.h"

#include "gc/Policy.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/JitCompartment.h"
#include "vm/Debugger.h"
#include "vm/Runtime.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

JS::Zone::Zone(JSRuntime* rt)
  : JS::shadow::Zone(rt, nullptr),
    suppressAllocationMetadataBuilder(false),
#ifndef OMR
    arenas(rt),
#endif
    types(this),
	gcWeakMapList(),
    compartments(),
    gcGrayRoots(),
    typeDescrObjects(this, SystemAllocPolicy()),
    gcMallocBytes(0),
    gcMallocGCTriggered(false),
    usage(&rt->gc.usage),
    gcDelayBytes(0),
    propertyTree(this),
    baseShapes(this, BaseShapeSet()),
    initialShapes(this, InitialShapeSet()),
    data(nullptr),
    isSystem(false),
    usedByExclusiveThread(false),
    active(false),
    jitZone_(nullptr) {}

Zone::DebuggerVector*
Zone::getOrCreateDebuggers(JSContext* cx)
{
    return nullptr;
}

Zone::~Zone()
{
    js_delete(jitZone_);
}

JS_PUBLIC_API(void)
JS::shadow::RegisterWeakCache(JS::Zone* zone, WeakCache<void*>* cachep)
{
    zone->registerWeakCache(cachep);
}

js::jit::JitZone*
Zone::createJitZone(JSContext* cx)
{
    MOZ_ASSERT(!jitZone_);

    if (!cx->runtime()->getJitRuntime(cx))
        return nullptr;

    jitZone_ = cx->new_<js::jit::JitZone>();
    return jitZone_;
}

void
Zone::clearTables()
{
    if (baseShapes.initialized())
        baseShapes.clear();
    if (initialShapes.initialized())
        initialShapes.clear();
}
