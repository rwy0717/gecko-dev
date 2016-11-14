/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Garbage Collector. */

#ifndef gc_Policy_h
#define gc_Policy_h

#include "mozilla/TypeTraits.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/GCPolicyAPI.h"

// Forward declare the types we're defining policies for. This file is
// included in all places that define GC things, so the real definitions
// will be available when we do template expansion, allowing for use of
// static members in the underlying types. We cannot, however, use
// static_assert to verify relations between types.
class JSLinearString;
namespace js {
class AccessorShape;
class ArgumentsObject;
class ArrayBufferObject;
class ArrayBufferObjectMaybeShared;
class ArrayBufferViewObject;
class ArrayObject;
class BaseShape;
class DebugEnvironmentProxy;
class DebuggerFrame;
class ExportEntryObject;
class EnvironmentObject;
class GlobalObject;
class ImportEntryObject;
class LazyScript;
class LexicalEnvironmentObject;
class ModuleEnvironmentObject;
class ModuleNamespaceObject;
class ModuleObject;
class NativeObject;
class ObjectGroup;
class PlainObject;
class PropertyName;
class RegExpObject;
class SavedFrame;
class Scope;
class EnvironmentObject;
class ScriptSourceObject;
class Shape;
class SharedArrayBufferObject;
class StructTypeDescr;
class UnownedBaseShape;
class WasmMemoryObject;
namespace jit {
class JitCode;
} // namespace jit
} // namespace js

namespace JS {

template <typename T>
struct GCPolicy<js::HeapPtr<T>>
{
    static void trace(JSTracer* trc, js::HeapPtr<T>* thingp, const char* name) {
        js::TraceEdge(trc, thingp, name);
    }
    static bool needsSweep(js::HeapPtr<T>* thingp) {
        return false;
    }
};

template <typename T>
struct GCPolicy<js::ReadBarriered<T>>
{
    static void trace(JSTracer* trc, js::ReadBarriered<T>* thingp, const char* name) {
        js::TraceEdge(trc, thingp, name);
    }
    static bool needsSweep(js::ReadBarriered<T>* thingp) {
        return false;
    }
};

} // namespace JS

#endif // gc_Policy_h