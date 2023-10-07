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

struct CEnvelopePoint : public CEnvPoint
{
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

	CEnvPointBezier m_Bezier;
};

#endif
