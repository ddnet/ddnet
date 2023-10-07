#include "envelope_point.h"

#include <base/system.h>

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

float CEnvelopePoint::Volume() const
{
	return fx2f(m_aValues[0]);
}

void CEnvelopePoint::SetVolume(float Volume)
{
	m_aValues[0] = f2fx(Volume);
}

CTransform CEnvelopePoint::Transform() const
{
	// TODO: return in gradients or degrees?
	return {fx2f(m_aValues[0]), fx2f(m_aValues[1]), fx2f(m_aValues[2]) / 360.0f * pi * 2};
}

void CEnvelopePoint::SetTransform(const CTransform &Transform)
{
	m_aValues[0] = f2fx(Transform.OffsetX);
	m_aValues[1] = f2fx(Transform.OffsetY);
	m_aValues[2] = f2fx(Transform.Rotation / pi / 2 * 360.0f);
}

ColorRGBA CEnvelopePoint::Color() const
{
	return {fx2f(m_aValues[0]), fx2f(m_aValues[1]), fx2f(m_aValues[2]), fx2f(m_aValues[3])};
}

void CEnvelopePoint::SetColor(const ColorRGBA &Color)
{
	m_aValues[0] = f2fx(Color.r);
	m_aValues[1] = f2fx(Color.g);
	m_aValues[2] = f2fx(Color.b);
	m_aValues[3] = f2fx(Color.a);
}

float CEnvelopePoint::Value(int Channel) const
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	return fx2f(m_aValues[Channel]);
}

void CEnvelopePoint::SetValue(int Channel, float Value)
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	m_aValues[Channel] = f2fx(Value);
}

int CEnvelopePoint::CurveType() const
{
	return m_Curvetype;
}

void CEnvelopePoint::SetCurveType(int Type)
{
	m_Curvetype = Type;
}
