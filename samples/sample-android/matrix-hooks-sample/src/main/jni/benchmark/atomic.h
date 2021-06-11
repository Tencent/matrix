// ported from https://github.com/mjansson/rpmalloc-benchmark
#include <stdint.h>

#if defined( __x86_64__ ) || defined( _M_AMD64 ) || defined( _M_X64 ) || defined( _AMD64_ ) || defined( __arm64__ ) || defined( __aarch64__ )
#  define ARCH_64BIT 1
#else
#  define ARCH_64BIT 0
#endif

#ifdef _MSC_VER
#  define ALIGNED_STRUCT(name, alignment) __declspec(align(alignment)) struct name
#else
#  define ALIGNED_STRUCT(name, alignment) struct __attribute__((__aligned__(alignment))) name
#endif

ALIGNED_STRUCT(atomicptr_t, 8) {
	void* nonatomic;
};
typedef struct atomicptr_t atomicptr_t;

static void*
atomic_load_ptr(atomicptr_t* src) {
	return src->nonatomic;
}

static void
atomic_store_ptr(atomicptr_t* dst, void* val) {
	dst->nonatomic = val;
}

ALIGNED_STRUCT(atomic32_t, 4) {
	int32_t nonatomic;
};
typedef struct atomic32_t atomic32_t;

static int32_t
atomic_load32(atomic32_t* src) {
	return src->nonatomic;
}

static void
atomic_store32(atomic32_t* dst, int32_t val) {
	dst->nonatomic = val;
}

static int32_t
atomic_incr32(atomic32_t* val) {
#ifdef _MSC_VER
	int32_t old = (int32_t)_InterlockedExchangeAdd((volatile long*)&val->nonatomic, 1);
	return (old + 1);
#else
	return __sync_add_and_fetch(&val->nonatomic, 1);
#endif
}

static int32_t
atomic_add32(atomic32_t* val, int32_t add) {
#ifdef _MSC_VER
	int32_t old = (int32_t)_InterlockedExchangeAdd((volatile long*)&val->nonatomic, add);
	return (old + add);
#else
	return __sync_add_and_fetch(&val->nonatomic, add);
#endif
}

static int
atomic_cas_ptr(atomicptr_t* dst, void* val, void* ref) {
#ifdef _MSC_VER
#  if ARCH_64BIT
	return (_InterlockedCompareExchange64((volatile long long*)&dst->nonatomic,
		(long long)val, (long long)ref) == (long long)ref) ? 1 : 0;
#  else
	return (_InterlockedCompareExchange((volatile long*)&dst->nonatomic,
		(long)val, (long)ref) == (long)ref) ? 1 : 0;
#  endif
#else
	return __sync_bool_compare_and_swap(&dst->nonatomic, ref, val);
#endif
}

#undef ARCH_64BIT
#undef ALIGNED_STRUCT
