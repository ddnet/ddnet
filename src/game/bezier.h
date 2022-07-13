#ifndef GAME_BEZIER_H
#define GAME_BEZIER_H

// Evaluates the Bernstein polynomial of degree 3/a one-dimensional Bezier curve
//
// https://en.wikipedia.org/w/index.php?title=Bernstein_polynomial&oldid=965314973
//
// f(t) = (1-t)³ a + 3(1-t)²t b + 3(1-t)t² c + t³ d
class CCubicBezier
{
	float a = 0;
	float b = 0;
	float c = 0;
	float d = 0;
	CCubicBezier(float a_, float b_, float c_, float d_) :
		a(a_), b(b_), c(c_), d(d_)
	{
	}

public:
	CCubicBezier() {}
	float Evaluate(float t) const;
	float Derivative(float t) const;
	static CCubicBezier With(float Start, float StartDerivative, float EndDerivative, float End);
};

#endif // GAME_BEZIER_H
