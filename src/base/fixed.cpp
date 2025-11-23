#include "fixed.h"

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

void CFixed::AsStr(char *pBuffer, size_t BufferSize) const
{
	if(!pBuffer || BufferSize == 0)
		return;

	char *Dst = pBuffer;
	char *End = pBuffer + BufferSize;

	Underlying Value = m_Value;
	const bool Neg = Value < 0;
	if(Neg)
		Value = -Value;

	const Underlying Whole = Value / SCALE;
	const Underlying Frac = Value % SCALE;

	if(Neg)
	{
		if(Dst >= End - 1)
		{
			*pBuffer = '\0';
			return;
		}
		*Dst++ = '-';
	}

	const auto ToChars = std::to_chars(Dst, End, Whole);
	if(ToChars.ec != std::errc())
	{
		*pBuffer = '\0';
		return;
	}
	Dst = ToChars.ptr;

	if(Frac != 0)
	{
		if(Dst >= End - 1)
		{
			*pBuffer = '\0';
			return;
		}
		*Dst++ = '.';
		if(Dst + NUM_DECIMAL_DIGITS >= End)
		{
			*pBuffer = '\0';
			return;
		}
		WritePadded3(Dst, Frac);
		TrimTrailingZerosAfterDot(pBuffer, Dst);
	}
	else
	{
		*Dst = '\0';
	}
}

bool CFixed::AccumWholeProtect(Underlying &Whole, int Digit)
{
	using U = Underlying;
	constexpr U MaxU = std::numeric_limits<U>::max();
	if(Whole > (MaxU - Digit) / 10)
		return false;
	Whole = Whole * 10 + Digit;
	return true;
}

int CFixed::RoundFrac3(int Frac, int FirstDroppedDigit)
{
	// Round by the first dropped digit.
	if(FirstDroppedDigit >= 5)
	{
		Frac += 1;
		if(Frac >= SCALE)
			Frac = SCALE - 1;
	}
	return Frac;
}

bool CFixed::ParseRuntime(const char *p, Underlying &Out)
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

	// Validation: must have at least one digit (whole or fractional)
	if(DigitsWhole == 0 && FracDigits == 0)
		return false;

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

bool CFixed::FromStr(const char *pStr, CFixed &Out)
{
	Underlying V{};
	if(!ParseRuntime(pStr, V))
		return false;
	Out = FromRaw(V);
	return true;
}
