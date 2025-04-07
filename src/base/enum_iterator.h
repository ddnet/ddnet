#ifndef BASE_ENUM_ITERATOR_H
#define BASE_ENUM_ITERATOR_H

#include <cstddef>
#include <type_traits>

template<typename Enum, std::size_t Size = static_cast<std::size_t>(Enum::NUM)>
class CEnumIterator
{
	static_assert(std::is_enum_v<Enum>, "EnumIterator requires an enum type");

	class CIterator
	{
	public:
		CIterator(std::size_t Index) :
			m_Index(Index) {}

		CIterator &operator++()
		{
			++m_Index;
			return *this;
		}

		bool operator!=(const CIterator &other) const
		{
			return m_Index != other.m_Index;
		}

		Enum operator*() const
		{
			return static_cast<Enum>(m_Index);
		}

	private:
		std::size_t m_Index;
	};

public:
	static auto begin()
	{
		return CIterator(0);
	}

	static auto end()
	{
		return CIterator(static_cast<std::size_t>(Enum::NUM));
	}
};

#endif
