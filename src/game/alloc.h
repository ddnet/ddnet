/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_ALLOC_H
#define GAME_ALLOC_H

#include <new>

#include <base/system.h>
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if __has_feature(address_sanitizer)
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) \
	((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
	((void)(addr), (void)(size))
#endif

#define MACRO_ALLOC_HEAP() \
public: \
	void *operator new(size_t Size) \
	{ \
		void *pObj = malloc(Size); \
		mem_zero(pObj, Size); \
		return pObj; \
	} \
	void operator delete(void *pPtr) \
	{ \
		free(pPtr); \
	} \
\
private:

#define MACRO_ALLOC_POOL_ID() \
public: \
	void *operator new(size_t Size, int Id); \
	void operator delete(void *pObj, int Id); \
	void operator delete(void *pObj); /* NOLINT(misc-new-delete-overloads) */ \
\
private:

#if __has_feature(address_sanitizer)
#define MACRO_ALLOC_GET_SIZE(POOLTYPE) ((sizeof(POOLTYPE) + 7) & ~7)
#else
#define MACRO_ALLOC_GET_SIZE(POOLTYPE) (sizeof(POOLTYPE))
#endif

#define MACRO_ALLOC_POOL_ID_IMPL(POOLTYPE, PoolSize) \
	static char gs_PoolData##POOLTYPE[PoolSize][MACRO_ALLOC_GET_SIZE(POOLTYPE)] = {{0}}; \
	static int gs_PoolUsed##POOLTYPE[PoolSize] = {0}; \
	MAYBE_UNUSED static int gs_PoolDummy##POOLTYPE = (ASAN_POISON_MEMORY_REGION(gs_PoolData##POOLTYPE, sizeof(gs_PoolData##POOLTYPE)), 0); \
	void *POOLTYPE::operator new(size_t Size, int Id) \
	{ \
		dbg_assert(sizeof(POOLTYPE) >= Size, "size error"); \
		dbg_assert(!gs_PoolUsed##POOLTYPE[Id], "already used"); \
		ASAN_UNPOISON_MEMORY_REGION(gs_PoolData##POOLTYPE[Id], sizeof(gs_PoolData##POOLTYPE[Id])); \
		gs_PoolUsed##POOLTYPE[Id] = 1; \
		mem_zero(gs_PoolData##POOLTYPE[Id], sizeof(gs_PoolData##POOLTYPE[Id])); \
		return gs_PoolData##POOLTYPE[Id]; \
	} \
	void POOLTYPE::operator delete(void *pObj, int Id) \
	{ \
		dbg_assert(gs_PoolUsed##POOLTYPE[Id], "not used"); \
		dbg_assert(Id == (POOLTYPE *)pObj - (POOLTYPE *)gs_PoolData##POOLTYPE, "invalid id"); \
		gs_PoolUsed##POOLTYPE[Id] = 0; \
		mem_zero(gs_PoolData##POOLTYPE[Id], sizeof(gs_PoolData##POOLTYPE[Id])); \
		ASAN_POISON_MEMORY_REGION(gs_PoolData##POOLTYPE[Id], sizeof(gs_PoolData##POOLTYPE[Id])); \
	} \
	void POOLTYPE::operator delete(void *pObj) /* NOLINT(misc-new-delete-overloads) */ \
	{ \
		int Id = (POOLTYPE *)pObj - (POOLTYPE *)gs_PoolData##POOLTYPE; \
		dbg_assert(gs_PoolUsed##POOLTYPE[Id], "not used"); \
		gs_PoolUsed##POOLTYPE[Id] = 0; \
		mem_zero(gs_PoolData##POOLTYPE[Id], sizeof(gs_PoolData##POOLTYPE[Id])); \
		ASAN_POISON_MEMORY_REGION(gs_PoolData##POOLTYPE[Id], sizeof(gs_PoolData##POOLTYPE[Id])); \
	}

#endif
