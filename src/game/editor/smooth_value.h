#ifndef GAME_EDITOR_SMOOTH_VALUE_H
#define GAME_EDITOR_SMOOTH_VALUE_H

#include <base/bezier.h>

#include "component.h"

/**
 * A value that is changed smoothly over time.
 */
class CSmoothValue : public CEditorComponent
{
public:
	CSmoothValue(float InitialValue, float MinValue, float MaxValue);

	/**
	 * Set a new target which the value should change to.
	 */
	void SetValue(float Target);

	/**
	 * Change the value by the given amount.
	 */
	void ChangeValue(float Amount);

	/**
	 * Set the value to the target instantly. If the value was changing the
	 * target will be discarded.
	 */
	void SetValueInstant(float Target);

	bool UpdateValue();

	float GetValue() const;
	void SetValueRange(float MinValue, float MaxValue);
	float GetMinValue() const;
	float GetMaxValue() const;

private:
	float ZoomProgress(float CurrentTime) const;

	bool m_Smoothing;
	float m_Value;
	CCubicBezier m_ValueSmoothing;
	float m_ValueSmoothingTarget;
	float m_ValueSmoothingStart;
	float m_ValueSmoothingEnd;

	float m_MinValue;
	float m_MaxValue;
};

#endif
