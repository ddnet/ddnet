#ifndef ENGINE_SHARED_FIXED_POINT_NUMBER_H
#define ENGINE_SHARED_FIXED_POINT_NUMBER_H

#include <base/math.h>

#include <cstdint>

class CFixedPointNumber
{
public:
	constexpr CFixedPointNumber() = default;
	constexpr CFixedPointNumber(float Value);

	constexpr CFixedPointNumber &operator=(float Value);
	constexpr operator float() const;
	static constexpr float MaxValue();

	const char *AsStr() const;

protected:
	using Underlying = int32_t;
	Underlying m_Value = 0;
};

inline constexpr CFixedPointNumber::CFixedPointNumber(float Value)
{
	m_Value = round_to_int(Value * 1000);
}

inline constexpr CFixedPointNumber &CFixedPointNumber::operator=(float Value)
{
	m_Value = round_to_int(Value * 1000);
	return *this;
}

inline constexpr CFixedPointNumber::operator float() const
{
	return m_Value / 1000.0f;
}

inline constexpr float CFixedPointNumber::MaxValue()
{
	constexpr Underlying v = (std::numeric_limits<Underlying>::max() / 100);
	return CFixedPointNumber{v / 1000.0f};
}

#endif // ENGINE_SHARED_FIXED_POINT_NUMBER_H