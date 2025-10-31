#include "fixed_point_number.h"

#include <charconv>
#include <limits>

static void WritePadded3(char *&Dst, const int Frac)
{
	// Exactly 3 digits with leading zeros.
	const int D2 = (Frac / 100) % 10;
	const int D1 = (Frac / 10) % 10;
	const int D0 = Frac % 10;
	*Dst++ = static_cast<char>('0' + D2);
	*Dst++ = static_cast<char>('0' + D1);
	*Dst++ = static_cast<char>('0' + D0);
}

static void TrimTrailingZerosAfterDot(const char *Begin, char *&End)
{
	// Strip trailing zeros and a trailing dot if needed.
	while(End > Begin && *(End - 1) == '0')
		--End;
	if(End > Begin && *(End - 1) == '.')
		--End;
	*End = '\0';
}

const char *CFixedPointNumber::AsStr() const
{
	// Sign + up to 10 digits + '.' + 3 fractional + NUL.
	static thread_local char s_Buf[1 + 10 + 1 + NUM_DECIMAL_DIGITS + 1];
	char *Dst = s_Buf;

	Underlying Value = m_Value;
	const bool Neg = Value < 0;
	if(Neg)
		Value = -Value;

	const Underlying Whole = Value / SCALE;
	const Underlying Frac = Value % SCALE;

	if(Neg)
		*Dst++ = '-';

	const auto ToChars = std::to_chars(Dst, s_Buf + sizeof(s_Buf), Whole);
	if(ToChars.ec != std::errc())
	{
		*s_Buf = '\0';
		return s_Buf;
	}
	Dst = ToChars.ptr;

	if(Frac != 0)
	{
		*Dst++ = '.';
		WritePadded3(Dst, Frac);
		TrimTrailingZerosAfterDot(s_Buf, Dst);
	}
	else
	{
		*Dst = '\0';
	}
	return s_Buf;
}

static bool AccumWholeProtect(CFixedPointNumber::Underlying &Whole, const int Digit)
{
	using U = CFixedPointNumber::Underlying;
	constexpr U MaxU = std::numeric_limits<U>::max();
	if(Whole > (MaxU - Digit) / 10)
		return false;
	Whole = Whole * 10 + Digit;
	return true;
}

static int RoundFrac3(int Frac, const int FirstDroppedDigit)
{
	// Round by the first dropped digit.
	if(FirstDroppedDigit >= 5)
	{
		Frac += 1;
		if(Frac >= CFixedPointNumber::SCALE)
			Frac = CFixedPointNumber::SCALE - 1;
	}
	return Frac;
}

bool CFixedPointNumber::ParseRuntime(const char *p, Underlying &Out)
{
	if(!p || *p == '\0')
		return false;

	// Sign.
	bool Neg = false;
	if(*p == '+' || *p == '-')
	{
		Neg = (*p == '-');
		++p;
	}

	// Whole part.
	Underlying Whole = 0;
	int DigitsWhole = 0;
	while(*p && (*p >= '0' && *p <= '9'))
	{
		if(!AccumWholeProtect(Whole, *p - '0'))
			return false;
		++p;
		++DigitsWhole;
	}
	if(DigitsWhole == 0 && (*p != '.'))
		return false;

	// Fractional part.
	int Frac = 0;
	int FracDigits = 0;
	int FirstDropped = -1;
	if(*p == '.')
	{
		++p;
		if(!(*p >= '0' && *p <= '9') && *p != '\0')
			return false;
		while(*p && (*p >= '0' && *p <= '9'))
		{
			const int D = *p - '0';
			if(FracDigits < NUM_DECIMAL_DIGITS)
			{
				Frac = Frac * 10 + D;
				++FracDigits;
			}
			else if(FirstDropped < 0)
			{
				FirstDropped = D;
			}
			++p;
		}
	}

	if(FracDigits == 0)
	{
		Frac = 0;
	}
	else if(FracDigits < NUM_DECIMAL_DIGITS)
	{
		for(int k = FracDigits; k < NUM_DECIMAL_DIGITS; ++k)
			Frac *= 10;
	}
	else
	{
		Frac = RoundFrac3(Frac, FirstDropped < 0 ? 0 : FirstDropped);
	}

	// Strict: must end exactly here.
	if(*p != '\0')
		return false;

	// Assemble and clamp.
	long long Mag = static_cast<long long>(Whole) * SCALE + static_cast<long long>(Frac);
	if(Neg)
		Mag = -Mag;

	if(Mag > std::numeric_limits<Underlying>::max())
		Out = std::numeric_limits<Underlying>::max();
	else if(Mag < std::numeric_limits<Underlying>::min())
		Out = std::numeric_limits<Underlying>::min();
	else
		Out = static_cast<Underlying>(Mag);

	return true;
}

bool CFixedPointNumber::FromStr(const char *pStr, CFixedPointNumber &Out)
{
	Underlying V{};
	if(!ParseRuntime(pStr, V))
		return false;
	Out = FromRaw(V);
	return true;
}
