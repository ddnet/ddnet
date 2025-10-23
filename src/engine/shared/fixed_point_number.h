#ifndef ENGINE_SHARED_FIXED_POINT_NUMBER_H
#define ENGINE_SHARED_FIXED_POINT_NUMBER_H

#include <cstddef>
#include <cstdint>
#include <limits>

class CFixedPointNumber
{
public:
	using Underlying = int32_t;

	// Fixed-point format: 3 decimal places -> SCALE = 1000.
	static constexpr Underlying SCALE = 1000;
	static constexpr int NUM_DECIMAL_DIGITS = 3;

	// Runtime parse from C-string (strict: only [-]123 or [-]123.456, no spaces/suffixes/inf/nan).
	static bool FromStr(const char *pStr, CFixedPointNumber &Out);

	// Compile-time parse for string literals.
	template<size_t N>
	static consteval CFixedPointNumber FromLiteral(const char (&Lit)[N])
	{
		Underlying V{};
		if(!ParseLiteral(Lit, N, V))
		{
			return CFixedPointNumber(0);
		}
		return FromRaw(V);
	}

	// Convert to C-string. Returns a pointer to a thread_local buffer.
	const char *AsStr() const;

	// Raw access and conversions.
	static constexpr CFixedPointNumber FromRaw(const Underlying V) { return CFixedPointNumber(V); }
	constexpr bool IsZero() const { return m_Value == 0; }
	constexpr explicit operator float() const { return static_cast<float>(m_Value) / static_cast<float>(SCALE); }

	// Comparisons.
	constexpr bool operator==(const CFixedPointNumber &O) const { return m_Value == O.m_Value; }
	constexpr bool operator!=(const CFixedPointNumber &O) const { return m_Value != O.m_Value; }
	constexpr bool operator<(const CFixedPointNumber &O) const { return m_Value < O.m_Value; }
	constexpr bool operator<=(const CFixedPointNumber &O) const { return m_Value <= O.m_Value; }
	constexpr bool operator>(const CFixedPointNumber &O) const { return m_Value > O.m_Value; }
	constexpr bool operator>=(const CFixedPointNumber &O) const { return m_Value >= O.m_Value; }

	constexpr CFixedPointNumber() = default;

private:
	Underlying m_Value;

	constexpr explicit CFixedPointNumber(const Underlying V) :
		m_Value(V) {}

	// Runtime parsing entry.
	static bool ParseRuntime(const char *p, Underlying &Out);

	// Compile-time literal parsing (fully defined here).
	static consteval bool ParseLiteral(const char *p, const size_t N, Underlying &Out)
	{
		// Minimal constexpr helpers.
		auto IsDigit = [](const char C) consteval { return C >= '0' && C <= '9'; };

		if(N == 0)
			return false;

		const char *Cur = p;
		const char *End = p + (N > 0 ? N - 1 : 0); // Exclude the trailing '\0'.
		if(Cur == End)
			return false;

		// Sign.
		bool Neg = false;
		if(*Cur == '+' || *Cur == '-')
		{
			Neg = (*Cur == '-');
			++Cur;
		}
		if(Cur == End)
			return false;

		// Whole part.
		Underlying Whole = 0;
		int DigitsWhole = 0;
		while(Cur < End && IsDigit(*Cur))
		{
			const int Digit = *Cur - '0';
			using U = Underlying;
			constexpr U MaxU = std::numeric_limits<U>::max();
			if(Whole > (MaxU - Digit) / 10)
				return false; // overflow
			Whole = Whole * 10 + Digit;
			++Cur;
			++DigitsWhole;
		}
		if(DigitsWhole == 0 && (Cur >= End || *Cur != '.'))
			return false;

		// Fractional part.
		int Frac = 0;
		int FracDigits = 0;
		int FirstDropped = -1;
		if(Cur < End && *Cur == '.')
		{
			++Cur;
			if(Cur == End)
			{
				// Allowed: "123." -> no fractional digits, treated as integer.
			}
			else if(!IsDigit(*Cur))
			{
				return false;
			}
			while(Cur < End && IsDigit(*Cur))
			{
				const int D = *Cur - '0';
				if(FracDigits < NUM_DECIMAL_DIGITS)
				{
					Frac = Frac * 10 + D;
					++FracDigits;
				}
				else if(FirstDropped < 0)
				{
					FirstDropped = D;
				}
				++Cur;
			}
		}

		// Must end exactly at End.
		if(Cur != End)
			return false;

		// Normalize to 3 digits (pad or round by the first dropped digit).
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
			if(FirstDropped >= 5)
			{
				Frac += 1;
				if(Frac >= SCALE)
					Frac = SCALE - 1;
			}
		}

		// Assemble and clamp.
		long long Mag = static_cast<long long>(Whole) * SCALE + static_cast<long long>(Frac);
		if(Neg)
			Mag = -Mag;

		constexpr long long MaxLL = std::numeric_limits<Underlying>::max();
		constexpr long long MinLL = std::numeric_limits<Underlying>::min();
		if(Mag > MaxLL)
			Out = std::numeric_limits<Underlying>::max();
		else if(Mag < MinLL)
			Out = std::numeric_limits<Underlying>::min();
		else
			Out = static_cast<Underlying>(Mag);

		return true;
	}
};

#endif // ENGINE_SHARED_FIXED_POINT_NUMBER_H
