/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ALLOC_H
#define GAME_SERVER_ALLOC_H

#include <bitset>
#include <iterator>

#include <base/system.h>

template<typename Type, const std::size_t Size>
class CObjectPool {
    char m_Data[Size][sizeof(Type)]{{0}};
    std::bitset<Size> m_Used{0};

public:
    CObjectPool() {
    }

    template <typename ...TArgs>
    Type *New(std::size_t ObjectId, TArgs ...Args) {
        dbg_assert(!m_Used[ObjectId], "Already constructed");
        dbg_assert(0 <= ObjectId && ObjectId < Size, "Invalid ObjectId");

        Type *Pointer = new (m_Data[ObjectId]) Type(std::forward<TArgs>(Args)...);
        m_Used[ObjectId] = true;

        return Pointer;
    }

    Type *Get(std::size_t ObjectId) const {
        dbg_assert(0 <= ObjectId && ObjectId < Size, "Invalid ObjectId");

        if (!m_Used[ObjectId])
            return nullptr;
        return (Type *)(m_Data[ObjectId]);
    }

    Type *operator[](std::size_t ObjectId) const
    {
        return Get(ObjectId);
    }

    void Delete(std::size_t ObjectId) {
        dbg_assert(m_Used[ObjectId], "Already unused");
        dbg_assert(0 <= ObjectId && ObjectId < Size, "Invalid ObjectId");

        m_Used[ObjectId] = false;
    }

    void Reset()
    {
        for (std::size_t i = 0; i < Size; i++)
            m_Used[i] = false;
    }

    template<typename T>
    class iterator
    {
        const CObjectPool *m_pPool;
        std::size_t m_Id;

        // helper
        T Value() const { return (T)(m_pPool->Get(m_Id)); };

    public:
        using value_type = T;
        using self_type = iterator<T>;
        using size_type = std::size_t;
        using difference_type = T;

        iterator(const CObjectPool *pPool, std::size_t Id) : m_pPool(pPool), m_Id(Id) {}

        self_type operator++() { m_Id++; return *this; }
        self_type operator++(int) { return self_type(m_pPool, m_Id + 1); }
        value_type operator*() { return Value(); }
        value_type *operator->() { return &Value(); }
        friend bool operator==(const self_type& Lhs, const self_type& Rhs) { return Lhs.m_Id == Rhs.m_Id; }
        friend bool operator!=(const self_type& Lhs, const self_type& Rhs) { return Lhs.m_Id != Rhs.m_Id; }
    };

    iterator<Type *> begin() const { return iterator<Type *>(this, 0); }
    iterator<Type *> end() const { return iterator<Type *>(this, Size); }
    iterator<const Type *> cbegin() const { return iterator<const Type *>(this, 0); }
    iterator<const Type *> cend() const { return iterator<const Type *>(this, Size); }
};

#endif
