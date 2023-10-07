#ifndef GAME_EDITOR_MAPITEMS_ENVELOPE_POINT_H
#define GAME_EDITOR_MAPITEMS_ENVELOPE_POINT_H

#include <base/color.h>
#include <game/mapitems.h>

#include <array>

struct CTransform
{
	float OffsetX;
	float OffsetY;
	float Rotation;
};

struct CEnvelopePoint
{
	CEnvelopePoint(float Time, int Type, const std::array<float, CEnvPoint::MAX_CHANNELS> &Values);
	CEnvelopePoint(const CEnvPoint &EnvPoint, const CEnvPointBezier &Bezier);

	float Time() const;
	void SetTime(float Time);

	int TimeFixed() const;
	void SetTimeFixed(int Time);

	float Volume() const;
	void SetVolume(float Volume);

	CTransform Transform() const;
	void SetTransform(const CTransform &Transform);

	ColorRGBA Color() const;
	void SetColor(const ColorRGBA &Color);

	float Value(int Channel) const;
	void SetValue(int Channel, float Value);

	int CurveType() const;
	void SetCurveType(int Type);

	vec2 InTangentHandle(int Channel) const;
	void SetInTangentHandle(int Channel, vec2 TangentHandle);

	vec2 OutTangentHandle(int Channel) const;
	void SetOutTangentHandle(int Channel, vec2 TangentHandle);

	void ResetInTangentHandle(int Channel);
	void ResetOutTangentHandle(int Channel);

	bool operator<(const CEnvelopePoint &Other) const { return m_PointInfo < Other.m_PointInfo; }

	// TODO: make these protected and friend map io
	CEnvPointBezier m_Bezier;
	CEnvPoint m_PointInfo;
};

#endif
