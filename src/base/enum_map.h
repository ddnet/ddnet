#ifndef BASE_ENUM_MAP_H
#define BASE_ENUM_MAP_H

#include <array>
#include <cstddef>
#include <type_traits>

template<typename Enum, typename Type, std::size_t Size = static_cast<std::size_t>(Enum::NUM)>
class CEnumMap
{
	static_assert(std::is_enum_v<Enum>, "EnumMap requires an enum type");
	std::array<Type, Size> m_aData;

public:
	CEnumMap() = default;
	CEnumMap(std::array<Type, Size> aData) :
		m_aData(aData) {}
	inline constexpr Type &operator[](Enum e) noexcept
	{
		return m_aData[static_cast<std::size_t>(e)];
	}

	inline constexpr Type &operator[](Enum e) const noexcept
	{
		return m_aData[static_cast<std::size_t>(e)];
	}

	inline constexpr void fill(const Type &value)
	{
		m_aData.fill(value);
	}

	auto begin() { return m_aData.begin(); }
	auto end() { return m_aData.end(); }
	auto begin() const { return m_aData.begin(); }
	auto end() const { return m_aData.end(); }
};

#endif
