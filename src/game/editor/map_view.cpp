#include "map_view.h"

#include <engine/shared/config.h>

#include <game/client/render.h>
#include <game/client/ui.h>

void CMapView::Init(CEditor *pEditor)
{
	CEditorComponent::Init(pEditor);
	m_Zoom.Init(pEditor);
}

void CMapView::ResetZoom()
{
	m_EditorOffset = vec2(0, 0);
	m_Zoom.SetValue(100.0f);
}

float CMapView::ScaleLength(float Value)
{
	return m_WorldZoom * Value;
}

void CMapView::ZoomMouseTarget(float ZoomFactor)
{
	// zoom to the current mouse position
	// get absolute mouse position
	float aPoints[4];
	RenderTools()->MapScreenToWorld(
		m_WorldOffset.x, m_WorldOffset.y,
		100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Graphics()->ScreenAspect(), m_WorldZoom, aPoints);

	float WorldWidth = aPoints[2] - aPoints[0];
	float WorldHeight = aPoints[3] - aPoints[1];

	float Mwx = aPoints[0] + WorldWidth * (UI()->MouseX() / UI()->Screen()->w);
	float Mwy = aPoints[1] + WorldHeight * (UI()->MouseY() / UI()->Screen()->h);

	// adjust camera
	m_WorldOffset += (vec2(Mwx, Mwy) - m_WorldOffset) * (1.0f - ZoomFactor);
}

void CMapView::UpdateZoom()
{
	float OldLevel = m_Zoom.GetValue();
	bool UpdatedZoom = m_Zoom.UpdateValue();
	m_Zoom.SetValueRange(10.0f, g_Config.m_EdLimitMaxZoomLevel ? 2000.0f : std::numeric_limits<float>::max());
	float NewLevel = m_Zoom.GetValue();
	if(UpdatedZoom && g_Config.m_EdZoomTarget)
		ZoomMouseTarget(NewLevel / OldLevel);
	m_WorldZoom = NewLevel / 100.0f;
}
