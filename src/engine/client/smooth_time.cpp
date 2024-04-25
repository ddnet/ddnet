/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/math.h>
#include <base/system.h>

#include "graph.h"
#include "smooth_time.h"

void CSmoothTime::Init(int64_t Target)
{
	m_Snap = time_get();
	m_Current = Target;
	m_Target = Target;
	m_Margin = 0;
	m_SpikeCounter = 0;
	m_aAdjustSpeed[ADJUSTDIRECTION_DOWN] = 0.3f;
	m_aAdjustSpeed[ADJUSTDIRECTION_UP] = 0.3f;
}

void CSmoothTime::SetAdjustSpeed(EAdjustDirection Direction, float Value)
{
	m_aAdjustSpeed[Direction] = Value;
}

int64_t CSmoothTime::Get(int64_t Now) const
{
	int64_t c = m_Current + (Now - m_Snap);
	int64_t t = m_Target + (Now - m_Snap);

	// it's faster to adjust upward instead of downward
	// we might need to adjust these abit

	float AdjustSpeed = m_aAdjustSpeed[ADJUSTDIRECTION_DOWN];
	if(t > c)
		AdjustSpeed = m_aAdjustSpeed[ADJUSTDIRECTION_UP];

	float a = ((Now - m_Snap) / (float)time_freq()) * AdjustSpeed;
	if(a > 1.0f)
		a = 1.0f;

	int64_t r = c + (int64_t)((t - c) * a);
	return r + m_Margin;
}

void CSmoothTime::UpdateInt(int64_t Target)
{
	int64_t Now = time_get();
	m_Current = Get(Now) - m_Margin;
	m_Snap = Now;
	m_Target = Target;
}

void CSmoothTime::Update(CGraph *pGraph, int64_t Target, int TimeLeft, EAdjustDirection AdjustDirection)
{
	bool UpdateTimer = true;

	if(TimeLeft < 0)
	{
		bool IsSpike = false;
		if(TimeLeft < -50)
		{
			IsSpike = true;

			m_SpikeCounter += 5;
			if(m_SpikeCounter > 50)
				m_SpikeCounter = 50;
		}

		if(IsSpike && m_SpikeCounter < 15)
		{
			// ignore this ping spike
			UpdateTimer = false;
			pGraph->Add(TimeLeft, ColorRGBA(1.0f, 1.0f, 0.0f, 0.75f));
		}
		else
		{
			pGraph->Add(TimeLeft, ColorRGBA(1.0f, 0.0f, 0.0f, 0.75f));
			if(m_aAdjustSpeed[AdjustDirection] < 30.0f)
				m_aAdjustSpeed[AdjustDirection] *= 2.0f;
		}
	}
	else
	{
		if(m_SpikeCounter)
			m_SpikeCounter--;

		pGraph->Add(TimeLeft, ColorRGBA(0.0f, 1.0f, 0.0f, 0.75f));

		m_aAdjustSpeed[AdjustDirection] *= 0.95f;
		if(m_aAdjustSpeed[AdjustDirection] < 2.0f)
			m_aAdjustSpeed[AdjustDirection] = 2.0f;
	}

	if(UpdateTimer)
		UpdateInt(Target);
}

void CSmoothTime::UpdateMargin(int64_t Margin)
{
	m_Margin = Margin;
}
