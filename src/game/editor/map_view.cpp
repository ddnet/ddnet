#include "map_view.h"

#include "editor.h"

#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/ui.h>

void CMapView::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);
	RegisterSubComponent(m_MapGrid);
	RegisterSubComponent(m_ProofMode);
	InitSubComponents();
}

void CMapView::OnReset()
{
	m_Zoom = CSmoothValue(200.0f, 10.0f, 2000.0f);
	m_Zoom.OnInit(Editor());
	m_WorldZoom = 1.0f;

	SetWorldOffset({0, 0});
	SetEditorOffset({0, 0});

	m_ProofMode.OnReset();
	m_MapGrid.OnReset();
}

void CMapView::OnMapLoad()
{
	m_ProofMode.OnMapLoad();
}

bool CMapView::IsFocused()
{
	return GetWorldOffset() == (m_ProofMode.IsModeMenu() ? m_ProofMode.CurrentMenuBackgroundPosition() : vec2(0.0f, 0.0f));
}

void CMapView::Focus()
{
	SetWorldOffset(m_ProofMode.IsModeMenu() ? m_ProofMode.CurrentMenuBackgroundPosition() : vec2(0.0f, 0.0f));
}

void CMapView::RenderGroupBorder()
{
	std::shared_ptr<CLayerGroup> pGroup = Map()->SelectedGroup();
	if(pGroup)
	{
		pGroup->MapScreen();

		for(size_t i = 0; i < Map()->m_vSelectedLayers.size(); i++)
		{
			std::shared_ptr<CLayer> pLayer = Map()->SelectedLayerType(i, LAYERTYPE_TILES);
			if(pLayer)
			{
				CUIRect BorderRect;
				BorderRect.x = 0.0f;
				BorderRect.y = 0.0f;
				pLayer->GetSize(&BorderRect.w, &BorderRect.h);
				BorderRect.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
			}
		}
	}
}

void CMapView::RenderEditorMap()
{
	if(Editor()->m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && Input()->KeyPress(KEY_G))
	{
		const bool AnyHidden =
			!Map()->m_pGameLayer->m_Visible ||
			(Map()->m_pFrontLayer && !Map()->m_pFrontLayer->m_Visible) ||
			(Map()->m_pTeleLayer && !Map()->m_pTeleLayer->m_Visible) ||
			(Map()->m_pSpeedupLayer && !Map()->m_pSpeedupLayer->m_Visible) ||
			(Map()->m_pTuneLayer && !Map()->m_pTuneLayer->m_Visible) ||
			(Map()->m_pSwitchLayer && !Map()->m_pSwitchLayer->m_Visible);
		Map()->m_pGameLayer->m_Visible = AnyHidden;
		if(Map()->m_pFrontLayer)
			Map()->m_pFrontLayer->m_Visible = AnyHidden;
		if(Map()->m_pTeleLayer)
			Map()->m_pTeleLayer->m_Visible = AnyHidden;
		if(Map()->m_pSpeedupLayer)
			Map()->m_pSpeedupLayer->m_Visible = AnyHidden;
		if(Map()->m_pTuneLayer)
			Map()->m_pTuneLayer->m_Visible = AnyHidden;
		if(Map()->m_pSwitchLayer)
			Map()->m_pSwitchLayer->m_Visible = AnyHidden;
	}

	for(auto &pGroup : Map()->m_vpGroups)
	{
		if(pGroup->m_Visible)
			pGroup->Render();
	}

	// render the game, tele, speedup, front, tune and switch above everything else
	if(Map()->m_pGameGroup->m_Visible)
	{
		Map()->m_pGameGroup->MapScreen();
		for(auto &pLayer : Map()->m_pGameGroup->m_vpLayers)
		{
			if(pLayer->m_Visible && pLayer->IsEntitiesLayer())
				pLayer->Render();
		}
	}

	std::shared_ptr<CLayerTiles> pSelectedTilesLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
	if(Editor()->m_ShowTileInfo != CEditor::SHOW_TILE_OFF && pSelectedTilesLayer && pSelectedTilesLayer->m_Visible && m_Zoom.GetValue() <= 300.0f)
	{
		Map()->SelectedGroup()->MapScreen();
		pSelectedTilesLayer->ShowInfo();
	}
}

void CMapView::ResetZoom()
{
	SetEditorOffset({0, 0});
	m_Zoom.SetValue(100.0f);
}

float CMapView::ScaleLength(float Value) const
{
	return m_WorldZoom * Value;
}

void CMapView::ZoomMouseTarget(float ZoomFactor)
{
	// zoom to the current mouse position
	// get absolute mouse position
	float aPoints[4];
	Graphics()->MapScreenToWorld(
		GetWorldOffset().x, GetWorldOffset().y,
		100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Graphics()->ScreenAspect(), m_WorldZoom, aPoints);

	float WorldWidth = aPoints[2] - aPoints[0];
	float WorldHeight = aPoints[3] - aPoints[1];

	float MouseWorldX = aPoints[0] + WorldWidth * (Ui()->MouseX() / Ui()->Screen()->w);
	float MouseWorldY = aPoints[1] + WorldHeight * (Ui()->MouseY() / Ui()->Screen()->h);

	// adjust camera
	OffsetWorld((vec2(MouseWorldX, MouseWorldY) - GetWorldOffset()) * (1.0f - ZoomFactor));
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

CSmoothValue *CMapView::Zoom()
{
	return &m_Zoom;
}

const CSmoothValue *CMapView::Zoom() const
{
	return &m_Zoom;
}

CProofMode *CMapView::ProofMode()
{
	return &m_ProofMode;
}

const CProofMode *CMapView::ProofMode() const
{
	return &m_ProofMode;
}

CMapGrid *CMapView::MapGrid()
{
	return &m_MapGrid;
}

const CMapGrid *CMapView::MapGrid() const
{
	return &m_MapGrid;
}

void CMapView::OffsetWorld(vec2 Offset)
{
	m_WorldOffset += Offset;
}

void CMapView::OffsetEditor(vec2 Offset)
{
	m_EditorOffset += Offset;
}

void CMapView::SetWorldOffset(vec2 WorldOffset)
{
	m_WorldOffset = WorldOffset;
}

void CMapView::SetEditorOffset(vec2 EditorOffset)
{
	m_EditorOffset = EditorOffset;
}

vec2 CMapView::GetWorldOffset() const
{
	return m_WorldOffset;
}

vec2 CMapView::GetEditorOffset() const
{
	return m_EditorOffset;
}

float CMapView::GetWorldZoom() const
{
	return m_WorldZoom;
}
