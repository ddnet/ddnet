#ifndef GAME_EDITOR_MAPITEMS_ENVELOPE_POINT_H
#define GAME_EDITOR_MAPITEMS_ENVELOPE_POINT_H

#include <game/mapitems.h>

struct CEnvelopePoint : public CEnvPoint
{
	float Time() const;
	void SetTime(float Time);

	int TimeFixed() const;
	void SetTimeFixed(int Time);

	CEnvPointBezier m_Bezier;
};

#endif
