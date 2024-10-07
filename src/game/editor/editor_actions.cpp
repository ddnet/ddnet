#include "editor_actions.h"
#include <game/editor/mapitems/image.h>

CEditorBrushDrawAction::CEditorBrushDrawAction(CEditor *pEditor, int Group) :
	IEditorAction(pEditor), m_Group(Group)
{
	auto &Map = pEditor->m_Map;
	for(size_t k = 0; k < Map.m_vpGroups[Group]->m_vpLayers.size(); k++)
	{
		auto pLayer = Map.m_vpGroups[Group]->m_vpLayers[k];

		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			auto pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);

			if(pLayer == Map.m_pTeleLayer)
			{
				if(!Map.m_pTeleLayer->m_History.empty())
				{
					m_TeleTileChanges = std::map(Map.m_pTeleLayer->m_History);
					Map.m_pTeleLayer->ClearHistory();
				}
			}
			else if(pLayer == Map.m_pTuneLayer)
			{
				if(!Map.m_pTuneLayer->m_History.empty())
				{
					m_TuneTileChanges = std::map(Map.m_pTuneLayer->m_History);
					Map.m_pTuneLayer->ClearHistory();
				}
			}
			else if(pLayer == Map.m_pSwitchLayer)
			{
				if(!Map.m_pSwitchLayer->m_History.empty())
				{
					m_SwitchTileChanges = std::map(Map.m_pSwitchLayer->m_History);
					Map.m_pSwitchLayer->ClearHistory();
				}
			}
			else if(pLayer == Map.m_pSpeedupLayer)
			{
				if(!Map.m_pSpeedupLayer->m_History.empty())
				{
					m_SpeedupTileChanges = std::map(Map.m_pSpeedupLayer->m_History);
					Map.m_pSpeedupLayer->ClearHistory();
				}
			}

			if(!pLayerTiles->m_TilesHistory.empty())
			{
				m_vTileChanges.emplace_back(k, std::map(pLayerTiles->m_TilesHistory));
				pLayerTiles->ClearHistory();
			}
		}
	}

	SetInfos();
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Brush draw (x%d) on %d layers", m_TotalTilesDrawn, m_TotalLayers);
}

void CEditorBrushDrawAction::SetInfos()
{
	m_TotalTilesDrawn = 0;
	m_TotalLayers = 0;

	// Process normal tiles
	for(auto const &Pair : m_vTileChanges)
	{
		int Layer = Pair.first;
		std::shared_ptr<CLayer> pLayer = m_pEditor->m_Map.m_vpGroups[m_Group]->m_vpLayers[Layer];
		m_TotalLayers++;

		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			auto Changes = Pair.second;
			for(auto &Change : Changes)
			{
				m_TotalTilesDrawn += Change.second.size();
			}
		}
	}

	// Process speedup tiles
	for(auto const &SpeedupChange : m_SpeedupTileChanges)
	{
		m_TotalTilesDrawn += SpeedupChange.second.size();
	}

	// Process tele tiles
	for(auto const &TeleChange : m_TeleTileChanges)
	{
		m_TotalTilesDrawn += TeleChange.second.size();
	}

	// Process switch tiles
	for(auto const &SwitchChange : m_SwitchTileChanges)
	{
		m_TotalTilesDrawn += SwitchChange.second.size();
	}

	// Process tune tiles
	for(auto const &TuneChange : m_TuneTileChanges)
	{
		m_TotalTilesDrawn += TuneChange.second.size();
	}

	m_TotalLayers += !m_SpeedupTileChanges.empty();
	m_TotalLayers += !m_SwitchTileChanges.empty();
	m_TotalLayers += !m_TeleTileChanges.empty();
	m_TotalLayers += !m_TuneTileChanges.empty();
}

bool CEditorBrushDrawAction::IsEmpty()
{
	return m_vTileChanges.empty() && m_SpeedupTileChanges.empty() && m_SwitchTileChanges.empty() && m_TeleTileChanges.empty() && m_TuneTileChanges.empty();
}

void CEditorBrushDrawAction::Undo()
{
	Apply(true);
}

void CEditorBrushDrawAction::Redo()
{
	Apply(false);
}

void CEditorBrushDrawAction::Apply(bool Undo)
{
	auto &Map = m_pEditor->m_Map;

	// Process normal tiles
	for(auto const &Pair : m_vTileChanges)
	{
		int Layer = Pair.first;
		std::shared_ptr<CLayer> pLayer = Map.m_vpGroups[m_Group]->m_vpLayers[Layer];

		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			auto Changes = Pair.second;
			for(auto &Change : Changes)
			{
				int y = Change.first;
				auto Line = Change.second;
				for(auto &Tile : Line)
				{
					int x = Tile.first;
					STileStateChange State = Tile.second;
					pLayerTiles->SetTileIgnoreHistory(x, y, Undo ? State.m_Previous : State.m_Current);
				}
			}
		}
	}

	// Process speedup tiles
	for(auto const &SpeedupChange : m_SpeedupTileChanges)
	{
		int y = SpeedupChange.first;
		auto Line = SpeedupChange.second;
		for(auto &Tile : Line)
		{
			int x = Tile.first;
			int Index = y * Map.m_pSpeedupLayer->m_Width + x;
			SSpeedupTileStateChange State = Tile.second;
			SSpeedupTileStateChange::SData Data = Undo ? State.m_Previous : State.m_Current;

			Map.m_pSpeedupLayer->m_pSpeedupTile[Index].m_Force = Data.m_Force;
			Map.m_pSpeedupLayer->m_pSpeedupTile[Index].m_MaxSpeed = Data.m_MaxSpeed;
			Map.m_pSpeedupLayer->m_pSpeedupTile[Index].m_Angle = Data.m_Angle;
			Map.m_pSpeedupLayer->m_pSpeedupTile[Index].m_Type = Data.m_Type;
			Map.m_pSpeedupLayer->m_pTiles[Index].m_Index = Data.m_Index;
		}
	}

	// Process tele tiles
	for(auto const &TeleChange : m_TeleTileChanges)
	{
		int y = TeleChange.first;
		auto Line = TeleChange.second;
		for(auto &Tile : Line)
		{
			int x = Tile.first;
			int Index = y * Map.m_pTeleLayer->m_Width + x;
			STeleTileStateChange State = Tile.second;
			STeleTileStateChange::SData Data = Undo ? State.m_Previous : State.m_Current;

			Map.m_pTeleLayer->m_pTeleTile[Index].m_Number = Data.m_Number;
			Map.m_pTeleLayer->m_pTeleTile[Index].m_Type = Data.m_Type;
			Map.m_pTeleLayer->m_pTiles[Index].m_Index = Data.m_Index;
		}
	}

	// Process switch tiles
	for(auto const &SwitchChange : m_SwitchTileChanges)
	{
		int y = SwitchChange.first;
		auto Line = SwitchChange.second;
		for(auto &Tile : Line)
		{
			int x = Tile.first;
			int Index = y * Map.m_pSwitchLayer->m_Width + x;
			SSwitchTileStateChange State = Tile.second;
			SSwitchTileStateChange::SData Data = Undo ? State.m_Previous : State.m_Current;

			Map.m_pSwitchLayer->m_pSwitchTile[Index].m_Number = Data.m_Number;
			Map.m_pSwitchLayer->m_pSwitchTile[Index].m_Type = Data.m_Type;
			Map.m_pSwitchLayer->m_pSwitchTile[Index].m_Flags = Data.m_Flags;
			Map.m_pSwitchLayer->m_pSwitchTile[Index].m_Delay = Data.m_Delay;
			Map.m_pSwitchLayer->m_pTiles[Index].m_Index = Data.m_Index;
		}
	}

	// Process tune tiles
	for(auto const &TuneChange : m_TuneTileChanges)
	{
		int y = TuneChange.first;
		auto Line = TuneChange.second;
		for(auto &Tile : Line)
		{
			int x = Tile.first;
			int Index = y * Map.m_pTuneLayer->m_Width + x;
			STuneTileStateChange State = Tile.second;
			STuneTileStateChange::SData Data = Undo ? State.m_Previous : State.m_Current;

			Map.m_pTuneLayer->m_pTuneTile[Index].m_Number = Data.m_Number;
			Map.m_pTuneLayer->m_pTuneTile[Index].m_Type = Data.m_Type;
			Map.m_pTuneLayer->m_pTiles[Index].m_Index = Data.m_Index;
		}
	}
}

// -------------------------------------------

CEditorActionQuadPlace::CEditorActionQuadPlace(CEditor *pEditor, int GroupIndex, int LayerIndex, std::vector<CQuad> &vBrush) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_vBrush(vBrush)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Quad place (x%d)", (int)m_vBrush.size());
}

void CEditorActionQuadPlace::Undo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	for(size_t k = 0; k < m_vBrush.size(); k++)
		pLayerQuads->m_vQuads.pop_back();

	m_pEditor->m_Map.OnModify();
}
void CEditorActionQuadPlace::Redo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	for(auto &Brush : m_vBrush)
		pLayerQuads->m_vQuads.push_back(Brush);

	m_pEditor->m_Map.OnModify();
}

CEditorActionSoundPlace::CEditorActionSoundPlace(CEditor *pEditor, int GroupIndex, int LayerIndex, std::vector<CSoundSource> &vBrush) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_vBrush(vBrush)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Sound place (x%d)", (int)m_vBrush.size());
}

void CEditorActionSoundPlace::Undo()
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	for(size_t k = 0; k < m_vBrush.size(); k++)
		pLayerSounds->m_vSources.pop_back();

	m_pEditor->m_Map.OnModify();
}

void CEditorActionSoundPlace::Redo()
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	for(auto &Brush : m_vBrush)
		pLayerSounds->m_vSources.push_back(Brush);

	m_pEditor->m_Map.OnModify();
}

// ---------------------------------------------------------------------------------------

CEditorActionDeleteQuad::CEditorActionDeleteQuad(CEditor *pEditor, int GroupIndex, int LayerIndex, std::vector<int> const &vQuadsIndices, std::vector<CQuad> const &vDeletedQuads) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_vQuadsIndices(vQuadsIndices), m_vDeletedQuads(vDeletedQuads)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Delete quad (x%d)", (int)m_vDeletedQuads.size());
}

void CEditorActionDeleteQuad::Undo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	for(size_t k = 0; k < m_vQuadsIndices.size(); k++)
	{
		pLayerQuads->m_vQuads.insert(pLayerQuads->m_vQuads.begin() + m_vQuadsIndices[k], m_vDeletedQuads[k]);
	}
}

void CEditorActionDeleteQuad::Redo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	std::vector<int> vQuads(m_vQuadsIndices);

	for(int i = 0; i < (int)vQuads.size(); ++i)
	{
		pLayerQuads->m_vQuads.erase(pLayerQuads->m_vQuads.begin() + vQuads[i]);
		for(int j = i + 1; j < (int)vQuads.size(); ++j)
			if(vQuads[j] > vQuads[i])
				vQuads[j]--;

		vQuads.erase(vQuads.begin() + i);

		i--;
	}
}

// ---------------------------------------------------------------------------------------

CEditorActionEditQuadPoint::CEditorActionEditQuadPoint(CEditor *pEditor, int GroupIndex, int LayerIndex, int QuadIndex, std::vector<CPoint> const &vPreviousPoints, std::vector<CPoint> const &vCurrentPoints) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_QuadIndex(QuadIndex), m_vPreviousPoints(vPreviousPoints), m_vCurrentPoints(vCurrentPoints)
{
	str_copy(m_aDisplayText, "Edit quad points");
}

void CEditorActionEditQuadPoint::Undo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	CQuad &Quad = pLayerQuads->m_vQuads[m_QuadIndex];
	for(int k = 0; k < 5; k++)
		Quad.m_aPoints[k] = m_vPreviousPoints[k];
}

void CEditorActionEditQuadPoint::Redo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	CQuad &Quad = pLayerQuads->m_vQuads[m_QuadIndex];
	for(int k = 0; k < 5; k++)
		Quad.m_aPoints[k] = m_vCurrentPoints[k];
}

CEditorActionEditQuadProp::CEditorActionEditQuadProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int QuadIndex, EQuadProp Prop, int Previous, int Current) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_QuadIndex(QuadIndex), m_Prop(Prop), m_Previous(Previous), m_Current(Current)
{
	static const char *s_apNames[] = {
		"order",
		"pos X",
		"pos Y",
		"pos env",
		"pos env offset",
		"color env",
		"color env offset"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit quad %s property in layer %d of group %d", s_apNames[(int)m_Prop], m_LayerIndex, m_GroupIndex);
}

void CEditorActionEditQuadProp::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEditQuadProp::Redo()
{
	Apply(m_Current);
}

void CEditorActionEditQuadProp::Apply(int Value)
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	CQuad &Quad = pLayerQuads->m_vQuads[m_QuadIndex];
	if(m_Prop == EQuadProp::PROP_POS_ENV)
		Quad.m_PosEnv = Value;
	else if(m_Prop == EQuadProp::PROP_POS_ENV_OFFSET)
		Quad.m_PosEnvOffset = Value;
	else if(m_Prop == EQuadProp::PROP_COLOR_ENV)
		Quad.m_ColorEnv = Value;
	else if(m_Prop == EQuadProp::PROP_COLOR_ENV_OFFSET)
		Quad.m_ColorEnvOffset = Value;
}

CEditorActionEditQuadPointProp::CEditorActionEditQuadPointProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int QuadIndex, int PointIndex, EQuadPointProp Prop, int Previous, int Current) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_QuadIndex(QuadIndex), m_PointIndex(PointIndex), m_Prop(Prop), m_Previous(Previous), m_Current(Current)
{
	static const char *s_apNames[] = {
		"pos X",
		"pos Y",
		"color",
		"tex U",
		"tex V"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit quad point %s property in layer %d of group %d", s_apNames[(int)m_Prop], m_LayerIndex, m_GroupIndex);
}

void CEditorActionEditQuadPointProp::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEditQuadPointProp::Redo()
{
	Apply(m_Current);
}

void CEditorActionEditQuadPointProp::Apply(int Value)
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	CQuad &Quad = pLayerQuads->m_vQuads[m_QuadIndex];

	if(m_Prop == EQuadPointProp::PROP_COLOR)
	{
		const ColorRGBA ColorPick = ColorRGBA::UnpackAlphaLast<ColorRGBA>(Value);

		Quad.m_aColors[m_PointIndex].r = ColorPick.r * 255.0f;
		Quad.m_aColors[m_PointIndex].g = ColorPick.g * 255.0f;
		Quad.m_aColors[m_PointIndex].b = ColorPick.b * 255.0f;
		Quad.m_aColors[m_PointIndex].a = ColorPick.a * 255.0f;

		m_pEditor->m_ColorPickerPopupContext.m_RgbaColor = ColorPick;
		m_pEditor->m_ColorPickerPopupContext.m_HslaColor = color_cast<ColorHSLA>(ColorPick);
		m_pEditor->m_ColorPickerPopupContext.m_HsvaColor = color_cast<ColorHSVA>(m_pEditor->m_ColorPickerPopupContext.m_HslaColor);
	}
	else if(m_Prop == EQuadPointProp::PROP_TEX_U)
	{
		Quad.m_aTexcoords[m_PointIndex].x = Value;
	}
	else if(m_Prop == EQuadPointProp::PROP_TEX_V)
	{
		Quad.m_aTexcoords[m_PointIndex].y = Value;
	}
}

// ---------------------------------------------------------------------------------------

CEditorActionBulk::CEditorActionBulk(CEditor *pEditor, const std::vector<std::shared_ptr<IEditorAction>> &vpActions, const char *pDisplay, bool Reverse) :
	IEditorAction(pEditor), m_vpActions(vpActions), m_Reverse(Reverse)
{
	// Assuming we only use bulk for actions of same type, if no display was provided
	if(!pDisplay)
	{
		const char *pBaseDisplay = m_vpActions[0]->DisplayText();
		if(m_vpActions.size() == 1)
			str_copy(m_aDisplayText, pBaseDisplay);
		else
			str_format(m_aDisplayText, sizeof(m_aDisplayText), "%s (x%d)", pBaseDisplay, (int)m_vpActions.size());
	}
	else
	{
		str_copy(m_aDisplayText, pDisplay);
	}
}

void CEditorActionBulk::Undo()
{
	if(m_Reverse)
	{
		for(auto pIt = m_vpActions.rbegin(); pIt != m_vpActions.rend(); pIt++)
		{
			auto &pAction = *pIt;
			pAction->Undo();
		}
	}
	else
	{
		for(auto &pAction : m_vpActions)
		{
			pAction->Undo();
		}
	}
}

void CEditorActionBulk::Redo()
{
	for(auto &pAction : m_vpActions)
	{
		pAction->Redo();
	}
}

// ---------

CEditorActionTileChanges::CEditorActionTileChanges(CEditor *pEditor, int GroupIndex, int LayerIndex, const char *pAction, const EditorTileStateChangeHistory<STileStateChange> &Changes) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_Changes(Changes)
{
	ComputeInfos();
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "%s (x%d)", pAction, m_TotalChanges);
}

void CEditorActionTileChanges::Undo()
{
	Apply(true);
}

void CEditorActionTileChanges::Redo()
{
	Apply(false);
}

void CEditorActionTileChanges::Apply(bool Undo)
{
	auto &Map = m_pEditor->m_Map;
	std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(m_pLayer);
	for(auto &Change : m_Changes)
	{
		int y = Change.first;
		auto Line = Change.second;
		for(auto &Tile : Line)
		{
			int x = Tile.first;
			STileStateChange State = Tile.second;
			pLayerTiles->SetTileIgnoreHistory(x, y, Undo ? State.m_Previous : State.m_Current);
		}
	}

	Map.OnModify();
}

void CEditorActionTileChanges::ComputeInfos()
{
	m_TotalChanges = 0;
	for(auto &Line : m_Changes)
		m_TotalChanges += Line.second.size();
}

// ---------

CEditorActionLayerBase::CEditorActionLayerBase(CEditor *pEditor, int GroupIndex, int LayerIndex) :
	IEditorAction(pEditor), m_GroupIndex(GroupIndex), m_LayerIndex(LayerIndex)
{
	m_pLayer = pEditor->m_Map.m_vpGroups[GroupIndex]->m_vpLayers[LayerIndex];
}

// ----------

CEditorActionAddLayer::CEditorActionAddLayer(CEditor *pEditor, int GroupIndex, int LayerIndex, bool Duplicate) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_Duplicate(Duplicate)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "%s %s layer in group %d", m_Duplicate ? "Duplicate" : "New", m_pLayer->TypeName(), m_GroupIndex);
}

void CEditorActionAddLayer::Undo()
{
	// Undo: remove layer from vector but keep it in case we want to add it back
	auto &vLayers = m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers;

	if(m_pLayer->m_Type == LAYERTYPE_TILES)
	{
		std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(m_pLayer);
		if(pLayerTiles->m_Front)
			m_pEditor->m_Map.m_pFrontLayer = nullptr;
		else if(pLayerTiles->m_Tele)
			m_pEditor->m_Map.m_pTeleLayer = nullptr;
		else if(pLayerTiles->m_Speedup)
			m_pEditor->m_Map.m_pSpeedupLayer = nullptr;
		else if(pLayerTiles->m_Switch)
			m_pEditor->m_Map.m_pSwitchLayer = nullptr;
		else if(pLayerTiles->m_Tune)
			m_pEditor->m_Map.m_pTuneLayer = nullptr;
	}

	vLayers.erase(vLayers.begin() + m_LayerIndex);

	m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_Collapse = false;
	if(m_LayerIndex >= (int)vLayers.size())
		m_pEditor->SelectLayer(vLayers.size() - 1, m_GroupIndex);

	m_pEditor->m_Map.OnModify();
}

void CEditorActionAddLayer::Redo()
{
	// Redo: add back the removed layer contained in this class
	auto &vLayers = m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers;

	if(m_pLayer->m_Type == LAYERTYPE_TILES)
	{
		std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(m_pLayer);
		if(pLayerTiles->m_Front)
			m_pEditor->m_Map.m_pFrontLayer = std::static_pointer_cast<CLayerFront>(m_pLayer);
		else if(pLayerTiles->m_Tele)
			m_pEditor->m_Map.m_pTeleLayer = std::static_pointer_cast<CLayerTele>(m_pLayer);
		else if(pLayerTiles->m_Speedup)
			m_pEditor->m_Map.m_pSpeedupLayer = std::static_pointer_cast<CLayerSpeedup>(m_pLayer);
		else if(pLayerTiles->m_Switch)
			m_pEditor->m_Map.m_pSwitchLayer = std::static_pointer_cast<CLayerSwitch>(m_pLayer);
		else if(pLayerTiles->m_Tune)
			m_pEditor->m_Map.m_pTuneLayer = std::static_pointer_cast<CLayerTune>(m_pLayer);
	}

	vLayers.insert(vLayers.begin() + m_LayerIndex, m_pLayer);

	m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_Collapse = false;
	m_pEditor->SelectLayer(m_LayerIndex, m_GroupIndex);
	m_pEditor->m_Map.OnModify();
}

CEditorActionDeleteLayer::CEditorActionDeleteLayer(CEditor *pEditor, int GroupIndex, int LayerIndex) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Delete %s layer of group %d", m_pLayer->TypeName(), m_GroupIndex);
}

void CEditorActionDeleteLayer::Redo()
{
	// Redo: remove layer from vector but keep it in case we want to add it back
	auto &vLayers = m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers;

	if(m_pLayer->m_Type == LAYERTYPE_TILES)
	{
		std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(m_pLayer);
		if(pLayerTiles->m_Front)
			m_pEditor->m_Map.m_pFrontLayer = nullptr;
		else if(pLayerTiles->m_Tele)
			m_pEditor->m_Map.m_pTeleLayer = nullptr;
		else if(pLayerTiles->m_Speedup)
			m_pEditor->m_Map.m_pSpeedupLayer = nullptr;
		else if(pLayerTiles->m_Switch)
			m_pEditor->m_Map.m_pSwitchLayer = nullptr;
		else if(pLayerTiles->m_Tune)
			m_pEditor->m_Map.m_pTuneLayer = nullptr;
	}

	m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->DeleteLayer(m_LayerIndex);

	m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_Collapse = false;
	if(m_LayerIndex >= (int)vLayers.size())
		m_pEditor->SelectLayer(vLayers.size() - 1, m_GroupIndex);

	m_pEditor->m_Map.OnModify();
}

void CEditorActionDeleteLayer::Undo()
{
	// Undo: add back the removed layer contained in this class
	auto &vLayers = m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers;

	if(m_pLayer->m_Type == LAYERTYPE_TILES)
	{
		std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(m_pLayer);
		if(pLayerTiles->m_Front)
			m_pEditor->m_Map.m_pFrontLayer = std::static_pointer_cast<CLayerFront>(m_pLayer);
		else if(pLayerTiles->m_Tele)
			m_pEditor->m_Map.m_pTeleLayer = std::static_pointer_cast<CLayerTele>(m_pLayer);
		else if(pLayerTiles->m_Speedup)
			m_pEditor->m_Map.m_pSpeedupLayer = std::static_pointer_cast<CLayerSpeedup>(m_pLayer);
		else if(pLayerTiles->m_Switch)
			m_pEditor->m_Map.m_pSwitchLayer = std::static_pointer_cast<CLayerSwitch>(m_pLayer);
		else if(pLayerTiles->m_Tune)
			m_pEditor->m_Map.m_pTuneLayer = std::static_pointer_cast<CLayerTune>(m_pLayer);
	}

	vLayers.insert(vLayers.begin() + m_LayerIndex, m_pLayer);

	m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_Collapse = false;
	m_pEditor->SelectLayer(m_LayerIndex, m_GroupIndex);
	m_pEditor->m_Map.OnModify();
}

CEditorActionGroup::CEditorActionGroup(CEditor *pEditor, int GroupIndex, bool Delete) :
	IEditorAction(pEditor), m_GroupIndex(GroupIndex), m_Delete(Delete)
{
	m_pGroup = pEditor->m_Map.m_vpGroups[GroupIndex];
	if(m_Delete)
		str_format(m_aDisplayText, sizeof(m_aDisplayText), "Delete group %d", m_GroupIndex);
	else
		str_copy(m_aDisplayText, "New group", sizeof(m_aDisplayText));
}

void CEditorActionGroup::Undo()
{
	if(m_Delete)
	{
		// Undo: add back the group
		m_pEditor->m_Map.m_vpGroups.insert(m_pEditor->m_Map.m_vpGroups.begin() + m_GroupIndex, m_pGroup);
		m_pEditor->m_SelectedGroup = m_GroupIndex;
		m_pEditor->m_Map.OnModify();
	}
	else
	{
		// Undo: delete the group
		m_pEditor->m_Map.DeleteGroup(m_GroupIndex);
		m_pEditor->m_SelectedGroup = maximum(0, m_GroupIndex - 1);
	}

	m_pEditor->m_Map.OnModify();
}

void CEditorActionGroup::Redo()
{
	if(!m_Delete)
	{
		// Redo: add back the group
		m_pEditor->m_Map.m_vpGroups.insert(m_pEditor->m_Map.m_vpGroups.begin() + m_GroupIndex, m_pGroup);
		m_pEditor->m_SelectedGroup = m_GroupIndex;
	}
	else
	{
		// Redo: delete the group
		m_pEditor->m_Map.DeleteGroup(m_GroupIndex);
		m_pEditor->m_SelectedGroup = maximum(0, m_GroupIndex - 1);
	}

	m_pEditor->m_Map.OnModify();
}

CEditorActionEditGroupProp::CEditorActionEditGroupProp(CEditor *pEditor, int GroupIndex, EGroupProp Prop, int Previous, int Current) :
	IEditorAction(pEditor), m_GroupIndex(GroupIndex), m_Prop(Prop), m_Previous(Previous), m_Current(Current)
{
	static const char *s_apNames[] = {
		"order",
		"pos X",
		"pos Y",
		"para X",
		"para Y",
		"use clipping",
		"clip X",
		"clip Y",
		"clip W",
		"clip H"};

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit group %d %s property", m_GroupIndex, s_apNames[(int)Prop]);
}

void CEditorActionEditGroupProp::Undo()
{
	auto pGroup = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	if(m_Prop == EGroupProp::PROP_ORDER)
	{
		int CurrentOrder = m_Current;
		bool Dir = m_Current > m_Previous;
		while(CurrentOrder != m_Previous)
		{
			CurrentOrder = m_pEditor->m_Map.SwapGroups(CurrentOrder, Dir ? CurrentOrder - 1 : CurrentOrder + 1);
		}
		m_pEditor->m_SelectedGroup = m_Previous;
	}
	else
		Apply(m_Previous);
}

void CEditorActionEditGroupProp::Redo()
{
	auto pGroup = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	if(m_Prop == EGroupProp::PROP_ORDER)
	{
		int CurrentOrder = m_Previous;
		bool Dir = m_Previous > m_Current;
		while(CurrentOrder != m_Current)
		{
			CurrentOrder = m_pEditor->m_Map.SwapGroups(CurrentOrder, Dir ? CurrentOrder - 1 : CurrentOrder + 1);
		}
		m_pEditor->m_SelectedGroup = m_Current;
	}
	else
		Apply(m_Current);
}

void CEditorActionEditGroupProp::Apply(int Value)
{
	auto pGroup = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	if(m_Prop == EGroupProp::PROP_POS_X)
		pGroup->m_OffsetX = Value;
	if(m_Prop == EGroupProp::PROP_POS_Y)
		pGroup->m_OffsetY = Value;
	if(m_Prop == EGroupProp::PROP_PARA_X)
		pGroup->m_ParallaxX = Value;
	if(m_Prop == EGroupProp::PROP_PARA_Y)
		pGroup->m_ParallaxY = Value;
	if(m_Prop == EGroupProp::PROP_USE_CLIPPING)
		pGroup->m_UseClipping = Value;
	if(m_Prop == EGroupProp::PROP_CLIP_X)
		pGroup->m_ClipX = Value;
	if(m_Prop == EGroupProp::PROP_CLIP_Y)
		pGroup->m_ClipY = Value;
	if(m_Prop == EGroupProp::PROP_CLIP_W)
		pGroup->m_ClipW = Value;
	if(m_Prop == EGroupProp::PROP_CLIP_H)
		pGroup->m_ClipH = Value;

	m_pEditor->m_Map.OnModify();
}

template<typename E>
CEditorActionEditLayerPropBase<E>::CEditorActionEditLayerPropBase(CEditor *pEditor, int GroupIndex, int LayerIndex, E Prop, int Previous, int Current) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_Prop(Prop), m_Previous(Previous), m_Current(Current)
{
}

CEditorActionEditLayerProp::CEditorActionEditLayerProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ELayerProp Prop, int Previous, int Current) :
	CEditorActionEditLayerPropBase(pEditor, GroupIndex, LayerIndex, Prop, Previous, Current)
{
	static const char *s_apNames[] = {
		"group",
		"order",
		"HQ"};

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit layer %d in group %d %s property", m_LayerIndex, m_GroupIndex, s_apNames[(int)m_Prop]);
}

void CEditorActionEditLayerProp::Undo()
{
	auto pCurrentGroup = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	if(m_Prop == ELayerProp::PROP_ORDER)
	{
		m_pEditor->SelectLayer(pCurrentGroup->SwapLayers(m_Current, m_Previous));
	}
	else
		Apply(m_Previous);
}

void CEditorActionEditLayerProp::Redo()
{
	auto pCurrentGroup = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	if(m_Prop == ELayerProp::PROP_ORDER)
	{
		m_pEditor->SelectLayer(pCurrentGroup->SwapLayers(m_Previous, m_Current));
	}
	else
		Apply(m_Current);
}

void CEditorActionEditLayerProp::Apply(int Value)
{
	if(m_Prop == ELayerProp::PROP_GROUP)
	{
		auto pCurrentGroup = m_pEditor->m_Map.m_vpGroups[Value == m_Previous ? m_Current : m_Previous];
		auto pPreviousGroup = m_pEditor->m_Map.m_vpGroups[Value];
		pCurrentGroup->m_vpLayers.erase(pCurrentGroup->m_vpLayers.begin() + pCurrentGroup->m_vpLayers.size() - 1);
		if(Value == m_Previous)
			pPreviousGroup->m_vpLayers.insert(pPreviousGroup->m_vpLayers.begin() + m_LayerIndex, m_pLayer);
		else
			pPreviousGroup->m_vpLayers.push_back(m_pLayer);
		m_pEditor->m_SelectedGroup = Value;
		m_pEditor->SelectLayer(m_LayerIndex);
	}
	else if(m_Prop == ELayerProp::PROP_HQ)
	{
		m_pLayer->m_Flags = Value;
	}

	m_pEditor->m_Map.OnModify();
}

CEditorActionEditLayerTilesProp::CEditorActionEditLayerTilesProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ETilesProp Prop, int Previous, int Current) :
	CEditorActionEditLayerPropBase(pEditor, GroupIndex, LayerIndex, Prop, Previous, Current)
{
	static const char *s_apNames[] = {
		"width",
		"height",
		"shift",
		"shift by",
		"image",
		"color",
		"color env",
		"color env offset",
		"automapper",
		"seed"};

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit tiles layer %d in group %d %s property", m_LayerIndex, m_GroupIndex, s_apNames[(int)Prop]);
}

void CEditorActionEditLayerTilesProp::SetSavedLayers(const std::map<int, std::shared_ptr<CLayer>> &SavedLayers)
{
	m_SavedLayers = std::map(SavedLayers);
}

void CEditorActionEditLayerTilesProp::Undo()
{
	std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(m_pLayer);
	std::shared_ptr<CLayerTiles> pSavedLayerTiles = nullptr;

	if(m_Prop == ETilesProp::PROP_WIDTH || m_Prop == ETilesProp::PROP_HEIGHT)
	{
		if(m_Prop == ETilesProp::PROP_HEIGHT)
			pLayerTiles->Resize(pLayerTiles->m_Width, m_Previous);
		else if(m_Prop == ETilesProp::PROP_WIDTH)
			pLayerTiles->Resize(m_Previous, pLayerTiles->m_Height);

		RestoreLayer(LAYERTYPE_TILES, pLayerTiles);
		if(pLayerTiles->m_Game || pLayerTiles->m_Front || pLayerTiles->m_Switch || pLayerTiles->m_Speedup || pLayerTiles->m_Tune)
		{
			if(m_pEditor->m_Map.m_pFrontLayer && !pLayerTiles->m_Front)
				RestoreLayer(LAYERTYPE_FRONT, m_pEditor->m_Map.m_pFrontLayer);
			if(m_pEditor->m_Map.m_pTeleLayer && !pLayerTiles->m_Tele)
				RestoreLayer(LAYERTYPE_TELE, m_pEditor->m_Map.m_pTeleLayer);
			if(m_pEditor->m_Map.m_pSwitchLayer && !pLayerTiles->m_Switch)
				RestoreLayer(LAYERTYPE_SWITCH, m_pEditor->m_Map.m_pSwitchLayer);
			if(m_pEditor->m_Map.m_pSpeedupLayer && !pLayerTiles->m_Speedup)
				RestoreLayer(LAYERTYPE_SPEEDUP, m_pEditor->m_Map.m_pSpeedupLayer);
			if(m_pEditor->m_Map.m_pTuneLayer && !pLayerTiles->m_Tune)
				RestoreLayer(LAYERTYPE_TUNE, m_pEditor->m_Map.m_pTuneLayer);
			if(!pLayerTiles->m_Game)
				RestoreLayer(LAYERTYPE_GAME, m_pEditor->m_Map.m_pGameLayer);
		}
	}
	else if(m_Prop == ETilesProp::PROP_SHIFT)
	{
		RestoreLayer(LAYERTYPE_TILES, pLayerTiles);
	}
	else if(m_Prop == ETilesProp::PROP_SHIFT_BY)
	{
		m_pEditor->m_ShiftBy = m_Previous;
	}
	else if(m_Prop == ETilesProp::PROP_IMAGE)
	{
		if(m_Previous == -1)
		{
			pLayerTiles->m_Image = -1;
		}
		else
		{
			pLayerTiles->m_Image = m_Previous % m_pEditor->m_Map.m_vpImages.size();
			pLayerTiles->m_AutoMapperConfig = -1;
		}
	}
	else if(m_Prop == ETilesProp::PROP_COLOR)
	{
		const ColorRGBA ColorPick = ColorRGBA::UnpackAlphaLast<ColorRGBA>(m_Previous);

		pLayerTiles->m_Color.r = ColorPick.r * 255.0f;
		pLayerTiles->m_Color.g = ColorPick.g * 255.0f;
		pLayerTiles->m_Color.b = ColorPick.b * 255.0f;
		pLayerTiles->m_Color.a = ColorPick.a * 255.0f;

		m_pEditor->m_ColorPickerPopupContext.m_RgbaColor = ColorPick;
		m_pEditor->m_ColorPickerPopupContext.m_HslaColor = color_cast<ColorHSLA>(ColorPick);
		m_pEditor->m_ColorPickerPopupContext.m_HsvaColor = color_cast<ColorHSVA>(m_pEditor->m_ColorPickerPopupContext.m_HslaColor);
	}
	else if(m_Prop == ETilesProp::PROP_COLOR_ENV)
	{
		pLayerTiles->m_ColorEnv = m_Previous;
	}
	else if(m_Prop == ETilesProp::PROP_COLOR_ENV_OFFSET)
	{
		pLayerTiles->m_ColorEnvOffset = m_Previous;
	}
	else if(m_Prop == ETilesProp::PROP_AUTOMAPPER)
	{
		pLayerTiles->m_AutoMapperConfig = m_Previous;
	}
	else if(m_Prop == ETilesProp::PROP_SEED)
	{
		pLayerTiles->m_Seed = m_Previous;
	}

	m_pEditor->m_Map.OnModify();
}

void CEditorActionEditLayerTilesProp::Redo()
{
	std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(m_pLayer);

	if(m_Prop == ETilesProp::PROP_WIDTH || m_Prop == ETilesProp::PROP_HEIGHT)
	{
		if(m_Prop == ETilesProp::PROP_HEIGHT)
			pLayerTiles->Resize(pLayerTiles->m_Width, m_Current);
		else if(m_Prop == ETilesProp::PROP_WIDTH)
			pLayerTiles->Resize(m_Current, pLayerTiles->m_Height);

		if(pLayerTiles->m_Game || pLayerTiles->m_Front || pLayerTiles->m_Switch || pLayerTiles->m_Speedup || pLayerTiles->m_Tune)
		{
			if(m_pEditor->m_Map.m_pFrontLayer && !pLayerTiles->m_Front)
				m_pEditor->m_Map.m_pFrontLayer->Resize(pLayerTiles->m_Width, pLayerTiles->m_Height);
			if(m_pEditor->m_Map.m_pTeleLayer && !pLayerTiles->m_Tele)
				m_pEditor->m_Map.m_pTeleLayer->Resize(pLayerTiles->m_Width, pLayerTiles->m_Height);
			if(m_pEditor->m_Map.m_pSwitchLayer && !pLayerTiles->m_Switch)
				m_pEditor->m_Map.m_pSwitchLayer->Resize(pLayerTiles->m_Width, pLayerTiles->m_Height);
			if(m_pEditor->m_Map.m_pSpeedupLayer && !pLayerTiles->m_Speedup)
				m_pEditor->m_Map.m_pSpeedupLayer->Resize(pLayerTiles->m_Width, pLayerTiles->m_Height);
			if(m_pEditor->m_Map.m_pTuneLayer && !pLayerTiles->m_Tune)
				m_pEditor->m_Map.m_pTuneLayer->Resize(pLayerTiles->m_Width, pLayerTiles->m_Height);
			if(!pLayerTiles->m_Game)
				m_pEditor->m_Map.m_pGameLayer->Resize(pLayerTiles->m_Width, pLayerTiles->m_Height);
		}
	}
	else if(m_Prop == ETilesProp::PROP_SHIFT)
	{
		pLayerTiles->Shift(m_Current);
	}
	else if(m_Prop == ETilesProp::PROP_SHIFT_BY)
	{
		m_pEditor->m_ShiftBy = m_Current;
	}
	else if(m_Prop == ETilesProp::PROP_IMAGE)
	{
		if(m_Current == -1)
		{
			pLayerTiles->m_Image = -1;
		}
		else
		{
			pLayerTiles->m_Image = m_Current % m_pEditor->m_Map.m_vpImages.size();
			pLayerTiles->m_AutoMapperConfig = -1;
		}
	}
	else if(m_Prop == ETilesProp::PROP_COLOR)
	{
		const ColorRGBA ColorPick = ColorRGBA::UnpackAlphaLast<ColorRGBA>(m_Current);

		pLayerTiles->m_Color.r = ColorPick.r * 255.0f;
		pLayerTiles->m_Color.g = ColorPick.g * 255.0f;
		pLayerTiles->m_Color.b = ColorPick.b * 255.0f;
		pLayerTiles->m_Color.a = ColorPick.a * 255.0f;

		m_pEditor->m_ColorPickerPopupContext.m_RgbaColor = ColorPick;
		m_pEditor->m_ColorPickerPopupContext.m_HslaColor = color_cast<ColorHSLA>(ColorPick);
		m_pEditor->m_ColorPickerPopupContext.m_HsvaColor = color_cast<ColorHSVA>(m_pEditor->m_ColorPickerPopupContext.m_HslaColor);
	}
	else if(m_Prop == ETilesProp::PROP_COLOR_ENV)
	{
		pLayerTiles->m_ColorEnv = m_Current;
	}
	else if(m_Prop == ETilesProp::PROP_COLOR_ENV_OFFSET)
	{
		pLayerTiles->m_ColorEnvOffset = m_Current;
	}
	else if(m_Prop == ETilesProp::PROP_AUTOMAPPER)
	{
		pLayerTiles->m_AutoMapperConfig = m_Current;
	}
	else if(m_Prop == ETilesProp::PROP_SEED)
	{
		pLayerTiles->m_Seed = m_Current;
	}

	m_pEditor->m_Map.OnModify();
}

void CEditorActionEditLayerTilesProp::RestoreLayer(int Layer, const std::shared_ptr<CLayerTiles> &pLayerTiles)
{
	if(m_SavedLayers[Layer] != nullptr)
	{
		std::shared_ptr<CLayerTiles> pSavedLayerTiles = std::static_pointer_cast<CLayerTiles>(m_SavedLayers[Layer]);
		mem_copy(pLayerTiles->m_pTiles, pSavedLayerTiles->m_pTiles, (size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTile));

		if(pLayerTiles->m_Tele)
		{
			std::shared_ptr<CLayerTele> pLayerTele = std::static_pointer_cast<CLayerTele>(pLayerTiles);
			std::shared_ptr<CLayerTele> pSavedLayerTele = std::static_pointer_cast<CLayerTele>(pSavedLayerTiles);
			mem_copy(pLayerTele->m_pTeleTile, pSavedLayerTele->m_pTeleTile, (size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTeleTile));
		}
		else if(pLayerTiles->m_Speedup)
		{
			std::shared_ptr<CLayerSpeedup> pLayerSpeedup = std::static_pointer_cast<CLayerSpeedup>(pLayerTiles);
			std::shared_ptr<CLayerSpeedup> pSavedLayerSpeedup = std::static_pointer_cast<CLayerSpeedup>(pSavedLayerTiles);
			mem_copy(pLayerSpeedup->m_pSpeedupTile, pSavedLayerSpeedup->m_pSpeedupTile, (size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CSpeedupTile));
		}
		else if(pLayerTiles->m_Switch)
		{
			std::shared_ptr<CLayerSwitch> pLayerSwitch = std::static_pointer_cast<CLayerSwitch>(pLayerTiles);
			std::shared_ptr<CLayerSwitch> pSavedLayerSwitch = std::static_pointer_cast<CLayerSwitch>(pSavedLayerTiles);
			mem_copy(pLayerSwitch->m_pSwitchTile, pSavedLayerSwitch->m_pSwitchTile, (size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CSwitchTile));
		}
		else if(pLayerTiles->m_Tune)
		{
			std::shared_ptr<CLayerTune> pLayerTune = std::static_pointer_cast<CLayerTune>(pLayerTiles);
			std::shared_ptr<CLayerTune> pSavedLayerTune = std::static_pointer_cast<CLayerTune>(pSavedLayerTiles);
			mem_copy(pLayerTune->m_pTuneTile, pSavedLayerTune->m_pTuneTile, (size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTuneTile));
		}
	}
}

CEditorActionEditLayerQuadsProp::CEditorActionEditLayerQuadsProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ELayerQuadsProp Prop, int Previous, int Current) :
	CEditorActionEditLayerPropBase(pEditor, GroupIndex, LayerIndex, Prop, Previous, Current)
{
	static const char *s_apNames[] = {
		"image"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit quads layer %d in group %d %s property", m_LayerIndex, m_GroupIndex, s_apNames[(int)m_Prop]);
}

void CEditorActionEditLayerQuadsProp::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEditLayerQuadsProp::Redo()
{
	Apply(m_Current);
}

void CEditorActionEditLayerQuadsProp::Apply(int Value)
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	if(m_Prop == ELayerQuadsProp::PROP_IMAGE)
	{
		if(Value >= 0)
			pLayerQuads->m_Image = Value % m_pEditor->m_Map.m_vpImages.size();
		else
			pLayerQuads->m_Image = -1;
	}

	m_pEditor->m_Map.OnModify();
}

// --------------------------------------------------------------

CEditorActionEditLayersGroupAndOrder::CEditorActionEditLayersGroupAndOrder(CEditor *pEditor, int GroupIndex, const std::vector<int> &LayerIndices, int NewGroupIndex, const std::vector<int> &NewLayerIndices) :
	IEditorAction(pEditor), m_GroupIndex(GroupIndex), m_LayerIndices(LayerIndices), m_NewGroupIndex(NewGroupIndex), m_NewLayerIndices(NewLayerIndices)
{
	std::sort(m_LayerIndices.begin(), m_LayerIndices.end());
	std::sort(m_NewLayerIndices.begin(), m_NewLayerIndices.end());

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit layers group and order (x%d)", (int)m_LayerIndices.size());
}

void CEditorActionEditLayersGroupAndOrder::Undo()
{
	// Undo : restore group and order
	auto &Map = m_pEditor->m_Map;
	auto &pCurrentGroup = Map.m_vpGroups[m_NewGroupIndex];
	auto &pPreviousGroup = Map.m_vpGroups[m_GroupIndex];
	std::vector<std::shared_ptr<CLayer>> vpLayers;
	vpLayers.reserve(m_NewLayerIndices.size());
	for(auto &LayerIndex : m_NewLayerIndices)
		vpLayers.push_back(pCurrentGroup->m_vpLayers[LayerIndex]);

	int k = 0;
	for(auto &pLayer : vpLayers)
	{
		pCurrentGroup->m_vpLayers.erase(std::find(pCurrentGroup->m_vpLayers.begin(), pCurrentGroup->m_vpLayers.end(), pLayer));
		pPreviousGroup->m_vpLayers.insert(pPreviousGroup->m_vpLayers.begin() + m_LayerIndices[k++], pLayer);
	}

	m_pEditor->m_vSelectedLayers = m_LayerIndices;
	m_pEditor->m_SelectedGroup = m_GroupIndex;
}

void CEditorActionEditLayersGroupAndOrder::Redo()
{
	// Redo : move layers
	auto &Map = m_pEditor->m_Map;
	auto &pCurrentGroup = Map.m_vpGroups[m_GroupIndex];
	auto &pPreviousGroup = Map.m_vpGroups[m_NewGroupIndex];
	std::vector<std::shared_ptr<CLayer>> vpLayers;
	vpLayers.reserve(m_LayerIndices.size());
	for(auto &LayerIndex : m_LayerIndices)
		vpLayers.push_back(pCurrentGroup->m_vpLayers[LayerIndex]);

	int k = 0;
	for(auto &pLayer : vpLayers)
	{
		pCurrentGroup->m_vpLayers.erase(std::find(pCurrentGroup->m_vpLayers.begin(), pCurrentGroup->m_vpLayers.end(), pLayer));
		pPreviousGroup->m_vpLayers.insert(pPreviousGroup->m_vpLayers.begin() + m_NewLayerIndices[k++], pLayer);
	}

	m_pEditor->m_vSelectedLayers = m_NewLayerIndices;
	m_pEditor->m_SelectedGroup = m_NewGroupIndex;
}

// -----------------------------------

CEditorActionAppendMap::CEditorActionAppendMap(CEditor *pEditor, const char *pMapName, const SPrevInfo &PrevInfo, std::vector<int> &vImageIndexMap) :
	IEditorAction(pEditor), m_PrevInfo(PrevInfo), m_vImageIndexMap(vImageIndexMap)
{
	str_copy(m_aMapName, pMapName);
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Append %s", m_aMapName);
}

void CEditorActionAppendMap::Undo()
{
	auto &Map = m_pEditor->m_Map;
	// Undo append:
	// - delete added groups
	// - delete added envelopes
	// - delete added images
	// - delete added sounds

	// Delete added groups
	while((int)Map.m_vpGroups.size() != m_PrevInfo.m_Groups)
	{
		Map.m_vpGroups.pop_back();
	}

	// Delete added envelopes
	while((int)Map.m_vpEnvelopes.size() != m_PrevInfo.m_Envelopes)
	{
		Map.m_vpEnvelopes.pop_back();
	}

	// Delete added sounds
	while((int)Map.m_vpSounds.size() != m_PrevInfo.m_Sounds)
	{
		Map.m_vpSounds.pop_back();
	}

	// Delete added images
	// Images are sorted when appending, so we need to revert sorting before deleting the images
	if(!m_vImageIndexMap.empty())
	{
		std::vector<int> vReverseIndexMap;
		vReverseIndexMap.resize(m_vImageIndexMap.size());

		for(int k = 0; k < (int)m_vImageIndexMap.size(); k++)
			vReverseIndexMap[m_vImageIndexMap[k]] = k;

		std::vector<std::shared_ptr<CEditorImage>> vpRevertedImages;
		vpRevertedImages.resize(Map.m_vpImages.size());

		for(int k = 0; k < (int)vReverseIndexMap.size(); k++)
		{
			vpRevertedImages[vReverseIndexMap[k]] = Map.m_vpImages[k];
		}
		Map.m_vpImages = vpRevertedImages;

		Map.ModifyImageIndex([vReverseIndexMap](int *pIndex) {
			if(*pIndex >= 0)
			{
				*pIndex = vReverseIndexMap[*pIndex];
			}
		});
	}

	while((int)Map.m_vpImages.size() != m_PrevInfo.m_Images)
	{
		Map.m_vpImages.pop_back();
	}
}

void CEditorActionAppendMap::Redo()
{
	// Redo is just re-appending the same map
	m_pEditor->Append(m_aMapName, IStorage::TYPE_ALL, true);
}

// ---------------------------

CEditorActionTileArt::CEditorActionTileArt(CEditor *pEditor, int PreviousImageCount, const char *pTileArtFile, std::vector<int> &vImageIndexMap) :
	IEditorAction(pEditor), m_PreviousImageCount(PreviousImageCount), m_vImageIndexMap(vImageIndexMap)
{
	str_copy(m_aTileArtFile, pTileArtFile);
	str_copy(m_aDisplayText, "Tile art");
}

void CEditorActionTileArt::Undo()
{
	auto &Map = m_pEditor->m_Map;

	// Delete added group
	Map.m_vpGroups.pop_back();

	// Delete added images
	// Images are sorted when appending, so we need to revert sorting before deleting the images
	if(!m_vImageIndexMap.empty())
	{
		std::vector<int> vReverseIndexMap;
		vReverseIndexMap.resize(m_vImageIndexMap.size());

		for(int k = 0; k < (int)m_vImageIndexMap.size(); k++)
			vReverseIndexMap[m_vImageIndexMap[k]] = k;

		std::vector<std::shared_ptr<CEditorImage>> vpRevertedImages;
		vpRevertedImages.resize(Map.m_vpImages.size());

		for(int k = 0; k < (int)vReverseIndexMap.size(); k++)
		{
			vpRevertedImages[vReverseIndexMap[k]] = Map.m_vpImages[k];
		}
		Map.m_vpImages = vpRevertedImages;

		Map.ModifyImageIndex([vReverseIndexMap](int *pIndex) {
			if(*pIndex >= 0)
			{
				*pIndex = vReverseIndexMap[*pIndex];
			}
		});
	}

	while((int)Map.m_vpImages.size() != m_PreviousImageCount)
	{
		Map.m_vpImages.pop_back();
	}
}

void CEditorActionTileArt::Redo()
{
	if(!m_pEditor->Graphics()->LoadPng(m_pEditor->m_TileartImageInfo, m_aTileArtFile, IStorage::TYPE_ALL))
	{
		m_pEditor->ShowFileDialogError("Failed to load image from file '%s'.", m_aTileArtFile);
		return;
	}

	IStorage::StripPathAndExtension(m_aTileArtFile, m_pEditor->m_aTileartFilename, sizeof(m_pEditor->m_aTileartFilename));
	m_pEditor->AddTileart(true);
}

// ---------------------------------

CEditorCommandAction::CEditorCommandAction(CEditor *pEditor, EType Type, int *pSelectedCommandIndex, int CommandIndex, const char *pPreviousCommand, const char *pCurrentCommand) :
	IEditorAction(pEditor), m_Type(Type), m_pSelectedCommandIndex(pSelectedCommandIndex), m_CommandIndex(CommandIndex)
{
	if(pPreviousCommand != nullptr)
		m_PreviousCommand = std::string(pPreviousCommand);
	if(pCurrentCommand != nullptr)
		m_CurrentCommand = std::string(pCurrentCommand);

	switch(m_Type)
	{
	case EType::ADD:
		str_copy(m_aDisplayText, "Add command");
		break;
	case EType::EDIT:
		str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit command %d", m_CommandIndex);
		break;
	case EType::DELETE:
		str_format(m_aDisplayText, sizeof(m_aDisplayText), "Delete command %d", m_CommandIndex);
		break;
	case EType::MOVE_UP:
		str_format(m_aDisplayText, sizeof(m_aDisplayText), "Move command %d up", m_CommandIndex);
		break;
	case EType::MOVE_DOWN:
		str_format(m_aDisplayText, sizeof(m_aDisplayText), "Move command %d down", m_CommandIndex);
		break;
	default:
		str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit command %d", m_CommandIndex);
		break;
	}
}

void CEditorCommandAction::Undo()
{
	auto &Map = m_pEditor->m_Map;
	switch(m_Type)
	{
	case EType::DELETE:
	{
		Map.m_vSettings.insert(Map.m_vSettings.begin() + m_CommandIndex, m_PreviousCommand.c_str());
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	case EType::ADD:
	{
		Map.m_vSettings.erase(Map.m_vSettings.begin() + m_CommandIndex);
		*m_pSelectedCommandIndex = Map.m_vSettings.size() - 1;
		break;
	}
	case EType::EDIT:
	{
		str_copy(Map.m_vSettings[m_CommandIndex].m_aCommand, m_PreviousCommand.c_str());
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	case EType::MOVE_DOWN:
	{
		std::swap(Map.m_vSettings[m_CommandIndex], Map.m_vSettings[m_CommandIndex + 1]);
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	case EType::MOVE_UP:
	{
		std::swap(Map.m_vSettings[m_CommandIndex], Map.m_vSettings[m_CommandIndex - 1]);
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	}
}

void CEditorCommandAction::Redo()
{
	auto &Map = m_pEditor->m_Map;
	switch(m_Type)
	{
	case EType::DELETE:
	{
		Map.m_vSettings.erase(Map.m_vSettings.begin() + m_CommandIndex);
		*m_pSelectedCommandIndex = Map.m_vSettings.size() - 1;
		break;
	}
	case EType::ADD:
	{
		Map.m_vSettings.insert(Map.m_vSettings.begin() + m_CommandIndex, m_PreviousCommand.c_str());
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	case EType::EDIT:
	{
		str_copy(Map.m_vSettings[m_CommandIndex].m_aCommand, m_CurrentCommand.c_str());
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	case EType::MOVE_DOWN:
	{
		std::swap(Map.m_vSettings[m_CommandIndex], Map.m_vSettings[m_CommandIndex + 1]);
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	case EType::MOVE_UP:
	{
		std::swap(Map.m_vSettings[m_CommandIndex], Map.m_vSettings[m_CommandIndex - 1]);
		*m_pSelectedCommandIndex = m_CommandIndex;
		break;
	}
	}
}

// ------------------------------------------------

CEditorActionEnvelopeAdd::CEditorActionEnvelopeAdd(CEditor *pEditor, const std::shared_ptr<CEnvelope> &pEnv) :
	IEditorAction(pEditor), m_pEnv(pEnv)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Add new %s envelope", pEnv->Type() == CEnvelope::EType::COLOR ? "color" : (pEnv->Type() == CEnvelope::EType::POSITION ? "position" : "sound"));
}

void CEditorActionEnvelopeAdd::Undo()
{
	// Undo is removing the envelope, which was added at the back of the list
	m_pEditor->m_Map.m_vpEnvelopes.pop_back();
	m_pEditor->m_SelectedEnvelope = m_pEditor->m_Map.m_vpEnvelopes.size() - 1;
}

void CEditorActionEnvelopeAdd::Redo()
{
	// Redo is adding back at the back the saved envelope
	m_pEditor->m_Map.m_vpEnvelopes.push_back(m_pEnv);
	m_pEditor->m_SelectedEnvelope = m_pEditor->m_Map.m_vpEnvelopes.size() - 1;
}

CEditorActionEveloppeDelete::CEditorActionEveloppeDelete(CEditor *pEditor, int EnvelopeIndex) :
	IEditorAction(pEditor), m_EnvelopeIndex(EnvelopeIndex), m_pEnv(pEditor->m_Map.m_vpEnvelopes[EnvelopeIndex])
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Delete envelope %d", m_EnvelopeIndex);
}

void CEditorActionEveloppeDelete::Undo()
{
	// Undo is adding back the envelope
	m_pEditor->m_Map.m_vpEnvelopes.insert(m_pEditor->m_Map.m_vpEnvelopes.begin() + m_EnvelopeIndex, m_pEnv);
	m_pEditor->m_SelectedEnvelope = m_EnvelopeIndex;
}

void CEditorActionEveloppeDelete::Redo()
{
	// Redo is erasing the same envelope index
	m_pEditor->m_Map.m_vpEnvelopes.erase(m_pEditor->m_Map.m_vpEnvelopes.begin() + m_EnvelopeIndex);
	if(m_pEditor->m_SelectedEnvelope >= (int)m_pEditor->m_Map.m_vpEnvelopes.size())
		m_pEditor->m_SelectedEnvelope = m_pEditor->m_Map.m_vpEnvelopes.size() - 1;
}

CEditorActionEnvelopeEdit::CEditorActionEnvelopeEdit(CEditor *pEditor, int EnvelopeIndex, EEditType EditType, int Previous, int Current) :
	IEditorAction(pEditor), m_EnvelopeIndex(EnvelopeIndex), m_EditType(EditType), m_Previous(Previous), m_Current(Current), m_pEnv(pEditor->m_Map.m_vpEnvelopes[EnvelopeIndex])
{
	static const char *s_apNames[] = {
		"sync",
		"order"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit envelope %d %s", m_EnvelopeIndex, s_apNames[(int)m_EditType]);
}

void CEditorActionEnvelopeEdit::Undo()
{
	switch(m_EditType)
	{
	case EEditType::ORDER:
	{
		m_pEditor->m_Map.SwapEnvelopes(m_Current, m_Previous);
		break;
	}
	case EEditType::SYNC:
	{
		m_pEnv->m_Synchronized = m_Previous;
		break;
	}
	}
	m_pEditor->m_Map.OnModify();
	m_pEditor->m_SelectedEnvelope = m_EnvelopeIndex;
}

void CEditorActionEnvelopeEdit::Redo()
{
	switch(m_EditType)
	{
	case EEditType::ORDER:
	{
		m_pEditor->m_Map.SwapEnvelopes(m_Previous, m_Current);
		break;
	}
	case EEditType::SYNC:
	{
		m_pEnv->m_Synchronized = m_Current;
		break;
	}
	}
	m_pEditor->m_Map.OnModify();
	m_pEditor->m_SelectedEnvelope = m_EnvelopeIndex;
}

CEditorActionEnvelopeEditPoint::CEditorActionEnvelopeEditPoint(CEditor *pEditor, int EnvelopeIndex, int PointIndex, int Channel, EEditType EditType, int Previous, int Current) :
	IEditorAction(pEditor), m_EnvelopeIndex(EnvelopeIndex), m_PointIndex(PointIndex), m_Channel(Channel), m_EditType(EditType), m_Previous(Previous), m_Current(Current), m_pEnv(pEditor->m_Map.m_vpEnvelopes[EnvelopeIndex])
{
	static const char *s_apNames[] = {
		"time",
		"value",
		"curve type"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit %s of point %d (channel %d) of env %d", s_apNames[(int)m_EditType], m_PointIndex, m_Channel, m_EnvelopeIndex);
}

void CEditorActionEnvelopeEditPoint::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEnvelopeEditPoint::Redo()
{
	Apply(m_Current);
}

void CEditorActionEnvelopeEditPoint::Apply(int Value)
{
	if(m_EditType == EEditType::TIME)
	{
		m_pEnv->m_vPoints[m_PointIndex].m_Time = Value;
	}
	else if(m_EditType == EEditType::VALUE)
	{
		m_pEnv->m_vPoints[m_PointIndex].m_aValues[m_Channel] = Value;

		if(m_pEnv->GetChannels() == 4)
		{
			auto *pValues = m_pEnv->m_vPoints[m_PointIndex].m_aValues;
			const ColorRGBA Color = ColorRGBA(fx2f(pValues[0]), fx2f(pValues[1]), fx2f(pValues[2]), fx2f(pValues[3]));

			m_pEditor->m_ColorPickerPopupContext.m_RgbaColor = Color;
			m_pEditor->m_ColorPickerPopupContext.m_HslaColor = color_cast<ColorHSLA>(Color);
			m_pEditor->m_ColorPickerPopupContext.m_HsvaColor = color_cast<ColorHSVA>(m_pEditor->m_ColorPickerPopupContext.m_HslaColor);
		}
	}
	else if(m_EditType == EEditType::CURVE_TYPE)
	{
		m_pEnv->m_vPoints[m_PointIndex].m_Curvetype = Value;
	}

	m_pEditor->m_Map.OnModify();
}

// ----

CEditorActionEditEnvelopePointValue::CEditorActionEditEnvelopePointValue(CEditor *pEditor, int EnvIndex, int PointIndex, int Channel, EType Type, int OldTime, int OldValue, int NewTime, int NewValue) :
	IEditorAction(pEditor), m_EnvIndex(EnvIndex), m_PtIndex(PointIndex), m_Channel(Channel), m_Type(Type), m_OldTime(OldTime), m_OldValue(OldValue), m_NewTime(NewTime), m_NewValue(NewValue)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit point %d%s value (envelope %d, channel %d)", PointIndex, m_Type == EType::TANGENT_IN ? "tangent in" : (m_Type == EType::TANGENT_OUT ? "tangent out" : ""), m_EnvIndex, m_Channel);
}

void CEditorActionEditEnvelopePointValue::Undo()
{
	Apply(true);
}

void CEditorActionEditEnvelopePointValue::Redo()
{
	Apply(false);
}

void CEditorActionEditEnvelopePointValue::Apply(bool Undo)
{
	float CurrentValue = fx2f(Undo ? m_OldValue : m_NewValue);
	float CurrentTime = (Undo ? m_OldTime : m_NewTime) / 1000.0f;

	std::shared_ptr<CEnvelope> pEnvelope = m_pEditor->m_Map.m_vpEnvelopes[m_EnvIndex];
	if(m_Type == EType::TANGENT_IN)
	{
		pEnvelope->m_vPoints[m_PtIndex].m_Bezier.m_aInTangentDeltaX[m_Channel] = minimum<int>(CurrentTime * 1000.0f - pEnvelope->m_vPoints[m_PtIndex].m_Time, 0);
		pEnvelope->m_vPoints[m_PtIndex].m_Bezier.m_aInTangentDeltaY[m_Channel] = f2fx(CurrentValue) - pEnvelope->m_vPoints[m_PtIndex].m_aValues[m_Channel];
	}
	else if(m_Type == EType::TANGENT_OUT)
	{
		pEnvelope->m_vPoints[m_PtIndex].m_Bezier.m_aOutTangentDeltaX[m_Channel] = maximum<int>(CurrentTime * 1000.0f - pEnvelope->m_vPoints[m_PtIndex].m_Time, 0);
		pEnvelope->m_vPoints[m_PtIndex].m_Bezier.m_aOutTangentDeltaY[m_Channel] = f2fx(CurrentValue) - pEnvelope->m_vPoints[m_PtIndex].m_aValues[m_Channel];
	}
	else
	{
		if(pEnvelope->GetChannels() == 1 || pEnvelope->GetChannels() == 4)
			CurrentValue = clamp(CurrentValue, 0.0f, 1.0f);
		pEnvelope->m_vPoints[m_PtIndex].m_aValues[m_Channel] = f2fx(CurrentValue);

		if(m_PtIndex != 0)
		{
			pEnvelope->m_vPoints[m_PtIndex].m_Time = CurrentTime * 1000.0f;

			if(pEnvelope->m_vPoints[m_PtIndex].m_Time < pEnvelope->m_vPoints[m_PtIndex - 1].m_Time)
				pEnvelope->m_vPoints[m_PtIndex].m_Time = pEnvelope->m_vPoints[m_PtIndex - 1].m_Time + 1;
			if(static_cast<size_t>(m_PtIndex) + 1 != pEnvelope->m_vPoints.size() && pEnvelope->m_vPoints[m_PtIndex].m_Time > pEnvelope->m_vPoints[m_PtIndex + 1].m_Time)
				pEnvelope->m_vPoints[m_PtIndex].m_Time = pEnvelope->m_vPoints[m_PtIndex + 1].m_Time - 1;
		}
		else
		{
			pEnvelope->m_vPoints[m_PtIndex].m_Time = 0.0f;
		}
	}

	m_pEditor->m_Map.OnModify();
	m_pEditor->m_UpdateEnvPointInfo = true;
}

// ---------------------

CEditorActionResetEnvelopePointTangent::CEditorActionResetEnvelopePointTangent(CEditor *pEditor, int EnvIndex, int PointIndex, int Channel, bool In) :
	IEditorAction(pEditor), m_EnvIndex(EnvIndex), m_PointIndex(PointIndex), m_Channel(Channel), m_In(In)
{
	std::shared_ptr<CEnvelope> pEnvelope = pEditor->m_Map.m_vpEnvelopes[EnvIndex];
	if(In)
	{
		m_Previous[0] = pEnvelope->m_vPoints[PointIndex].m_Bezier.m_aInTangentDeltaX[Channel];
		m_Previous[1] = pEnvelope->m_vPoints[PointIndex].m_Bezier.m_aInTangentDeltaY[Channel];
	}
	else
	{
		m_Previous[0] = pEnvelope->m_vPoints[PointIndex].m_Bezier.m_aOutTangentDeltaX[Channel];
		m_Previous[1] = pEnvelope->m_vPoints[PointIndex].m_Bezier.m_aOutTangentDeltaY[Channel];
	}

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Reset point %d of env %d tangent %s", m_PointIndex, m_EnvIndex, m_In ? "in" : "out");
}

void CEditorActionResetEnvelopePointTangent::Undo()
{
	std::shared_ptr<CEnvelope> pEnvelope = m_pEditor->m_Map.m_vpEnvelopes[m_EnvIndex];
	if(m_In)
	{
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aInTangentDeltaX[m_Channel] = m_Previous[0];
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aInTangentDeltaY[m_Channel] = m_Previous[1];
	}
	else
	{
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aOutTangentDeltaX[m_Channel] = m_Previous[0];
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aOutTangentDeltaY[m_Channel] = m_Previous[1];
	}
	m_pEditor->m_Map.OnModify();
}

void CEditorActionResetEnvelopePointTangent::Redo()
{
	std::shared_ptr<CEnvelope> pEnvelope = m_pEditor->m_Map.m_vpEnvelopes[m_EnvIndex];
	if(m_In)
	{
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aInTangentDeltaX[m_Channel] = 0.0f;
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aInTangentDeltaY[m_Channel] = 0.0f;
	}
	else
	{
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aOutTangentDeltaX[m_Channel] = 0.0f;
		pEnvelope->m_vPoints[m_PointIndex].m_Bezier.m_aOutTangentDeltaY[m_Channel] = 0.0f;
	}
	m_pEditor->m_Map.OnModify();
}

// ------------------

CEditorActionAddEnvelopePoint::CEditorActionAddEnvelopePoint(CEditor *pEditor, int EnvIndex, int Time, ColorRGBA Channels) :
	IEditorAction(pEditor), m_EnvIndex(EnvIndex), m_Time(Time), m_Channels(Channels)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Add new point in envelope %d at time %f", m_EnvIndex, Time / 1000.0);
}

void CEditorActionAddEnvelopePoint::Undo()
{
	// Delete added point
	auto pEnvelope = m_pEditor->m_Map.m_vpEnvelopes[m_EnvIndex];
	auto pIt = std::find_if(pEnvelope->m_vPoints.begin(), pEnvelope->m_vPoints.end(), [this](const CEnvPoint_runtime &Point) {
		return Point.m_Time == m_Time;
	});
	if(pIt != pEnvelope->m_vPoints.end())
	{
		pEnvelope->m_vPoints.erase(pIt);
	}

	m_pEditor->m_Map.OnModify();
}

void CEditorActionAddEnvelopePoint::Redo()
{
	auto pEnvelope = m_pEditor->m_Map.m_vpEnvelopes[m_EnvIndex];
	pEnvelope->AddPoint(m_Time,
		f2fx(m_Channels.r), f2fx(m_Channels.g),
		f2fx(m_Channels.b), f2fx(m_Channels.a));

	m_pEditor->m_Map.OnModify();
}

CEditorActionDeleteEnvelopePoint::CEditorActionDeleteEnvelopePoint(CEditor *pEditor, int EnvIndex, int PointIndex) :
	IEditorAction(pEditor), m_EnvIndex(EnvIndex), m_PointIndex(PointIndex), m_Point(pEditor->m_Map.m_vpEnvelopes[EnvIndex]->m_vPoints[PointIndex])
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Delete point %d of envelope %d", m_PointIndex, m_EnvIndex);
}

void CEditorActionDeleteEnvelopePoint::Undo()
{
	std::shared_ptr<CEnvelope> pEnvelope = m_pEditor->m_Map.m_vpEnvelopes[m_EnvIndex];
	pEnvelope->m_vPoints.insert(pEnvelope->m_vPoints.begin() + m_PointIndex, m_Point);

	m_pEditor->m_Map.OnModify();
}

void CEditorActionDeleteEnvelopePoint::Redo()
{
	std::shared_ptr<CEnvelope> pEnvelope = m_pEditor->m_Map.m_vpEnvelopes[m_EnvIndex];
	pEnvelope->m_vPoints.erase(pEnvelope->m_vPoints.begin() + m_PointIndex);

	auto pSelectedPointIt = std::find_if(m_pEditor->m_vSelectedEnvelopePoints.begin(), m_pEditor->m_vSelectedEnvelopePoints.end(), [this](const std::pair<int, int> Pair) {
		return Pair.first == m_PointIndex;
	});

	if(pSelectedPointIt != m_pEditor->m_vSelectedEnvelopePoints.end())
		m_pEditor->m_vSelectedEnvelopePoints.erase(pSelectedPointIt);

	m_pEditor->m_Map.OnModify();
}

// -------------------------------

CEditorActionEditLayerSoundsProp::CEditorActionEditLayerSoundsProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ELayerSoundsProp Prop, int Previous, int Current) :
	CEditorActionEditLayerPropBase(pEditor, GroupIndex, LayerIndex, Prop, Previous, Current)
{
	static const char *s_apNames[] = {
		"sound"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit sounds layer %d in group %d %s property", m_LayerIndex, m_GroupIndex, s_apNames[(int)m_Prop]);
}

void CEditorActionEditLayerSoundsProp::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEditLayerSoundsProp::Redo()
{
	Apply(m_Current);
}

void CEditorActionEditLayerSoundsProp::Apply(int Value)
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	if(m_Prop == ELayerSoundsProp::PROP_SOUND)
	{
		if(Value >= 0)
			pLayerSounds->m_Sound = Value % m_pEditor->m_Map.m_vpSounds.size();
		else
			pLayerSounds->m_Sound = -1;
	}

	m_pEditor->m_Map.OnModify();
}

// ---

CEditorActionDeleteSoundSource::CEditorActionDeleteSoundSource(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_SourceIndex(SourceIndex)
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	m_Source = pLayerSounds->m_vSources[SourceIndex];

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Delete sound source %d in layer %d of group %d", SourceIndex, LayerIndex, GroupIndex);
}

void CEditorActionDeleteSoundSource::Undo()
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	pLayerSounds->m_vSources.insert(pLayerSounds->m_vSources.begin() + m_SourceIndex, m_Source);
	m_pEditor->m_SelectedSource = m_SourceIndex;
	m_pEditor->m_Map.OnModify();
}

void CEditorActionDeleteSoundSource::Redo()
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	pLayerSounds->m_vSources.erase(pLayerSounds->m_vSources.begin() + m_SourceIndex);
	m_pEditor->m_SelectedSource--;
	m_pEditor->m_Map.OnModify();
}

// ---------------

CEditorActionEditSoundSource::CEditorActionEditSoundSource(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, EEditType Type, int Value) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_SourceIndex(SourceIndex), m_EditType(Type), m_CurrentValue(Value), m_pSavedObject(nullptr)
{
	Save();

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit sound source %d in layer %d of group %d", SourceIndex, LayerIndex, GroupIndex);
}

void CEditorActionEditSoundSource::Undo()
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	CSoundSource *pSource = &pLayerSounds->m_vSources[m_SourceIndex];

	if(m_EditType == EEditType::SHAPE)
	{
		CSoundShape *pSavedShape = (CSoundShape *)m_pSavedObject;
		pSource->m_Shape.m_Type = pSavedShape->m_Type;

		// set default values
		switch(pSource->m_Shape.m_Type)
		{
		case CSoundShape::SHAPE_CIRCLE:
		{
			pSource->m_Shape.m_Circle.m_Radius = pSavedShape->m_Circle.m_Radius;
			break;
		}
		case CSoundShape::SHAPE_RECTANGLE:
		{
			pSource->m_Shape.m_Rectangle.m_Width = pSavedShape->m_Rectangle.m_Width;
			pSource->m_Shape.m_Rectangle.m_Height = pSavedShape->m_Rectangle.m_Height;
			break;
		}
		}
	}

	m_pEditor->m_Map.OnModify();
}

void CEditorActionEditSoundSource::Redo()
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	CSoundSource *pSource = &pLayerSounds->m_vSources[m_SourceIndex];

	if(m_EditType == EEditType::SHAPE)
	{
		pSource->m_Shape.m_Type = m_CurrentValue;

		// set default values
		switch(pSource->m_Shape.m_Type)
		{
		case CSoundShape::SHAPE_CIRCLE:
		{
			pSource->m_Shape.m_Circle.m_Radius = 1000.0f;
			break;
		}
		case CSoundShape::SHAPE_RECTANGLE:
		{
			pSource->m_Shape.m_Rectangle.m_Width = f2fx(1000.0f);
			pSource->m_Shape.m_Rectangle.m_Height = f2fx(800.0f);
			break;
		}
		}
	}

	m_pEditor->m_Map.OnModify();
}

void CEditorActionEditSoundSource::Save()
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	CSoundSource *pSource = &pLayerSounds->m_vSources[m_SourceIndex];

	if(m_EditType == EEditType::SHAPE)
	{
		CSoundShape *pShapeInfo = new CSoundShape;
		pShapeInfo->m_Type = pSource->m_Shape.m_Type;

		switch(pSource->m_Shape.m_Type)
		{
		case CSoundShape::SHAPE_CIRCLE:
		{
			pShapeInfo->m_Circle.m_Radius = pSource->m_Shape.m_Circle.m_Radius;
			break;
		}
		case CSoundShape::SHAPE_RECTANGLE:
		{
			pShapeInfo->m_Rectangle.m_Width = pSource->m_Shape.m_Rectangle.m_Width;
			pShapeInfo->m_Rectangle.m_Height = pSource->m_Shape.m_Rectangle.m_Height;
			break;
		}
		}

		m_pSavedObject = pShapeInfo;
	}
}

CEditorActionEditSoundSource::~CEditorActionEditSoundSource()
{
	if(m_EditType == EEditType::SHAPE)
	{
		CSoundShape *pSavedShape = (CSoundShape *)m_pSavedObject;
		delete pSavedShape;
	}
}

// -----

CEditorActionEditSoundSourceProp::CEditorActionEditSoundSourceProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, ESoundProp Prop, int Previous, int Current) :
	CEditorActionEditLayerPropBase(pEditor, GroupIndex, LayerIndex, Prop, Previous, Current), m_SourceIndex(SourceIndex)
{
	static const char *s_apNames[] = {
		"pos X",
		"pos Y",
		"loop",
		"pan",
		"time delay",
		"falloff",
		"pos env",
		"pos env offset",
		"sound env",
		"sound env offset"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit sound source %d in layer %d of group %d %s property", SourceIndex, LayerIndex, GroupIndex, s_apNames[(int)Prop]);
}

void CEditorActionEditSoundSourceProp::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEditSoundSourceProp::Redo()
{
	Apply(m_Current);
}

void CEditorActionEditSoundSourceProp::Apply(int Value)
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	CSoundSource *pSource = &pLayerSounds->m_vSources[m_SourceIndex];

	if(m_Prop == ESoundProp::PROP_POS_X)
	{
		pSource->m_Position.x = Value;
	}
	else if(m_Prop == ESoundProp::PROP_POS_Y)
	{
		pSource->m_Position.y = Value;
	}
	else if(m_Prop == ESoundProp::PROP_LOOP)
	{
		pSource->m_Loop = Value;
	}
	else if(m_Prop == ESoundProp::PROP_PAN)
	{
		pSource->m_Pan = Value;
	}
	else if(m_Prop == ESoundProp::PROP_TIME_DELAY)
	{
		pSource->m_TimeDelay = Value;
	}
	else if(m_Prop == ESoundProp::PROP_FALLOFF)
	{
		pSource->m_Falloff = Value;
	}
	else if(m_Prop == ESoundProp::PROP_POS_ENV)
	{
		pSource->m_PosEnv = Value;
	}
	else if(m_Prop == ESoundProp::PROP_POS_ENV_OFFSET)
	{
		pSource->m_PosEnvOffset = Value;
	}
	else if(m_Prop == ESoundProp::PROP_SOUND_ENV)
	{
		pSource->m_SoundEnv = Value;
	}
	else if(m_Prop == ESoundProp::PROP_SOUND_ENV_OFFSET)
	{
		pSource->m_SoundEnvOffset = Value;
	}

	m_pEditor->m_Map.OnModify();
}

CEditorActionEditRectSoundSourceShapeProp::CEditorActionEditRectSoundSourceShapeProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, ERectangleShapeProp Prop, int Previous, int Current) :
	CEditorActionEditLayerPropBase(pEditor, GroupIndex, LayerIndex, Prop, Previous, Current), m_SourceIndex(SourceIndex)
{
	static const char *s_apNames[] = {
		"width",
		"height"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit sound source %d in layer %d of group %d sound shape %s property", m_SourceIndex, m_LayerIndex, m_GroupIndex, s_apNames[(int)Prop]);
}

void CEditorActionEditRectSoundSourceShapeProp::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEditRectSoundSourceShapeProp::Redo()
{
	Apply(m_Current);
}

void CEditorActionEditRectSoundSourceShapeProp::Apply(int Value)
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	CSoundSource *pSource = &pLayerSounds->m_vSources[m_SourceIndex];

	if(m_Prop == ERectangleShapeProp::PROP_RECTANGLE_WIDTH)
	{
		pSource->m_Shape.m_Rectangle.m_Width = Value;
	}
	else if(m_Prop == ERectangleShapeProp::PROP_RECTANGLE_HEIGHT)
	{
		pSource->m_Shape.m_Rectangle.m_Height = Value;
	}

	m_pEditor->m_Map.OnModify();
}

CEditorActionEditCircleSoundSourceShapeProp::CEditorActionEditCircleSoundSourceShapeProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, ECircleShapeProp Prop, int Previous, int Current) :
	CEditorActionEditLayerPropBase(pEditor, GroupIndex, LayerIndex, Prop, Previous, Current), m_SourceIndex(SourceIndex)
{
	static const char *s_apNames[] = {
		"radius"};
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Edit sound source %d in layer %d of group %d sound shape %s property", m_SourceIndex, m_LayerIndex, m_GroupIndex, s_apNames[(int)Prop]);
}

void CEditorActionEditCircleSoundSourceShapeProp::Undo()
{
	Apply(m_Previous);
}

void CEditorActionEditCircleSoundSourceShapeProp::Redo()
{
	Apply(m_Current);
}

void CEditorActionEditCircleSoundSourceShapeProp::Apply(int Value)
{
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	CSoundSource *pSource = &pLayerSounds->m_vSources[m_SourceIndex];

	if(m_Prop == ECircleShapeProp::PROP_CIRCLE_RADIUS)
	{
		pSource->m_Shape.m_Circle.m_Radius = Value;
	}

	m_pEditor->m_Map.OnModify();
}

// --------------------------

CEditorActionNewEmptySound::CEditorActionNewEmptySound(CEditor *pEditor, int GroupIndex, int LayerIndex, int x, int y) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_X(x), m_Y(y)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "New sound in layer %d of group %d", LayerIndex, GroupIndex);
}

void CEditorActionNewEmptySound::Undo()
{
	// Undo is simply deleting the added source
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	pLayerSounds->m_vSources.pop_back();

	m_pEditor->m_Map.OnModify();
}

void CEditorActionNewEmptySound::Redo()
{
	auto &Map = m_pEditor->m_Map;
	std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(m_pLayer);
	pLayerSounds->NewSource(m_X, m_Y);

	Map.OnModify();
}

CEditorActionNewEmptyQuad::CEditorActionNewEmptyQuad(CEditor *pEditor, int GroupIndex, int LayerIndex, int x, int y) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_X(x), m_Y(y)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "New quad in layer %d of group %d", LayerIndex, GroupIndex);
}

void CEditorActionNewEmptyQuad::Undo()
{
	// Undo is simply deleting the added quad
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	pLayerQuads->m_vQuads.pop_back();

	m_pEditor->m_Map.OnModify();
}

void CEditorActionNewEmptyQuad::Redo()
{
	auto &Map = m_pEditor->m_Map;
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);

	int Width = 64;
	int Height = 64;
	if(pLayerQuads->m_Image >= 0)
	{
		Width = Map.m_vpImages[pLayerQuads->m_Image]->m_Width;
		Height = Map.m_vpImages[pLayerQuads->m_Image]->m_Height;
	}

	pLayerQuads->NewQuad(m_X, m_Y, Width, Height);

	Map.OnModify();
}

// -------------

CEditorActionNewQuad::CEditorActionNewQuad(CEditor *pEditor, int GroupIndex, int LayerIndex) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex)
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	m_Quad = pLayerQuads->m_vQuads[pLayerQuads->m_vQuads.size() - 1];

	str_format(m_aDisplayText, sizeof(m_aDisplayText), "New quad in layer %d of group %d", LayerIndex, GroupIndex);
}

void CEditorActionNewQuad::Undo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	pLayerQuads->m_vQuads.pop_back();
}

void CEditorActionNewQuad::Redo()
{
	std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(m_pLayer);
	pLayerQuads->m_vQuads.emplace_back(m_Quad);
}

// --------------

CEditorActionMoveSoundSource::CEditorActionMoveSoundSource(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, CPoint OriginalPosition, CPoint CurrentPosition) :
	CEditorActionLayerBase(pEditor, GroupIndex, LayerIndex), m_SourceIndex(SourceIndex), m_OriginalPosition(OriginalPosition), m_CurrentPosition(CurrentPosition)
{
	str_format(m_aDisplayText, sizeof(m_aDisplayText), "Move sound source %d of layer %d in group %d", SourceIndex, LayerIndex, GroupIndex);
}

void CEditorActionMoveSoundSource::Undo()
{
	dbg_assert(m_pLayer->m_Type == LAYERTYPE_SOUNDS, "Layer type does not match a sound layer");
	std::static_pointer_cast<CLayerSounds>(m_pLayer)->m_vSources[m_SourceIndex].m_Position = m_OriginalPosition;
}

void CEditorActionMoveSoundSource::Redo()
{
	dbg_assert(m_pLayer->m_Type == LAYERTYPE_SOUNDS, "Layer type does not match a sound layer");
	std::static_pointer_cast<CLayerSounds>(m_pLayer)->m_vSources[m_SourceIndex].m_Position = m_CurrentPosition;
}
