/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_RINGBUFFER_H
#define ENGINE_SHARED_RINGBUFFER_H

class CRingBufferBase
{
	class CItem
	{
	public:
		CItem *m_pPrev = nullptr;
		CItem *m_pNext = nullptr;
		int m_Free = 0;
		int m_Size = 0;
	};

	CItem *m_pProduce = nullptr;
	CItem *m_pConsume = nullptr;

	CItem *m_pFirst = nullptr;
	CItem *m_pLast = nullptr;

	int m_Size = 0;
	int m_Flags = 0;

	CItem *NextBlock(CItem *pItem);
	CItem *PrevBlock(CItem *pItem);
	CItem *MergeBack(CItem *pItem);

protected:
	void *Allocate(int Size);

	void *Prev(void *pCurrent);
	void *Next(void *pCurrent);
	void *First();
	void *Last();

	void Init(void *pMemory, int Size, int Flags);
	int PopFirst();

public:
	enum
	{
		// Will start to destroy items to try to fit the next one
		FLAG_RECYCLE = 1
	};
};

template<typename T, int TSIZE, int TFLAGS = 0>
class CStaticRingBuffer : public CRingBufferBase
{
	unsigned char m_aBuffer[TSIZE] = {0};

public:
	CStaticRingBuffer() { Init(); }

	void Init() { CRingBufferBase::Init(m_aBuffer, TSIZE, TFLAGS); }

	T *Allocate(int Size) { return (T *)CRingBufferBase::Allocate(Size); }
	int PopFirst() { return CRingBufferBase::PopFirst(); }

	T *Prev(T *pCurrent) { return (T *)CRingBufferBase::Prev(pCurrent); }
	T *Next(T *pCurrent) { return (T *)CRingBufferBase::Next(pCurrent); }
	T *First() { return (T *)CRingBufferBase::First(); }
	T *Last() { return (T *)CRingBufferBase::Last(); }
};

#endif
