/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "object_event_handler.h"
#include "allocation_event_db.h"
#include "nsobject_hook_method.h"
#include "sb_tree.h"

#include <dlfcn.h>

#pragma mark -
#pragma mark Types

struct object_type {
	uint32_t type;
	char name[59];
	bool is_nsobject;
	
	object_type(uint32_t _t, const char *_n=NULL, bool _s=false) : type(_t), is_nsobject(_s) {
		if (_n != NULL) {
			strncpy(name, _n, sizeof(name));
			name[sizeof(name) - 1] = '\0';
		}
	}
	
	inline bool operator != (const object_type &another) const {
		return type != another.type;
	}
	
	inline bool operator > (const object_type &another) const {
		return type > another.type;
	}
};

struct object_type_db {
    malloc_lock_s lock;
    
    buffer_source *buffer_source0; // memory
    buffer_source *buffer_source1; // file

    sb_tree<uintptr_t> *object_type_exists;
	sb_tree<object_type> *object_type_list;
};

#pragma mark -
#pragma mark Constants/Globals

static object_type_db *s_object_type_db = NULL;

static const char *vm_memory_type_names[] = {
	"VM_MEMORY_MALLOC", // #define VM_MEMORY_MALLOC 1
	"VM_MEMORY_MALLOC_SMALL", // #define VM_MEMORY_MALLOC_SMALL 2
	"VM_MEMORY_MALLOC_LARGE", // #define VM_MEMORY_MALLOC_LARGE 3
	"VM_MEMORY_MALLOC_HUGE", // #define VM_MEMORY_MALLOC_HUGE 4
	"VM_MEMORY_SBRK", // #define VM_MEMORY_SBRK 5// uninteresting -- no one should call
	"VM_MEMORY_REALLOC", // #define VM_MEMORY_REALLOC 6
	"VM_MEMORY_MALLOC_TINY", // #define VM_MEMORY_MALLOC_TINY 7
	"VM_MEMORY_MALLOC_LARGE_REUSABLE", // #define VM_MEMORY_MALLOC_LARGE_REUSABLE 8
	"VM_MEMORY_MALLOC_LARGE_REUSED", // #define VM_MEMORY_MALLOC_LARGE_REUSED 9
	"VM_MEMORY_ANALYSIS_TOOL", // #define VM_MEMORY_ANALYSIS_TOOL 10
	"VM_MEMORY_MALLOC_NANO", // #define VM_MEMORY_MALLOC_NANO 11
	"Unkonw", // 12
	"Unkonw", // 13
	"Unkonw", // 14
	"Unkonw", // 15
	"Unkonw", // 16
	"Unkonw", // 17
	"Unkonw", // 18
	"Unkonw", // 19
	"VM_MEMORY_MACH_MSG", // #define VM_MEMORY_MACH_MSG 20
	"VM_MEMORY_IOKIT", // #define VM_MEMORY_IOKIT	21
	"Unkonw", // 22
	"Unkonw", // 23
	"Unkonw", // 24
	"Unkonw", // 25
	"Unkonw", // 26
	"Unkonw", // 27
	"Unkonw", // 28
	"Unkonw", // 29
	"VM_MEMORY_STACK", // #define VM_MEMORY_STACK  30
	"VM_MEMORY_GUARD", // #define VM_MEMORY_GUARD  31
	"VM_MEMORY_SHARED_PMAP", // #define	VM_MEMORY_SHARED_PMAP 32
	/* memory containing a dylib */
	"VM_MEMORY_DYLIB", // #define VM_MEMORY_DYLIB	33
	"VM_MEMORY_OBJC_DISPATCHERS", // #define VM_MEMORY_OBJC_DISPATCHERS 34
	/* Was a nested pmap (VM_MEMORY_SHARED_PMAP) which has now been unnested */
	"VM_MEMORY_UNSHARED_PMAP", // #define	VM_MEMORY_UNSHARED_PMAP	35
	"Unkonw", // 36
	"Unkonw", // 37
	"Unkonw", // 38
	"Unkonw", // 39
	// Placeholders for now -- as we analyze the libraries and find how they
	// use memory, we can make these labels more specific.
	"AppKit", // #define VM_MEMORY_APPKIT 40
	"Foundation", // #define VM_MEMORY_FOUNDATION 41
	"CoreGraphics", // #define VM_MEMORY_COREGRAPHICS 42
	"CoreServices", // #define VM_MEMORY_CORESERVICES 43
	"Java", // #define VM_MEMORY_JAVA 44
	"CoreData", // #define VM_MEMORY_COREDATA 45
	"CoreData ObjectIDs", // #define VM_MEMORY_COREDATA_OBJECTIDS 46
	"Unkonw", // 47
	"Unkonw", // 48
	"Unkonw", // 49
	"ATS", // #define VM_MEMORY_ATS 50
	"CoreAnimation (LayerKit)", // #define VM_MEMORY_LAYERKIT 51
	"CGImage", // #define VM_MEMORY_CGIMAGE 52
	"TCMalloc", // #define VM_MEMORY_TCMALLOC 53
	/* private raster data (i.e. layers, some images, QGL allocator) */
	"CoreGraphics Data", // #define	VM_MEMORY_COREGRAPHICS_DATA	54
	/* shared image and font caches */
	"CoreGraphics Shared", // #define VM_MEMORY_COREGRAPHICS_SHARED	55
	/* Memory used for virtual framebuffers, shadowing buffers, etc... */
	"CoreGraphics FrameBuffers", // #define	VM_MEMORY_COREGRAPHICS_FRAMEBUFFERS	56
	/* Window backing stores, custom shadow data, and compressed backing stores */
	"CoreGraphics BackingStores", // #define VM_MEMORY_COREGRAPHICS_BACKINGSTORES	57
	/* x-alloc'd memory */
	"CoreGraphics XMalloc", // #define VM_MEMORY_COREGRAPHICS_XALLOC 58
	"Unkonw", // 59
	/* memory allocated by the dynamic loader for itself */
	"Dyld", // #define VM_MEMORY_DYLD 60
	/* malloc'd memory created by dyld */
	"Dyld Malloc", // #define VM_MEMORY_DYLD_MALLOC 61
	/* Used for sqlite page cache */
	"SQLite Page Cache", // #define VM_MEMORY_SQLITE 62
	/* JavaScriptCore heaps */
	"JavaScript Core", // #define VM_MEMORY_JAVASCRIPT_CORE 63
	/* memory allocated for the JIT */
	"JavaScript Jit Executable Allocator", // #define VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR 64
	"JavaScript Jit Register file", //#define VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE 65
	/* memory allocated for GLSL */
	"GLSL", // #define VM_MEMORY_GLSL  66
	/* memory allocated for OpenCL.framework */
	"OpenCL", // #define VM_MEMORY_OPENCL    67
	/* memory allocated for QuartzCore.framework */
	"CoreImage", // #define VM_MEMORY_COREIMAGE 68
	/* memory allocated for WebCore Purgeable Buffers */
	"WebCore Purgeable Buffers", // #define VM_MEMORY_WEBCORE_PURGEABLE_BUFFERS 69
	/* ImageIO memory */
	"ImageIO", // #define VM_MEMORY_IMAGEIO	70
	/* CoreProfile memory */
	"CoreProfile", // #define VM_MEMORY_COREPROFILE	71
	/* assetsd / MobileSlideShow memory */
	"AssetSD", // #define VM_MEMORY_ASSETSD	72
	/* libsystem_kernel os_once_alloc */
	"OS Alloc Once", // #define VM_MEMORY_OS_ALLOC_ONCE 73
	/* libdispatch internal allocator */
	"LibDispatch", // #define VM_MEMORY_LIBDISPATCH 74
	/* Accelerate.framework image backing stores */
	"Accelerate", // #define VM_MEMORY_ACCELERATE 75
	/* CoreUI image block data */
	"CoreUI", // #define VM_MEMORY_COREUI 76
	/* CoreUI image file */
	"CoreUI Image File", // #define VM_MEMORY_COREUIFILE 77
	/* Genealogy buffers */
	"Genealogy", // #define VM_MEMORY_GENEALOGY 78
	/* RawCamera VM allocated memory */
	"RawCamera", // #define VM_MEMORY_RAWCAMERA 79
	/* corpse info for dead process */
	"CorpseInfo", // #define VM_MEMORY_CORPSEINFO 80
	/* Apple System Logger (ASL) messages */
	"ASL", // #define VM_MEMORY_ASL 81
	/* Swift runtime */
	"Swift Runtime", // #define VM_MEMORY_SWIFT_RUNTIME 82
	/* Swift metadata */
	"Swift MetaData", // #define VM_MEMORY_SWIFT_METADATA 83
	/* DHMM data */
	"DHMM", // #define VM_MEMORY_DHMM 84
	"Unkonw", // 85
	/* memory allocated by SceneKit.framework */
	"SceneKit", // #define VM_MEMORY_SCENEKIT 86
	/* memory allocated by skywalk networking */
	"Skywalk", // #define VM_MEMORY_SKYWALK 87
	"IOSurface", // #define VM_MEMORY_IOSURFACE 88
	"LibNetwork", // #define VM_MEMORY_LIBNETWORK 89
	"Audio", // #define VM_MEMORY_AUDIO 90
	"VideoBitStream", // #define VM_MEMORY_VIDEOBITSTREAM 91
};

// Private API, cannot use it directly!
#ifdef USE_PRIVATE_API
static void (**object_set_last_allocation_event_name_funcion)(void *, const char *); // void (*__CFObjectAllocSetLastAllocEventNameFunction)(void *, const char *) = NULL;
static bool *object_record_allocation_event_enable; // bool __CFOASafe = false;
#endif

extern void __memory_event_update_object(uint64_t address, uint32_t new_type);

#pragma mark -
#pragma mark OC Event Logging

inline size_t __string_hash(const char* str)
{
	size_t seed = 131; // 31 131 1313 13131 131313 etc..
	size_t hash = 0;
	while (*str) {
		hash = hash * seed + (*str++);
	}
	return (hash | 0x1);
}

void object_set_last_allocation_event_name(void *ptr, const char *classname)
{
	if (!ptr) return;
	if (!classname) classname = "(no class)";

	uint32_t type = 0;
	uintptr_t str_hash = __string_hash(classname);

	__malloc_lock_lock(&s_object_type_db->lock);

	if (s_object_type_db->object_type_exists->exist(str_hash) == false) {
		type = s_object_type_db->object_type_list->size() + 1;
		s_object_type_db->object_type_exists->insert(str_hash);
		s_object_type_db->object_type_list->insert(object_type(type, classname));
	} else {
		type = s_object_type_db->object_type_exists->foundIndex();
	}
	
	__malloc_lock_unlock(&s_object_type_db->lock);

	__memory_event_update_object((uint64_t)ptr, type);
}

void nsobject_set_last_allocation_event_name(void *ptr, const char *classname)
{
	if (!ptr) return;
	if (!classname) classname = "(no class)";
	
	uint32_t type = 0;
	
	__malloc_lock_lock(&s_object_type_db->lock);

	if (s_object_type_db->object_type_exists->exist((uintptr_t)classname) == false) {
		type = s_object_type_db->object_type_list->size() + 1;
		s_object_type_db->object_type_exists->insert((uintptr_t)classname);
		s_object_type_db->object_type_list->insert(object_type(type, classname, true));
	} else {
		type = s_object_type_db->object_type_exists->foundIndex();
	}
	
	__malloc_lock_unlock(&s_object_type_db->lock);
	
	__memory_event_update_object((uint64_t)ptr, type);
}

#pragma mark - 
#pragma mark Public Interface

object_type_db *prepare_object_event_logger(const char *log_dir)
{
	s_object_type_db = object_type_db_open_or_create(log_dir);
	if (s_object_type_db == NULL) {
		return NULL;
	}
	
	// Insert vm memory type names
	for (int i = 0; i < sizeof(vm_memory_type_names) / sizeof(char *); ++i) {
		uintptr_t str_hash = __string_hash(vm_memory_type_names[i]);
		uint32_t type = s_object_type_db->object_type_list->size() + 1;
		s_object_type_db->object_type_exists->insert(str_hash);
		s_object_type_db->object_type_list->insert(object_type(type, vm_memory_type_names[i]));
	}
	
#ifdef USE_PRIVATE_API
    // __CFObjectAllocSetLastAllocEventNameFunction
	object_set_last_allocation_event_name_funcion = (void (**)(void *, const char *))dlsym(RTLD_DEFAULT, "__CFObjectAllocSetLastAllocEventNameFunction");
	if (object_set_last_allocation_event_name_funcion != NULL) {
		*object_set_last_allocation_event_name_funcion = object_set_last_allocation_event_name;
	}
	
	// __CFOASafe
	object_record_allocation_event_enable = (bool *)dlsym(RTLD_DEFAULT, "__CFOASafe");
	if (object_record_allocation_event_enable != NULL) {
		*object_record_allocation_event_enable = true;
	}
#endif
	
	nsobject_hook_alloc_method();
	
	return s_object_type_db;
}

void disable_object_event_logger()
{
#ifdef USE_PRIVATE_API
    if (object_set_last_allocation_event_name_funcion != NULL) {
        *object_set_last_allocation_event_name_funcion = NULL;
    }
    if (object_record_allocation_event_enable != NULL) {
        *object_record_allocation_event_enable = false;
    }
#endif
	//nsobject_hook_alloc_method();
}

object_type_db *object_type_db_open_or_create(const char *event_dir)
{
	object_type_db *db_context = (object_type_db *)inter_malloc(sizeof(object_type_db));
    
    db_context->lock = __malloc_lock_init();
    db_context->buffer_source0 = new buffer_source_memory();
    db_context->buffer_source1 = new buffer_source_file(event_dir, "object_types.dat");
    
    if (db_context->buffer_source1->init_fail()) {
		// should not be happened
		err_code = MS_ERRC_OE_FILE_OPEN_FAIL;
		goto init_fail;
	} else {
        db_context->object_type_list = new sb_tree<object_type>(1 << 10, db_context->buffer_source1);
	}
    
	db_context->object_type_exists = new sb_tree<uintptr_t>(1 << 10, db_context->buffer_source0);
	return db_context;
	
init_fail:
    object_type_db_close(db_context);
	return NULL;
}

void object_type_db_close(object_type_db *db_context)
{
	if (db_context == NULL) {
		return;
	}
	
	delete db_context->object_type_list;
	delete db_context->object_type_exists;
    delete db_context->buffer_source0;
    delete db_context->buffer_source1;
	inter_free(db_context);
}

const char *object_type_db_get_object_name(object_type_db *db_context, uint32_t type)
{
	const char *name = NULL;
	
	__malloc_lock_lock(&db_context->lock);
	
	if (db_context->object_type_list->exist(type)) {
		name = db_context->object_type_list->find().name;
	}
	
	__malloc_lock_unlock(&db_context->lock);
	
	return name;
}

bool object_type_db_is_nsobject(object_type_db *db_context, uint32_t type)
{
	bool is_nsobject = false;
	
	__malloc_lock_lock(&db_context->lock);
	
	if (db_context->object_type_list->exist(type)) {
		is_nsobject = db_context->object_type_list->find().is_nsobject;
	}
	
	__malloc_lock_unlock(&db_context->lock);
	
	return is_nsobject;
}
