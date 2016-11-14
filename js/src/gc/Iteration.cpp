/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "jsgc.h"

#include "gc/GCInternals.h"
#include "js/HashTable.h"
#include "vm/Runtime.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

void
js::IterateZonesCompartmentsArenasCells(JSContext* cx, void* data,
                                        IterateZoneCallback zoneCallback,
                                        JSIterateCompartmentCallback compartmentCallback,
                                        IterateArenaCallback arenaCallback,
                                        IterateCellCallback cellCallback)
{
}

void
js::IterateZoneCompartmentsArenasCells(JSContext* cx, Zone* zone, void* data,
                                       IterateZoneCallback zoneCallback,
                                       JSIterateCompartmentCallback compartmentCallback,
                                       IterateArenaCallback arenaCallback,
                                       IterateCellCallback cellCallback)
{
}

void
js::IterateChunks(JSContext* cx, void* data, IterateChunkCallback chunkCallback)
{
}

void
js::IterateScripts(JSContext* cx, JSCompartment* compartment,
                   void* data, IterateScriptCallback scriptCallback)
{
}