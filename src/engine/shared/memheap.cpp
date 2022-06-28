/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "memheap.h"
#include <base/system.h>
#include <cstdint>
#include <cstdlib>

// allocates a new chunk to be used
void CHeap::NewChunk()
{
	CChunk *pChunk;
	char *pMem;

	// allocate memory
	pMem = (char *)malloc(sizeof(CChunk) + CHUNK_SIZE);
	if(!pMem)
		return;

	// the chunk structure is located in the beginning of the chunk
	// init it and return the chunk
	pChunk = (CChunk *)pMem;
	pChunk->m_pMemory = (char *)(pChunk + 1);
	pChunk->m_pCurrent = pChunk->m_pMemory;
	pChunk->m_pEnd = pChunk->m_pMemory + CHUNK_SIZE;
	pChunk->m_pNext = (CChunk *)0x0;

	pChunk->m_pNext = m_pCurrent;
	m_pCurrent = pChunk;
}

//****************
void *CHeap::AllocateFromChunk(unsigned int Size, unsigned Alignment)
{
	char *pMem;
	size_t Offset = reinterpret_cast<uintptr_t>(m_pCurrent->m_pCurrent) % Alignment;
	if(Offset)
		Offset = Alignment - Offset;

	// check if we need can fit the allocation
	if(m_pCurrent->m_pCurrent + Offset + Size > m_pCurrent->m_pEnd)
		return (void *)0x0;

	// get memory and move the pointer forward
	pMem = m_pCurrent->m_pCurrent + Offset;
	m_pCurrent->m_pCurrent += Offset + Size;
	return pMem;
}

// creates a heap
CHeap::CHeap()
{
	m_pCurrent = 0x0;
	Reset();
}

CHeap::~CHeap()
{
	Clear();
}

void CHeap::Reset()
{
	Clear();
	NewChunk();
}

// destroys the heap
void CHeap::Clear()
{
	CChunk *pChunk = m_pCurrent;
	CChunk *pNext;

	while(pChunk)
	{
		pNext = pChunk->m_pNext;
		free(pChunk);
		pChunk = pNext;
	}

	m_pCurrent = 0x0;
}

//
void *CHeap::Allocate(unsigned Size, unsigned Alignment)
{
	char *pMem;

	// try to allocate from current chunk
	pMem = (char *)AllocateFromChunk(Size, Alignment);
	if(!pMem)
	{
		// allocate new chunk and add it to the heap
		NewChunk();

		// try to allocate again
		pMem = (char *)AllocateFromChunk(Size, Alignment);
	}

	return pMem;
}

const char *CHeap::StoreString(const char *pSrc)
{
	const int Size = str_length(pSrc) + 1;
	char *pMem = static_cast<char *>(Allocate(Size));
	str_copy(pMem, pSrc, Size);
	return pMem;
}
