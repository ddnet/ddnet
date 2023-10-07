#include "envelope_point.h"

#include <base/system.h>

CEnvelopePoint::CEnvelopePoint(float Time, int Type, const std::array<float, CEnvPoint::MAX_CHANNELS> &Values) :
	m_Bezier{}
{
	SetTime(Time);
	SetCurveType(Type);

	for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
		SetValue(c, Values.at(c));
}

CEnvelopePoint::CEnvelopePoint(const CEnvPoint &EnvPoint, const CEnvPointBezier &Bezier) :
	m_Bezier(Bezier), m_PointInfo(EnvPoint) {}

float CEnvelopePoint::Time() const
{
	return fxt2f(m_PointInfo.m_Time);
}

void CEnvelopePoint::SetTime(float Time)
{
	m_PointInfo.m_Time = f2fxt(Time);
}

int CEnvelopePoint::TimeFixed() const
{
	return m_PointInfo.m_Time;
}

void CEnvelopePoint::SetTimeFixed(int Time)
{
	m_PointInfo.m_Time = Time;
}

float CEnvelopePoint::Volume() const
{
	return fx2f(m_PointInfo.m_aValues[0]);
}

void CEnvelopePoint::SetVolume(float Volume)
{
	m_PointInfo.m_aValues[0] = f2fx(Volume);
}

CTransform CEnvelopePoint::Transform() const
{
	// TODO: return in gradients or degrees?
	return {fx2f(m_PointInfo.m_aValues[0]), fx2f(m_PointInfo.m_aValues[1]), fx2f(m_PointInfo.m_aValues[2]) / 360.0f * pi * 2};
}

void CEnvelopePoint::SetTransform(const CTransform &Transform)
{
	m_PointInfo.m_aValues[0] = f2fx(Transform.OffsetX);
	m_PointInfo.m_aValues[1] = f2fx(Transform.OffsetY);
	m_PointInfo.m_aValues[2] = f2fx(Transform.Rotation / pi / 2 * 360.0f);
}

ColorRGBA CEnvelopePoint::Color() const
{
	return {fx2f(m_PointInfo.m_aValues[0]), fx2f(m_PointInfo.m_aValues[1]), fx2f(m_PointInfo.m_aValues[2]), fx2f(m_PointInfo.m_aValues[3])};
}

void CEnvelopePoint::SetColor(const ColorRGBA &Color)
{
	m_PointInfo.m_aValues[0] = f2fx(Color.r);
	m_PointInfo.m_aValues[1] = f2fx(Color.g);
	m_PointInfo.m_aValues[2] = f2fx(Color.b);
	m_PointInfo.m_aValues[3] = f2fx(Color.a);
}

float CEnvelopePoint::Value(int Channel) const
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	return fx2f(m_PointInfo.m_aValues[Channel]);
}

void CEnvelopePoint::SetValue(int Channel, float Value)
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	m_PointInfo.m_aValues[Channel] = f2fx(Value);
}

int CEnvelopePoint::CurveType() const
{
	return m_PointInfo.m_Curvetype;
}

void CEnvelopePoint::SetCurveType(int Type)
{
	m_PointInfo.m_Curvetype = Type;
}

vec2 CEnvelopePoint::InTangentHandle(int Channel) const
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	return {Time() + fxt2f(m_Bezier.m_aInTangentDeltaX[Channel]), Value(Channel) + fx2f(m_Bezier.m_aInTangentDeltaY[Channel])};
}

void CEnvelopePoint::SetInTangentHandle(int Channel, vec2 TangentHandle)
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	m_Bezier.m_aInTangentDeltaX[Channel] = f2fx(TangentHandle.x - Time());
	m_Bezier.m_aInTangentDeltaY[Channel] = f2fx(TangentHandle.y - Value(Channel));
}

vec2 CEnvelopePoint::OutTangentHandle(int Channel) const
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	return {Time() + fxt2f(m_Bezier.m_aOutTangentDeltaX[Channel]), Value(Channel) + fx2f(m_Bezier.m_aOutTangentDeltaY[Channel])};
}

void CEnvelopePoint::SetOutTangentHandle(int Channel, vec2 TangentHandle)
{
	dbg_assert(0 <= Channel && Channel <= CEnvPoint::MAX_CHANNELS, "channel index too big");
	m_Bezier.m_aOutTangentDeltaX[Channel] = f2fx(TangentHandle.x - Time());
	m_Bezier.m_aOutTangentDeltaY[Channel] = f2fx(TangentHandle.y - Value(Channel));
}

void CEnvelopePoint::ResetInTangentHandle(int Channel)
{
	m_Bezier.m_aInTangentDeltaX[Channel] = 0;
	m_Bezier.m_aInTangentDeltaY[Channel] = 0;
}

void CEnvelopePoint::ResetOutTangentHandle(int Channel)
{
	m_Bezier.m_aOutTangentDeltaY[Channel] = 0;
	m_Bezier.m_aOutTangentDeltaX[Channel] = 0;
}
