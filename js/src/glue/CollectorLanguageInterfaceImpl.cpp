/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#define DEBUG 1 
#define TRACING 1 
#define EXPORT_JS_API 
#define topsrcdir /team/jasonhal/spidermonkey/gecko-dev
#define CPP_THROW_NEW 'throw()' 
#define D_INO d_ino 
#define E10S_TESTING_ONLY 1 
#define EDITLINE 1 
#define ENABLE_INTL_API 1 
#define ENABLE_TESTS 1 
#define EXPOSE_INTL_API 1 
#define GTEST_HAS_RTTI 0 
#define HAVE_64BIT_BUILD 1 
#define HAVE_ALLOCA_H 1 
#define HAVE_CPP_AMBIGUITY_RESOLVING_USING 1 
#define HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR 1 
#define HAVE_CPUID_H 1 
#define HAVE_DIRENT_H 1 
#define HAVE_GETC_UNLOCKED 1 
#define HAVE_GETOPT_H 1 
#define HAVE_GMTIME_R 1 
#define HAVE_I18N_LC_MESSAGES 1 
#define HAVE_INTTYPES_H 1 
#define HAVE_LANGINFO_CODESET 1 
#define HAVE_LIBM 1 
#define HAVE_LOCALECONV 1 
#define HAVE_LOCALTIME_R 1 
#define HAVE_MACHINE_ENDIAN_H 1 
#define HAVE_MBRTOWC 1 
#define HAVE_NETINET_IN_H 1 
#define HAVE_NL_TYPES_H 1 
#define HAVE_POSIX_MEMALIGN 1 
#define HAVE_PTHREAD_GETNAME_NP 1 
#define HAVE_SETLOCALE 1 
#define HAVE_SINCOS 1 
#define HAVE_SSIZE_T 1 
#define HAVE_STDINT_H 1 
#define HAVE_STRNDUP 1 
#define HAVE_SYS_MOUNT_H 1 
#define HAVE_SYS_QUEUE_H 1 
#define HAVE_SYS_STATVFS_H 1 
#define HAVE_SYS_TYPES_H 1 
#define HAVE_THREAD_TLS_KEYWORD 1 
#define HAVE_TM_ZONE_TM_GMTOFF 1 
#define HAVE_UNISTD_H 1 
#define HAVE_VALLOC 1 
#define HAVE_VA_COPY 1 
#define HAVE_VA_LIST_AS_ARRAY 1 
#define HAVE_VISIBILITY_ATTRIBUTE 1 
#define HAVE_VISIBILITY_HIDDEN_ATTRIBUTE 1 
#define HAVE_WCRTOMB 1 
#define HAVE___CXA_DEMANGLE 1 
#define JS_CODEGEN_NONE 1 
#define JS_CPU_X64 1 
#define JS_DEBUG 1 
#define JS_DEFAULT_JITREPORT_GRANULARITY 3 
#define JS_GC_ZEAL 1 
#define JS_HAVE_MACHINE_ENDIAN_H 1 
#define JS_JITSPEW 1 
#define JS_POSIX_NSPR 1 
#define JS_PUNBOX64 1 
#define JS_TRACE_LOGGING 1 
#define MALLOC_H '<malloc/malloc.h>' 
#define MALLOC_USABLE_SIZE_CONST_PTR const 
#define MOZILLA_UAVERSION '"52.0"' 
#define MOZILLA_VERSION '"52.0a1"' 
#define MOZILLA_VERSION_U 52.0a1 
#define MOZJS_MAJOR_VERSION 52 
#define MOZJS_MINOR_VERSION 0 
#define MOZ_BUILD_APP js 
#define MOZ_DLL_SUFFIX '".dylib"' 
#define MOZ_MEMORY 1 
#define MOZ_MEMORY_DARWIN 1 
#define MOZ_MEMORY_DEBUG 1 
#define MOZ_REFLOW_PERF 1 
#define MOZ_REFLOW_PERF_DSP 1 
#define MOZ_UPDATE_CHANNEL default 
#define NIGHTLY_BUILD 1 
#define NO_NSPR_10_SUPPORT 1 
#define SPIDERMONKEY_PROMISE 1 
#define STDC_HEADERS 1 
#define U_STATIC_IMPLEMENTATION 1 
#define U_USING_ICU_NAMESPACE 0 
#define VA_COPY va_copy 
#define XP_DARWIN 1 
#define XP_MACOSX 1 
#define XP_UNIX 1 
#define X_DISPLAY_MISSING 1 
#define AB_CD  '/Users/rwyoung/wsp/gecko-dev/js/src/jsapi-tests/jsapi-tests-gdb.py.in' -o '../../../js/src/jsapi-tests/jsapi-tests-gdb.py'

#define DEBUG 1
#define JS_CODEGEN_X64 1
#define OMR 1

#include "modronbase.h"

#include "CollectorLanguageInterfaceImpl.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentSafepointCallback.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#if defined(OMR_GC_MODRON_COMPACTION)
#include "CompactScheme.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#include "EnvironmentStandard.hpp"
#include "ForwardedHeader.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "MarkingScheme.hpp"
#include "mminitcore.h"
#include "objectdescription.h"
#include "ObjectModel.hpp"
#include "omr.h"
#include "omrvm.h"
#include "OMRVMInterface.hpp"
#include "Scavenger.hpp"
#include "SlotObject.hpp"

/// Spidermonkey Headers
#include "js/TracingAPI.h"

// JS
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ReentrancyGuard.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TypeTraits.h"

#include "jsgc.h"
#include "jsprf.h"

#include "builtin/ModuleObject.h"
#include "gc/GCInternals.h"
#include "gc/Policy.h"
#include "jit/IonCode.h"
#include "js/SliceBudget.h"
#include "vm/ArgumentsObject.h"
#include "vm/ArrayObject.h"
#include "vm/Debugger.h"
#include "vm/EnvironmentObject.h"
#include "vm/Scope.h"
#include "vm/Shape.h"
#include "vm/Symbol.h"
#include "vm/TypedArrayObject.h"
#include "vm/UnboxedObject.h"
// #include "wasm/WasmJS.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "gc/Nursery-inl.h"
#include "vm/String-inl.h"
#include "vm/UnboxedObject-inl.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"

// OMR
#include "omrglue.hpp"

#include "CollectorLanguageInterfaceImpl.hpp"
#include "Dispatcher.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "MarkingScheme.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectMap.hpp"
#include "OMRVMInterface.hpp"
#include "Task.hpp"

using namespace js;
using namespace JS;
using namespace js::gc;

namespace omrjs {

using JS::MapTypeToTraceKind;

using mozilla::ArrayLength;
using mozilla::DebugOnly;
using mozilla::IsBaseOf;
using mozilla::IsSame;
using mozilla::MakeRange;
using mozilla::PodCopy;

OMRGCMarker::OMRGCMarker(JSRuntime* rt, MM_EnvironmentBase* env, MM_MarkingScheme* ms)
	: JSTracer(rt, JSTracer::TracerKindTag::OMR_SCAN, ExpandWeakMaps),
	  _env(env),
	  _markingScheme(ms) {
}

} // namespace omrjs

/* This enum extends ConcurrentStatus with values > CONCURRENT_ROOT_TRACING. Values from this
 * and from ConcurrentStatus are treated as uintptr_t values everywhere except when used as
 * case labels in switch() statements where manifest constants are required.
 * 
 * ConcurrentStatus extensions allow the client language to define discrete units of work
 * that can be executed in parallel by concurrent threads. ConcurrentGC will call 
 * MM_CollectorLanguageInterfaceImpl::concurrentGC_collectRoots(..., concurrentStatus, ...)
 * only once with each client-defined status value. The thread that receives the call
 * can check the concurrentStatus value to select and execute the appropriate unit of work.
 */
enum {
	CONCURRENT_ROOT_TRACING1 = ((uintptr_t)((uintptr_t)CONCURRENT_ROOT_TRACING + 1))
};

/**
 * Initialization
 */
MM_CollectorLanguageInterfaceImpl *
MM_CollectorLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_CollectorLanguageInterfaceImpl *cli = NULL;
	OMR_VM *omrVM = env->getOmrVM();
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	cli = (MM_CollectorLanguageInterfaceImpl *)extensions->getForge()->allocate(sizeof(MM_CollectorLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != cli) {
		new(cli) MM_CollectorLanguageInterfaceImpl(omrVM);
		if (!cli->initialize(omrVM)) {
			cli->kill(env);
			cli = NULL;
		}
	}

	return cli;
}

void
MM_CollectorLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	OMR_VM *omrVM = env->getOmrVM();
	tearDown(omrVM);
	MM_GCExtensionsBase::getExtensions(omrVM)->getForge()->free(this);
}

void
MM_CollectorLanguageInterfaceImpl::tearDown(OMR_VM *omrVM)
{

}

bool
MM_CollectorLanguageInterfaceImpl::initialize(OMR_VM *omrVM)
{
	return true;
}

void
MM_CollectorLanguageInterfaceImpl::flushNonAllocationCaches(MM_EnvironmentBase *env)
{
}

OMR_VMThread *
MM_CollectorLanguageInterfaceImpl::attachVMThread(OMR_VM *omrVM, const char *threadName, uintptr_t reason)
{
	OMR_VMThread *omrVMThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	rc = OMR_Glue_BindCurrentThread(omrVM, threadName, &omrVMThread);
	if (OMR_ERROR_NONE != rc) {
		return NULL;
	}
	return omrVMThread;
}

void
MM_CollectorLanguageInterfaceImpl::detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrVMThread, uintptr_t reason)
{
	if (NULL != omrVMThread) {
		OMR_Glue_UnbindCurrentThread(omrVMThread);
	}
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_masterSetupForGC(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_scanRoots(MM_EnvironmentBase *env)
{
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		OMR_VM *omrVM = env->getOmrVM();
		JSRuntime *rt = (JSRuntime *)omrVM->_language_vm;
		if (NULL == _omrGCMarker) {
			MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

			_omrGCMarker = (omrjs::OMRGCMarker *)extensions->getForge()->allocate(sizeof(omrjs::OMRGCMarker), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
			new (_omrGCMarker) omrjs::OMRGCMarker(rt, env, _markingScheme);
		}

		gcstats::AutoPhase ap(rt->gc.stats, gcstats::PHASE_MARK_ROOTS);
		js::gc::AutoTraceSession session(rt);
		rt->gc.traceRuntimeAtoms(_omrGCMarker, session.lock);
		// JSCompartment::traceIncomingCrossCompartmentEdgesForZoneGC(trc);
		rt->gc.traceRuntimeCommon(_omrGCMarker, js::gc::GCRuntime::TraceOrMarkRuntime::TraceRuntime, session.lock);
	}
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_completeMarking(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_markLiveObjectsComplete(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_masterSetupForWalk(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_masterCleanupAfterGC(MM_EnvironmentBase *env)
{
}

struct TraceChildrenFunctor {
	MM_CollectorLanguageInterfaceImpl *cli;

	template <typename T>
	void operator()(T* thing) {
		static_cast<T*>(thing)->traceChildren(cli->_omrGCMarker);
	}
};


uintptr_t
MM_CollectorLanguageInterfaceImpl::markingScheme_scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason)
{
	TraceChildrenFunctor traceChildren = {this};
	if (JS::TraceKind::Null != ((Cell *)objectPtr)->getTraceKind()) {
		DispatchTraceKindTyped(traceChildren, (Cell *)objectPtr, ((Cell *)objectPtr)->getTraceKind());
	}
	return 0;
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
uintptr_t
MM_CollectorLanguageInterfaceImpl::markingScheme_scanObjectWithSize(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason, uintptr_t sizeToDo)
{
#error implement an object scanner which scans up to sizeToDo bytes
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

void
MM_CollectorLanguageInterfaceImpl::parallelDispatcher_handleMasterThread(OMR_VMThread *omrVMThread)
{
	/* Do nothing for now.  only required for SRT */
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_reportObjectEvents(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterSetupForGC(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, uint32_t *gcCode)
{
#error Return true if scavenge cycle should be forgone and GC cycle percolated up to another collector
}

GC_ObjectScanner *
MM_CollectorLanguageInterfaceImpl::scavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
{
#error Implement a GC_ObjectScanner subclass for each distinct kind of objects (eg scanner for scalar objects and scanner for indexable objects)
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_flushReferenceObjects(MM_EnvironmentStandard *env)
{
	/* Do nothing for now */
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented and return true an object may hold any object references that are live
	 * but not reachable by traversing the reference graph from the root set or remembered set. Otherwise this
	 * default implementation should be used.
	 */
	return false;
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should
	 * call MM_Scavenger::copyObjectSlot(..) for each such indirect object reference, ORing the boolean result
	 * from each call to MM_Scavenger::copyObjectSlot(..) into a single boolean value to be returned.
	 */
	return false;
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should
	 * call MM_Scavenger::backOutFixSlotWithoutCompression(..) for each uncompressed slot holding a reference to
	 * an indirect object that is associated with the object.
	 */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_backOutIndirectObjects(MM_EnvironmentStandard *env)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should locate
	 * all such objects and call MM_Scavenger::backOutObjectScan(..) for each such object that is in the remembered
	 * set. For example,
	 *
	 * if (_extensions->objectModel.isRemembered(indirectObject)) {
	 *    _extensions->scavenger->backOutObjectScan(env, indirectObject);
	 * }
	 */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader)
{
	/* This method must restore the object header slot (and overlapped slot, if header is compressed)
	 * in the original object and install a reverse forwarded object in the forwarding location. 
	 * A reverse forwarded object is a hole (MM_HeapLinkedFreeHeader) whose 'next' pointer actually 
	 * points at the original object. This keeps tenure space walkable once the reverse forwarded 
	 * objects are abandoned.
	 */
}

#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, MM_MemorySubSpaceSemiSpace *subSpaceNew)
{
	/* This method must be implemented if (and only if) the object header is stored in a compressed slot. in that
	 * case the other half of the full (omrobjectptr_t sized) slot may hold a compressed object reference that
	 * must be restored by this method.
	 */
	Assert_MM_unimplemented();
}
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(OMR_GC_MODRON_COMPACTION)
void
MM_CollectorLanguageInterfaceImpl::compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap)
{
	Assert_MM_unimplemented();
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme)
{
	Assert_MM_unimplemented();
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env)
{
	Assert_MM_unimplemented();
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env)
{
	Assert_MM_unimplemented();
}
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
MM_ConcurrentSafepointCallback*
MM_CollectorLanguageInterfaceImpl::concurrentGC_createSafepointCallback(MM_EnvironmentBase *env)
{
	MM_EnvironmentStandard *envStd = MM_EnvironmentStandard::getEnvironment(env);
	return MM_ConcurrentSafepointCallback::newInstance(envStd);
}

uintptr_t
MM_CollectorLanguageInterfaceImpl::concurrentGC_getNextTracingMode(uintptr_t executionMode)
{
	uintptr_t nextExecutionMode = CONCURRENT_TRACE_ONLY;
	switch (executionMode) {
	case CONCURRENT_ROOT_TRACING:
		nextExecutionMode = CONCURRENT_ROOT_TRACING1;
		break;
	case CONCURRENT_ROOT_TRACING1:
		nextExecutionMode = CONCURRENT_TRACE_ONLY;
		break;
	default:
		Assert_MM_unreachable();
	}

	return nextExecutionMode;
}

uintptr_t
MM_CollectorLanguageInterfaceImpl::concurrentGC_collectRoots(MM_EnvironmentStandard *env, uintptr_t concurrentStatus, bool *collectedRoots, bool *paidTax)
{
	uintptr_t bytesScanned = 0;
	*collectedRoots = true;
	*paidTax = true;

	switch (concurrentStatus) {
	case CONCURRENT_ROOT_TRACING1:
		markingScheme_scanRoots(env);
		break;
	default:
		Assert_MM_unreachable();
	}

	return bytesScanned;
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

omrobjectptr_t
MM_CollectorLanguageInterfaceImpl::heapWalker_heapWalkerObjectSlotDo(omrobjectptr_t object)
{
	return NULL;
}
void
MM_CollectorLanguageInterfaceImpl::parallelGlobalGC_postMarkProcessing(MM_EnvironmentBase *env)
{
	/* This puts the heap into the state required to walk it */
	GC_OMRVMInterface::flushCachesForGC(env);

	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);

	/* Walk the heap, for objects that are not marked we corrupt them to maximize the chance we will crash immediately
	if they are used.  For live objects validate that they have the expected eyecatcher */
	MM_HeapRegionDescriptor *hrd = regionIterator.nextRegion();
	while (NULL != hrd) {
		/* Walk all of the objects, making sure that those that were not marked are no longer
		usable. If they are later used we will know this and optimally crash */
		GC_ObjectHeapIteratorAddressOrderedList objectIterator(_extensions, hrd, false);
		omrobjectptr_t omrobjPtr = objectIterator.nextObject();
		while (NULL != omrobjPtr) {
			if (!_markingScheme->isMarked(omrobjPtr)) {
				/* object will be collected. We write the full contents of the object with a known value. */
				uintptr_t objsize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(omrobjPtr);
				memset(omrobjPtr, 0x5E, (size_t)objsize);
				MM_HeapLinkedFreeHeader::fillWithHoles(omrobjPtr, objsize);
			}
			omrobjPtr = objectIterator.nextObject();
		}
		hrd = regionIterator.nextRegion();
	}
}
