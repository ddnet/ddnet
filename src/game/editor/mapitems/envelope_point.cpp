#include "envelope_point.h"

float CEnvelopePoint::Time() const
{
	return fxt2f(m_Time);
}

void CEnvelopePoint::SetTime(float Time)
{
	m_Time = f2fxt(Time);
}

int CEnvelopePoint::TimeFixed() const
{
	return m_Time;
}

void CEnvelopePoint::SetTimeFixed(int Time)
{
	m_Time = Time;
}
