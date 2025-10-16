#include "fixed_point_number.h"

#include <charconv>

static constexpr size_t c_FixedPointStrBufferSize = []() {
	int Decimals = 0;
	int32_t Val = std::numeric_limits<int32_t>::max();
	while(Val)
	{
		Decimals++;
		Val /= 10;
	}
	// +1 for leading minus
	// +1 for the float point character
	// +1 for null termination
	return Decimals + 3;
}();

const char *CFixedPointNumber::AsStr() const
{
	static thread_local char s_aBuf[c_FixedPointStrBufferSize];
	const int Whole = m_Value / 1000;
	int Fractional = std::abs(m_Value) % 1000;
	auto result = std::to_chars(s_aBuf, &s_aBuf[c_FixedPointStrBufferSize - 1], Whole);
	if(Fractional)
	{
		result.ptr[0] = '.';
		while(Fractional % 10 == 0)
			Fractional /= 10;

		result = std::to_chars(&result.ptr[1], &s_aBuf[c_FixedPointStrBufferSize - 1], Fractional);
	}
	result.ptr[0] = '\0';
	return s_aBuf;
}