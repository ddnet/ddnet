#include "bezier.h"

CCubicBezier CCubicBezier::With(float Start, float StartDerivative, float EndDerivative, float End)
{
	return CCubicBezier(Start, Start + StartDerivative / 3, End - EndDerivative / 3, End);
}

// f(t) = (1-t)³ a + 3(1-t)²t b + 3(1-t)t² c + t³ d
float CCubicBezier::Evaluate(float t) const
{
	return (1 - t) * (1 - t) * (1 - t) * a + 3 * (1 - t) * (1 - t) * t * b + 3 * (1 - t) * t * t * c + t * t * t * d;
}

// f(t) = 3(1-t)²(b-a) + 6(1-t)t(c-b) + 3t²(d-c)
float CCubicBezier::Derivative(float t) const
{
	return 3 * (1 - t) * (1 - t) * (b - a) + 6 * (1 - t) * t * (c - b) + 3 * t * t * (d - c);
}
