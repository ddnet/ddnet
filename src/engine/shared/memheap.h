/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_MEMHEAP_H
#define ENGINE_SHARED_MEMHEAP_H

#include <cstddef>
class CHeap
{
	struct CChunk
	{
		char *m_pMemory = nullptr;
		char *m_pCurrent = nullptr;
		char *m_pEnd = nullptr;
		CChunk *m_pNext = nullptr;
	};

	enum
	{
		// how large each chunk should be
		CHUNK_SIZE = 1025 * 64,
	};

	CChunk *m_pCurrent = nullptr;

	void Clear();
	void NewChunk();
	void *AllocateFromChunk(unsigned int Size, unsigned Alignment);

public:
	CHeap();
	~CHeap();
	void Reset();
	void *Allocate(unsigned Size, unsigned Alignment = alignof(std::max_align_t));
	const char *StoreString(const char *pSrc);
};
#endif
