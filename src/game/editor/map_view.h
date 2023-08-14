#ifndef GAME_EDITOR_MAP_VIEW_H
#define GAME_EDITOR_MAP_VIEW_H

#include <base/vmath.h>

#include "component.h"
#include "smooth_value.h"

class CMapView : public CEditorComponent
{
public:
	void Init(CEditor *pEditor) override;

	/**
	 * Reset zoom and editor offset.
	 */
	void ResetZoom();

	/**
	 * Scale length according to zoom value.
	 */
	float ScaleLength(float Value);

	void ZoomMouseTarget(float ZoomFactor);
	void UpdateZoom();

	vec2 m_WorldOffset;
	vec2 m_EditorOffset;

	CSmoothValue m_Zoom = CSmoothValue(200.0f, 10.0f, 2000.0f);
	float m_WorldZoom = 1.0f;
};

#endif
