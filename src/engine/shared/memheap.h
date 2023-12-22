/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_MEMHEAP_H
#define ENGINE_SHARED_MEMHEAP_H

#include <cstddef>
#if __has_include(<memory_resource>)
#include <memory_resource>
#else
// macOS < 14.0 needs this
#include <experimental/memory_resource>
#endif
#include <new>
#include <utility>

// A Heap backed by a bump allocator.
// The size of buffers obtained follows a geometric progression.
class CHeap
{
	std::pmr::monotonic_buffer_resource m_BumpAllocator;
	void Clear();

public:
	CHeap();
	~CHeap();
	// Resets the heap, deallocating everything at once.
	void Reset();
	void *Allocate(size_t Size, size_t Alignment = alignof(std::max_align_t));
	const char *StoreString(const char *pSrc);

	template<typename T, typename... TArgs>
	T *Allocate(TArgs &&... Args)
	{
		return new(Allocate(sizeof(T), alignof(T))) T(std::forward<TArgs>(Args)...);
	}
};

#endif
