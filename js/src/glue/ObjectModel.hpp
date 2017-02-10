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

#if !defined(OBJECTMODEL_HPP_)
#define OBJECTMODEL_HPP_

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

/*
 * @ddr_namespace: default
 */

#include "ModronAssertions.h"
#include "modronbase.h"
#include "objectdescription.h"
#include "Bits.hpp"
#include "HeapLinkedFreeHeader.hpp"

#include "gc/Heap.h"

#define J9_GC_OBJECT_ALIGNMENT_IN_BYTES 0x8
#define J9_GC_MINIMUM_OBJECT_SIZE 0x10

/**
 * Define object allocation categories. These are represented in MM_AllocateInitialization
 * objects and are used in GC_ObjectModel::initializeAllocation() to determine how to
 * initialize the header of a newly allocated object.
 *
 * A similar categorization is required for each client language.
 */
#define OMR_EXAMPLE_ALLOCATION_CATEGORY 0x0

/**
 * Define structure of object slot that is to be used to represent an object's metadata. In this slot, one byte
 * must be reserved to hold flags and object age (4 bits age, 4 bits flags). The remaining bytes in this slot may
 * be used by the client language for other purposes and will not be altered by OMR.
 */
#define OMR_OBJECT_METADATA_SLOT_OFFSET		0 /* fomrobject_t offset from object header address to metadata slot */
#define OMR_OBJECT_METADATA_FLAGS_SHIFT		0
#define OMR_OBJECT_METADATA_SIZE_SHIFT		8
#define OMR_OBJECT_METADATA_FLAGS_MASK		0xFF
#define OMR_OBJECT_METADATA_AGE_MASK		0xF0
#define OMR_OBJECT_METADATA_AGE_SHIFT		4
#define OMR_OBJECT_METADATA_SLOT_EA(object)	((fomrobject_t*)(object) + OMR_OBJECT_METADATA_SLOT_OFFSET) /* fomrobject_t* pointer to metadata slot */
#define OMR_OBJECT_AGE(object)				((*(OMR_OBJECT_METADATA_SLOT_EA(object)) & OMR_OBJECT_METADATA_AGE_MASK) >> OMR_OBJECT_METADATA_AGE_SHIFT)
#define OMR_OBJECT_FLAGS(object)			(*(OMR_OBJECT_METADATA_SLOT_EA(object)) & OMR_OBJECT_METADATA_FLAGS_MASK)
#define OMR_OBJECT_SIZE(object)				(*(OMR_OBJECT_METADATA_SLOT_EA(object)) >> OMR_OBJECT_METADATA_SIZE_SHIFT)

#define OMR_OBJECT_METADATA_REMEMBERED_BITS			OMR_OBJECT_METADATA_AGE_MASK
#define OMR_OBJECT_METADATA_REMEMBERED_BITS_TO_SET	0x10 /* OBJECT_HEADER_LOWEST_REMEMBERED */
#define OMR_OBJECT_METADATA_REMEMBERED_BITS_SHIFT	OMR_OBJECT_METADATA_AGE_SHIFT

#define STATE_NOT_REMEMBERED  	0
#define STATE_REMEMBERED		(OMR_OBJECT_METADATA_REMEMBERED_BITS_TO_SET & OMR_OBJECT_METADATA_REMEMBERED_BITS)

#define OMR_TENURED_STACK_OBJECT_RECENTLY_REFERENCED	(STATE_REMEMBERED + (1 << OMR_OBJECT_METADATA_REMEMBERED_BITS_SHIFT))
#define OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED	(STATE_REMEMBERED + (2 << OMR_OBJECT_METADATA_REMEMBERED_BITS_SHIFT))

class MM_AllocateInitialization;
class MM_EnvironmentBase;
class MM_GCExtensionsBase;

/**
 * Provides information for a given object.
 * @ingroup GC_Base
 */
class GC_ObjectModel
{
/*
 * Member data and types
 */
private:
	uintptr_t _objectAlignmentInBytes; /**< Cached copy of object alignment for getting object alignment for adjusting for alignment */
	uintptr_t _objectAlignmentShift; /**< Cached copy of object alignment shift, must be log2(_objectAlignmentInBytes)  */

protected:
public:

/*
 * Member functions
 */
private:
protected:
public:
	/**
	 * Initialize the receiver, a new instance of GC_ObjectModel
	 *
	 * @return true on success, false on failure
	 */
	bool
	initialize(MM_GCExtensionsBase *extensions)
	{
		return true;
	}

	void tearDown(MM_GCExtensionsBase *extensions) {}

	MMINLINE uintptr_t
	adjustSizeInBytes(uintptr_t sizeInBytes)
	{
		sizeInBytes =  (sizeInBytes + (_objectAlignmentInBytes - 1)) & (uintptr_t)~(_objectAlignmentInBytes - 1);

#if defined(OMR_GC_MINIMUM_OBJECT_SIZE)
		if (sizeInBytes < J9_GC_MINIMUM_OBJECT_SIZE) {
			sizeInBytes = J9_GC_MINIMUM_OBJECT_SIZE;
		}
#endif /* OMR_GC_MINIMUM_OBJECT_SIZE */

		return sizeInBytes;
	}

	/**
	 * This method must be implemented to initialize the object header for a new allocation
	 * of heap memory. The MM_AllocateInitialization instance provided allows access to the
	 * MM_AllocateDescription instance used to allocate the heap memory and language-specific
	 * metadata required to initialize the object header.
	 *
	 * @param[in] env Pointer to environment for calling thread.
	 * @param[in] allocatedBytes Pointer to allocated heap space
	 * @param[in] allocateInitialization Pointer to the allocation metadata
	 */
	omrobjectptr_t
	initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization)
	{
          // TODO: Store the AllocKind into the flags field.
          return (omrobjectptr_t)allocatedBytes;
	}

	/**
	 * Returns TRUE if an object is dead, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is dead, FALSE otherwise
	 */
	MMINLINE bool
	isDeadObject(void *objectPtr)
	{
		return 0 != (*((uintptr_t *)objectPtr) & J9_GC_OBJ_HEAP_HOLE_MASK);
	}

	/**
	 * Returns TRUE if an object is a dead single slot object, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is a dead single slot object, FALSE otherwise
	 */
	MMINLINE bool
	isSingleSlotDeadObject(omrobjectptr_t objectPtr)
	{
		return J9_GC_SINGLE_SLOT_HOLE == (*((uintptr_t *)objectPtr) & J9_GC_OBJ_HEAP_HOLE_MASK);
	}

	/**
	 * Returns the size, in bytes, of a single slot dead object.
	 * @param objectPtr Pointer to an object
	 * @return The size, in bytes, of a single slot dead object
	 */
	MMINLINE uintptr_t
	getSizeInBytesSingleSlotDeadObject(omrobjectptr_t objectPtr)
	{
		return sizeof(uintptr_t);
	}

	/**
	 * Returns the size, in bytes, of a multi-slot dead object.
	 * @param objectPtr Pointer to an object
	 * @return The size, in bytes, of a multi-slot dead object
	 */
	MMINLINE uintptr_t getSizeInBytesMultiSlotDeadObject(omrobjectptr_t objectPtr)
	{
		return MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(objectPtr)->getSize();
	}

	/**
	 * Returns the size in bytes of a dead object.
	 * @param objectPtr Pointer to an object
	 * @return The size in byts of a dead object
	 */
	MMINLINE uintptr_t
	getSizeInBytesDeadObject(omrobjectptr_t objectPtr)
	{
		if(isSingleSlotDeadObject(objectPtr)) {
			return getSizeInBytesSingleSlotDeadObject(objectPtr);
		}
		return getSizeInBytesMultiSlotDeadObject(objectPtr);
	}

	MMINLINE uintptr_t
	getConsumedSizeInSlotsWithHeader(omrobjectptr_t objectPtr)
	{
		return MM_Bits::convertBytesToSlots(getConsumedSizeInBytesWithHeader(objectPtr));
	}

	MMINLINE uintptr_t
	getConsumedSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
		return adjustSizeInBytes(getSizeInBytesWithHeader(objectPtr));
	}

	MMINLINE uintptr_t
	getConsumedSizeInBytesWithHeaderForMove(omrobjectptr_t objectPtr)
	{
		return getConsumedSizeInBytesWithHeader(objectPtr);
	}

	MMINLINE uintptr_t
	getSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
          return js::gc::OmrGcHelper::thingSize(((js::gc::Cell*)objectPtr)->getAllocKind());
	}

#if defined(OMR_GC_MODRON_COMPACTION)
	/**
	 * Before objects are moved during compaction is there any language specific updates
	 * @param vmThread - the thread performing the work
	 * @param objectPtr Pointer to an object
	 */
	MMINLINE void
	preMove(OMR_VMThread* vmThread, omrobjectptr_t objectPtr)
	{

	}

	/**
	 * After objects are moved during compaction is there any language specific updates
	 * @param vmThread - the thread performing the work
	 * @param objectPtr Pointer to an object
	 */
	MMINLINE void
	postMove(OMR_VMThread* vmThread, omrobjectptr_t objectPtr)
	{
		/* do nothing */
	}
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Returns TRUE if an object is remembered, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is remembered, FALSE otherwise
	 */
	MMINLINE bool
	isRemembered(omrobjectptr_t objectPtr)
	{
		return false;
	}
#endif /* OMR_GC_MODRON_SCAVENGER */

 	/**
	 * Set run-time Object Alignment in the heap value
	 * Function exists because we can only determine it is way after ObjectModel is init
	 */
	MMINLINE void
	setObjectAlignmentInBytes(uintptr_t objectAlignmentInBytes)
	{
		_objectAlignmentInBytes = objectAlignmentInBytes;
	}

 	/**
	 * Set run-time Object Alignment Shift value
	 * Function exists because we can only determine it is way after ObjectModel is init
	 */
	MMINLINE void
	setObjectAlignmentShift(uintptr_t objectAlignmentShift)
	{
		_objectAlignmentShift = objectAlignmentShift;
	}

 	/**
	 * Get run-time Object Alignment in the heap value
	 */
	MMINLINE uintptr_t
	getObjectAlignmentInBytes()
	{
		return _objectAlignmentInBytes;
	}

 	/**
	 * Get run-time Object Alignment Shift value
	 */
	MMINLINE uintptr_t
	getObjectAlignmentShift()
	{
		return _objectAlignmentShift;
	}

};

#endif /* OBJECTMODEL_HPP_ */
