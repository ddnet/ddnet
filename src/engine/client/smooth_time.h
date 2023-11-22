/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef ENGINE_CLIENT_SMOOTH_TIME_H
#define ENGINE_CLIENT_SMOOTH_TIME_H

#include <cstdint>

class CGraph;

class CSmoothTime
{
public:
	enum EAdjustDirection
	{
		ADJUSTDIRECTION_DOWN = 0,
		ADJUSTDIRECTION_UP,
		NUM_ADJUSTDIRECTIONS,
	};

private:
	int64_t m_Snap;
	int64_t m_Current;
	int64_t m_Target;
	int64_t m_Margin;

	int m_SpikeCounter;
	float m_aAdjustSpeed[NUM_ADJUSTDIRECTIONS];

public:
	void Init(int64_t Target);
	void SetAdjustSpeed(EAdjustDirection Direction, float Value);

	int64_t Get(int64_t Now) const;

	void UpdateInt(int64_t Target);
	void Update(CGraph *pGraph, int64_t Target, int TimeLeft, EAdjustDirection AdjustDirection);

	void UpdateMargin(int64_t Margin);
};

#endif
