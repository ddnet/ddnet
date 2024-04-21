/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_RINGBUFFER_H
#define ENGINE_SHARED_RINGBUFFER_H

#include <base/system.h>

#include <functional>

class CRingBufferBase
{
	class CItem
	{
	public:
		CItem *m_pPrev;
		CItem *m_pNext;
		int m_Free;
		int m_Size;
	};

	CItem *m_pProduce;
	CItem *m_pConsume;

	CItem *m_pFirst;
	CItem *m_pLast;

	int m_Size;
	int m_Flags;

	std::function<void(void *pCurrent)> m_PopCallback = nullptr;

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
	void SetPopCallback(const std::function<void(void *pCurrent)> PopCallback);

public:
	enum
	{
		// Will start to destroy items to try to fit the next one
		FLAG_RECYCLE = 1
	};
	static constexpr int ITEM_SIZE = sizeof(CItem);
};

template<typename T>
class CTypedRingBuffer : public CRingBufferBase
{
public:
	T *Allocate(int Size) { return (T *)CRingBufferBase::Allocate(Size); }
	int PopFirst() { return CRingBufferBase::PopFirst(); }
	void SetPopCallback(std::function<void(T *pCurrent)> PopCallback)
	{
		CRingBufferBase::SetPopCallback([PopCallback](void *pCurrent) {
			PopCallback((T *)pCurrent);
		});
	}

	T *Prev(T *pCurrent) { return (T *)CRingBufferBase::Prev(pCurrent); }
	T *Next(T *pCurrent) { return (T *)CRingBufferBase::Next(pCurrent); }
	T *First() { return (T *)CRingBufferBase::First(); }
	T *Last() { return (T *)CRingBufferBase::Last(); }
};

template<typename T, int TSIZE, int TFLAGS = 0>
class CStaticRingBuffer : public CTypedRingBuffer<T>
{
	unsigned char m_aBuffer[TSIZE];

public:
	CStaticRingBuffer() { Init(); }

	void Init() { CRingBufferBase::Init(m_aBuffer, TSIZE, TFLAGS); }
};

template<typename T>
class CDynamicRingBuffer : public CTypedRingBuffer<T>
{
	unsigned char *m_pBuffer = nullptr;

public:
	CDynamicRingBuffer(int Size, int Flags = 0) { Init(Size, Flags); }

	virtual ~CDynamicRingBuffer()
	{
		free(m_pBuffer);
	}

	void Init(int Size, int Flags)
	{
		free(m_pBuffer);
		m_pBuffer = static_cast<unsigned char *>(malloc(Size));
		CRingBufferBase::Init(m_pBuffer, Size, Flags);
	}
};

#endif
