/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Tracer.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/SizePrintfMacros.h"

#include "jsapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsutil.h"
#include "NamespaceImports.h"
#include "gc/GCInternals.h"
#include "gc/Marking.h"
#include "gc/Zone.h"

#include "vm/Shape.h"
#include "vm/Symbol.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"

#include "vm/ObjectGroup-inl.h"

using namespace js;
using namespace js::gc;
using mozilla::DebugOnly;

void
JS::CallbackTracer::getTracingEdgeName(char* buffer, size_t bufferSize)
{
}


/*** Public Tracing API **************************************************************************/

JS_PUBLIC_API(void)
JS::TraceChildren(JSTracer* trc, GCCellPtr thing)
{
}

JS_PUBLIC_API(void)
JS::TraceIncomingCCWs(JSTracer* trc, const JS::CompartmentSet& compartments)
{
}

JS_PUBLIC_API(void)
JS_GetTraceThingInfo(char* buf, size_t bufsize, JSTracer* trc, void* thing,
                     JS::TraceKind kind, bool details)
{
}

JS::CallbackTracer::CallbackTracer(JSContext* cx, WeakMapTraceKind weakTraceKind)
  : CallbackTracer(cx->runtime(), weakTraceKind)
{}