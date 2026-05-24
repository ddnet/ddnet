#ifndef GAME_EDITOR_MAP_VIEW_H
#define GAME_EDITOR_MAP_VIEW_H

#include "component.h"
#include "map_grid.h"
#include "proof_mode.h"
#include "smooth_value.h"

#include <base/vmath.h>

class CLayerGroup;

class CMapView : public CEditorComponent
{
public:
	enum class EActiveOp
	{
		NONE,
		BRUSH_GRAB,
		BRUSH_DRAW,
		BRUSH_PAINT,
		PAN_WORLD,
		PAN_EDITOR,
	};

	class CState
	{
	public:
		CSmoothValue m_Zoom;
		float m_WorldZoom;
		vec2 m_WorldOffset;
		vec2 m_EditorOffset;

		float m_MouseWorldScale; // Mouse (i.e. UI) scale relative to the World (selected Group)
		vec2 m_MouseWorldPos;
		vec2 m_MouseWorldNoParaPos;
		vec2 m_MouseDeltaWorld;

		EActiveOp m_ActiveOp;

		void Reset(CEditor *pEditor);
	};

	void OnInit(CEditor *pEditor) override;
	void OnMapLoad() override;

	void ZoomMouseTarget(float ZoomFactor);
	void UpdateZoom();

	void RenderGroupBorder();
	void RenderEditorMap();
	void Render(CUIRect View);

	void UpdateMouseWorld();
	void ResetMouseDeltaWorld();
	float MouseWorldScale() const;
	vec2 MouseDeltaWorld() const;
	vec2 MouseWorldPos() const;
	vec2 MouseWorldNoParaPos() const;

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
	CProofMode m_ProofMode;
	CMapGrid m_MapGrid;
};

#endif
