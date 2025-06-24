#ifndef GAME_EDITOR_MAP_VIEW_H
#define GAME_EDITOR_MAP_VIEW_H

#include <base/vmath.h>

#include "component.h"
#include "map_grid.h"
#include "proof_mode.h"
#include "smooth_value.h"

class CLayerGroup;

class CMapView : public CEditorComponent
{
public:
	void OnInit(CEditor *pEditor) override;
	void OnReset() override;
	void OnMapLoad() override;

	void ZoomMouseTarget(float ZoomFactor);
	void UpdateZoom();

	void RenderGroupBorder();
	void RenderMap();

	bool IsFocused();
	void Focus();

	/**
	 * Reset zoom and editor offset.
	 */
	void ResetZoom();

	/**
	 * Scale length according to zoom value.
	 */
	float ScaleLength(float Value) const;

	bool m_ShowPicker; // TODO: make private

	float GetWorldZoom() const;

	void OffsetWorld(vec2 Offset);
	void OffsetEditor(vec2 Offset);
	void SetWorldOffset(vec2 WorldOffset);
	void SetEditorOffset(vec2 EditorOffset);
	vec2 GetWorldOffset() const;
	vec2 GetEditorOffset() const;

	CSmoothValue *Zoom();
	const CSmoothValue *Zoom() const;
	CProofMode *ProofMode();
	const CProofMode *ProofMode() const;
	CMapGrid *MapGrid();
	const CMapGrid *MapGrid() const;

private:
	CSmoothValue m_Zoom = CSmoothValue(200.0f, 10.0f, 2000.0f);
	float m_WorldZoom;

	CProofMode m_ProofMode;
	CMapGrid m_MapGrid;

	vec2 m_WorldOffset;
	vec2 m_EditorOffset;
};

#endif
