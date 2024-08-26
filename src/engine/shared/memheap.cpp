/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "memheap.h"
#include <base/system.h>
#include <cstdint>
#include <cstdlib>

// creates a heap
CHeap::CHeap()
{
	Reset();
}

CHeap::~CHeap()
{
	Clear();
}

void CHeap::Reset()
{
	Clear();
}

// destroys the heap
void CHeap::Clear()
{
	m_BumpAllocator.release();
}

void *CHeap::Allocate(size_t Size, size_t Alignment)
{
	return m_BumpAllocator.allocate(Size, Alignment);
}

const char *CHeap::StoreString(const char *pSrc)
{
	const size_t Size = str_length(pSrc) + 1;
	char *pMem = Allocate<char>(Size);
	str_copy(pMem, pSrc, Size);
	return pMem;
}
