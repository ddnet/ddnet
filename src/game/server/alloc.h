/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ALLOC_H
#define GAME_SERVER_ALLOC_H

#include <base/system.h>
#include <bitset>

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

template<typename Type, const size_t Size>
class CObjectPool
{
	char m_aaData[Size][sizeof(Type)]{{0}};
	std::bitset<Size> m_Used{0};

public:
	template<typename... TArgs>
	Type *New(size_t ObjectId, TArgs... Args)
	{
		dbg_assert(!m_Used[ObjectId], "Already constructed");
		dbg_assert(ObjectId < Size, "Invalid ObjectId");

		Type *pObject = new(m_aaData[ObjectId]) Type(std::forward<TArgs>(Args)...);
		m_Used[ObjectId] = true;

		ASAN_UNPOISON_MEMORY_REGION(&m_aaData[ObjectId], sizeof(Type));

		return pObject;
	}

	Type *Get(size_t ObjectId) const
	{
		dbg_assert(ObjectId < Size, "Invalid ObjectId");

		if(!m_Used[ObjectId])
			return nullptr;
		return (Type *)(m_aaData[ObjectId]);
	}

	Type *operator[](size_t ObjectId) const
	{
		return Get(ObjectId);
	}

	void Delete(size_t ObjectId)
	{
		dbg_assert(m_Used[ObjectId], "Already unused");
		dbg_assert(ObjectId < Size, "Invalid ObjectId");

		m_Used[ObjectId] = false;

		ASAN_POISON_MEMORY_REGION(&m_aaData[ObjectId], sizeof(Type));
	}

	void Reset()
	{
		m_Used.reset();

		ASAN_POISON_MEMORY_REGION(&m_aaData[0], sizeof(Type) * Size);
	}

	template<typename T>
	class iterator
	{
		const CObjectPool *m_pPool;
		size_t m_Id;

		// helper
		T Value() const { return (T)(m_pPool->Get(m_Id)); };

	public:
		using value_type = T;
		using self_type = iterator<T>;
		using size_type = size_t;
		using difference_type = T;

		iterator(const CObjectPool *pPool, size_t Id) :
			m_pPool(pPool), m_Id(Id) {}

		self_type operator++()
		{
			m_Id++;
			return *this;
		}
		self_type operator++(int) { return self_type(m_pPool, m_Id + 1); }
		value_type operator*() { return Value(); }
		value_type *operator->() { return &Value(); }
		friend bool operator==(const self_type &Lhs, const self_type &Rhs) { return Lhs.m_Id == Rhs.m_Id; }
		friend bool operator!=(const self_type &Lhs, const self_type &Rhs) { return Lhs.m_Id != Rhs.m_Id; }
	};

	iterator<Type *> begin() const { return iterator<Type *>(this, 0); }
	iterator<Type *> end() const { return iterator<Type *>(this, Size); }
	iterator<const Type *> cbegin() const { return iterator<const Type *>(this, 0); }
	iterator<const Type *> cend() const { return iterator<const Type *>(this, Size); }
};

#endif
