#include "smooth_value.h"

#include <engine/client.h>
#include <engine/shared/config.h>

CSmoothValue::CSmoothValue(float InitialValue, float MinValue, float MaxValue) :
	m_Smoothing(false), m_Value(InitialValue), m_MinValue(MinValue), m_MaxValue(MaxValue) {}

void CSmoothValue::SetValue(float Target)
{
	Target = clamp(Target, m_MinValue, m_MaxValue);

	const float Now = Client()->GlobalTime();
	float Current = m_Value;
	float Derivative = 0.0f;
	if(m_Smoothing)
	{
		const float Progress = ZoomProgress(Now);
		Current = m_ValueSmoothing.Evaluate(Progress);
		Derivative = m_ValueSmoothing.Derivative(Progress);
	}

	m_ValueSmoothingTarget = Target;
	m_ValueSmoothing = CCubicBezier::With(Current, Derivative, 0.0f, m_ValueSmoothingTarget);
	m_ValueSmoothingStart = Now;
	m_ValueSmoothingEnd = Now + g_Config.m_EdSmoothZoomTime / 1000.0f;

	m_Smoothing = true;
}

void CSmoothValue::ChangeValue(float Amount)
{
	const float CurrentTarget = m_Smoothing ? m_ValueSmoothingTarget : m_Value;
	SetValue(CurrentTarget + Amount);
}

bool CSmoothValue::UpdateValue()
{
	if(m_Smoothing)
	{
		const float Time = Client()->GlobalTime();
		const float OldLevel = m_Value;
		if(Time >= m_ValueSmoothingEnd)
		{
			m_Value = m_ValueSmoothingTarget;
			m_Smoothing = false;
		}
		else
		{
			m_Value = m_ValueSmoothing.Evaluate(ZoomProgress(Time));
			if((OldLevel < m_ValueSmoothingTarget && m_Value > m_ValueSmoothingTarget) || (OldLevel > m_ValueSmoothingTarget && m_Value < m_ValueSmoothingTarget))
			{
				m_Value = m_ValueSmoothingTarget;
				m_Smoothing = false;
			}
		}
		m_Value = clamp(m_Value, m_MinValue, m_MaxValue);

		return true;
	}

	return false;
}

float CSmoothValue::ZoomProgress(float CurrentTime) const
{
	return (CurrentTime - m_ValueSmoothingStart) / (m_ValueSmoothingEnd - m_ValueSmoothingStart);
}

float CSmoothValue::GetValue() const
{
	return m_Value;
}

void CSmoothValue::SetValueInstant(float Target)
{
	m_Smoothing = false;
	m_Value = Target;
}

void CSmoothValue::SetValueRange(float MinValue, float MaxValue)
{
	m_MinValue = MinValue;
	m_MaxValue = MaxValue;
}

float CSmoothValue::GetMinValue() const
{
	return m_MinValue;
}

float CSmoothValue::GetMaxValue() const
{
	return m_MaxValue;
}
