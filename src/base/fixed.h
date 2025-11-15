#ifndef BASE_FIXED_H
#define BASE_FIXED_H

#include <cstddef>
#include <cstdint>
#include <limits>

class CFixed
{
	using Underlying = int32_t;

	// Fixed-point format: 3 decimal places -> SCALE = 1000.
	static constexpr Underlying SCALE = 1000;
	static constexpr int NUM_DECIMAL_DIGITS = 3;

public:
	// Create from integer value.
	static constexpr CFixed FromInt(int Value) { return FromRaw(Value * SCALE); }

	// Runtime parse from C-string (strict: only [-]123 or [-]123.456, no spaces/suffixes/inf/nan).
	static bool FromStr(const char *pStr, CFixed &Out);

	// Compile-time parse for string literals.
	template<size_t N>
	static consteval CFixed FromLiteral(const char (&Lit)[N])
	{
		Underlying V = 0;
		if(!ParseLiteral(Lit, N, V))
		{
			return CFixed(0);
		}
		return FromRaw(V);
	}

	// Convert to C-string. Writes to the provided buffer.
	void AsStr(char *pBuffer, size_t BufferSize) const;

	// Conversions.
	[[nodiscard]] constexpr bool IsZero() const { return m_Value == 0; }
	constexpr explicit operator float() const { return static_cast<float>(m_Value) / static_cast<float>(SCALE); }

	// Comparisons.
	constexpr bool operator==(const CFixed &O) const { return m_Value == O.m_Value; }
	constexpr bool operator!=(const CFixed &O) const { return m_Value != O.m_Value; }
	constexpr bool operator<(const CFixed &O) const { return m_Value < O.m_Value; }
	constexpr bool operator<=(const CFixed &O) const { return m_Value <= O.m_Value; }
	constexpr bool operator>(const CFixed &O) const { return m_Value > O.m_Value; }
	constexpr bool operator>=(const CFixed &O) const { return m_Value >= O.m_Value; }

	constexpr CFixed() = default;

private:
	Underlying m_Value;

	constexpr explicit CFixed(const Underlying V) :
		m_Value(V) {}

	// Raw access.
	static constexpr CFixed FromRaw(const Underlying V) { return CFixed(V); }

	// Runtime parsing entry.
	static bool ParseRuntime(const char *p, Underlying &Out);

	// Helper functions for parsing.
	static bool AccumWholeProtect(Underlying &Whole, int Digit);
	static int RoundFrac3(int Frac, int FirstDroppedDigit);

	// Compile-time literal parsing (fully defined here).
	static consteval bool ParseLiteral(const char *p, const size_t N, Underlying &Out)
	{
		if(N <= 1)
			return false;

		const char *Cur = p;
		const char *End = p + (N - 1); // Exclude the trailing '\0'.
		if(Cur == End)
			return false;

		// Sign.
		bool Neg = false;
		if(Cur < End && (*Cur == '+' || *Cur == '-'))
		{
			Neg = (*Cur == '-');
			++Cur;
			if(Cur == End) // Just a sign with no digits
				return false;
		}

		// Whole part.
		Underlying Whole = 0;
		int DigitsWhole = 0;
		while(Cur < End && (*Cur >= '0' && *Cur <= '9'))
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

		// Fractional part.
		int Frac = 0;
		int FracDigits = 0;
		int FirstDropped = -1;
		if(Cur < End && *Cur == '.')
		{
			++Cur;
			while(Cur < End && (*Cur >= '0' && *Cur <= '9'))
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

		// Validation: must have at least one digit (whole or fractional)
		if(DigitsWhole == 0 && FracDigits == 0)
			return false;

		// Must have consumed entire string
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

#endif // BASE_FIXED_H
