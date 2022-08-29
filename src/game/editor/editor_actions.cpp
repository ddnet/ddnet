#include <game/editor/editor_actions.h>

bool CEditorChangeColorTileAction::Undo()
{
	//CLayerTiles *pTileLayer = (CLayerTiles *)m_pObject;
	CLayerTiles *pTileLayer = (CLayerTiles *)m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers[m_LayerIndex];
	pTileLayer->m_Color = m_ValueFrom;
	return true;
}

bool CEditorChangeColorTileAction::Redo()
{
	//CLayerTiles *pTileLayer = (CLayerTiles *)m_pObject;
	CLayerTiles *pTileLayer = (CLayerTiles *)m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers[m_LayerIndex];
	pTileLayer->m_Color = m_ValueTo;
	return true;
}

bool CEditorAddLayerAction::Undo()
{
	CLayerGroup *Group = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];
	CLayer *Layer = Group->m_vpLayers[m_LayerIndex];

	if(Layer == m_pEditor->m_Map.m_pFrontLayer)
		m_pEditor->m_Map.m_pFrontLayer = nullptr;
	if(Layer == m_pEditor->m_Map.m_pTeleLayer)
		m_pEditor->m_Map.m_pTeleLayer = nullptr;
	if(Layer == m_pEditor->m_Map.m_pSpeedupLayer)
		m_pEditor->m_Map.m_pSpeedupLayer = nullptr;
	if(Layer == m_pEditor->m_Map.m_pSwitchLayer)
		m_pEditor->m_Map.m_pSwitchLayer = nullptr;
	if(Layer == m_pEditor->m_Map.m_pTuneLayer)
		m_pEditor->m_Map.m_pTuneLayer = nullptr;

	Group->DeleteLayer(m_LayerIndex);

	if(m_pEditor->m_SelectedGroup == m_GroupIndex)
		m_pEditor->SelectLayer(maximum(0, m_LayerIndex - 1));

	return true;
}

CEditorDeleteLayerAction::CEditorDeleteLayerAction(CEditor *pEditor, int GroupIndex, int LayerIndex) :
	CEditorAction(CEditorAction::EType::DELETE_LAYER, nullptr, pEditor->m_Map.m_vpGroups[GroupIndex]->m_vpLayers[LayerIndex]->Duplicate(), nullptr)
{
	m_GroupIndex = GroupIndex;
	m_LayerIndex = LayerIndex;
}

bool CEditorDeleteLayerAction::Undo()
{
	CLayerGroup *Group = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	CLayer *pLayer = m_ValueFrom->Duplicate();
	if(pLayer->m_Type == LAYERTYPE_TILES)
	{
		CLayerTiles *pTilesLayer = (CLayerTiles *)pLayer;
		if(pTilesLayer->m_Tele)
			m_pEditor->m_Map.m_pTeleLayer = (CLayerTele *)pLayer;
		else if(pTilesLayer->m_Tune)
			m_pEditor->m_Map.m_pTuneLayer = (CLayerTune *)pLayer;
		else if(pTilesLayer->m_Speedup)
			m_pEditor->m_Map.m_pSpeedupLayer = (CLayerSpeedup *)pLayer;
		else if(pTilesLayer->m_Switch)
			m_pEditor->m_Map.m_pSwitchLayer = (CLayerSwitch *)pLayer;
		else if(pTilesLayer->m_Front)
			m_pEditor->m_Map.m_pFrontLayer = (CLayerFront *)pLayer;
	}

	Group->m_vpLayers.insert(Group->m_vpLayers.begin() + m_LayerIndex, pLayer);
	m_pEditor->m_Map.m_Modified = true;

	if(m_pEditor->m_SelectedGroup == m_GroupIndex)
		m_pEditor->SelectLayer(m_LayerIndex);

	return true;
}

bool CEditorDeleteLayerAction::Redo()
{
	CLayerGroup *Group = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];
	CLayer *pLayer = m_ValueFrom;

	if(pLayer->m_Type == LAYERTYPE_TILES)
	{
		CLayerTiles *pTilesLayer = (CLayerTiles *)pLayer;
		if(pTilesLayer->m_Tele)
			m_pEditor->m_Map.m_pTeleLayer = nullptr;
		else if(pTilesLayer->m_Tune)
			m_pEditor->m_Map.m_pTuneLayer = nullptr;
		else if(pTilesLayer->m_Speedup)
			m_pEditor->m_Map.m_pSpeedupLayer = nullptr;
		else if(pTilesLayer->m_Switch)
			m_pEditor->m_Map.m_pSwitchLayer = nullptr;
		else if(pTilesLayer->m_Front)
			m_pEditor->m_Map.m_pFrontLayer = nullptr;
	}

	Group->DeleteLayer(m_LayerIndex);

	if(m_pEditor->m_SelectedGroup == m_GroupIndex)
		m_pEditor->SelectLayer(maximum(0, m_LayerIndex - 1));

	return true;
}

CEditorEditMultipleLayersAction::CEditorEditMultipleLayersAction(void *pObject, std::vector<CLayerTiles_SCommonPropState> vOriginals, CLayerTiles_SCommonPropState State, int GroupIndex, std::vector<int> vLayers) :
	CEditorAction(CEditorAction::EType::EDIT_MULTIPLE_LAYERS, pObject, vOriginals, {State})
{
	m_GroupIndex = GroupIndex;
	m_vLayers = vLayers;
}

bool CEditorEditMultipleLayersAction::Undo()
{
	if(m_vLayers.size() != m_ValueFrom.size())
		return false;

	for(size_t k = 0; k < m_vLayers.size(); k++)
	{
		int LayerIndex = m_vLayers[k];
		CLayerTiles *pLayer = (CLayerTiles *)m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers[LayerIndex];
		CLayerTiles_SCommonPropState Original = m_ValueFrom[k];

		pLayer->Resize(Original.m_Width, Original.m_Height);

		pLayer->m_Color.r = (Original.m_Color >> 24) & 0xff;
		pLayer->m_Color.g = (Original.m_Color >> 16) & 0xff;
		pLayer->m_Color.b = (Original.m_Color >> 8) & 0xff;
		pLayer->m_Color.a = Original.m_Color & 0xff;

		pLayer->FlagModified(0, 0, pLayer->m_Width, pLayer->m_Height);
	}

	return true;
}

bool CEditorEditMultipleLayersAction::Redo()
{
	CLayerTiles_SCommonPropState State = m_ValueTo[0];
	for(auto LayerIndex : m_vLayers)
	{
		CLayerTiles *pLayer = (CLayerTiles *)m_pEditor->m_Map.m_vpGroups[m_GroupIndex]->m_vpLayers[LayerIndex];

		if((State.m_Modified & CLayerTiles_SCommonPropState::MODIFIED_SIZE) != 0)
			pLayer->Resize(State.m_Width, State.m_Height);

		if((State.m_Modified & CLayerTiles_SCommonPropState::MODIFIED_COLOR) != 0)
		{
			pLayer->m_Color.r = (State.m_Color >> 24) & 0xff;
			pLayer->m_Color.g = (State.m_Color >> 16) & 0xff;
			pLayer->m_Color.b = (State.m_Color >> 8) & 0xff;
			pLayer->m_Color.a = State.m_Color & 0xff;
		}

		pLayer->FlagModified(0, 0, pLayer->m_Width, pLayer->m_Height);
	}

	return true;
}

CEditorAddGroupAction::CEditorAddGroupAction(CEditorMap *pObject, int GroupIndex) :
	CEditorAction(EType::ADD_GROUP, pObject, -1, GroupIndex)
{
}

bool CEditorAddGroupAction::Undo()
{
	CEditorMap *pMap = (CEditorMap *)m_pObject;
	pMap->DeleteGroup(m_ValueTo);

	m_pEditor->m_SelectedGroup = maximum(0, m_ValueTo - 1);

	return true;
}

bool CEditorAddGroupAction::Redo()
{
	CEditorMap *pMap = (CEditorMap *)m_pObject;
	pMap->NewGroup();

	m_pEditor->m_SelectedGroup = m_ValueTo;

	return true;
}

CEditorDeleteGroupAction::CEditorDeleteGroupAction(CEditorMap *pObject, int GroupIndex) :
	CEditorAction(CEditorAction::EType::DELETE_GROUP, pObject, new CLayerGroup(*pObject->m_vpGroups[GroupIndex]), nullptr)
{
	m_GroupIndex = GroupIndex;
}

bool CEditorDeleteGroupAction::Undo()
{
	// undo is re-adding back the group
	CEditorMap *pMap = (CEditorMap *)m_pObject;
	pMap->m_vpGroups.insert(pMap->m_vpGroups.begin() + m_GroupIndex, new CLayerGroup(*m_ValueFrom));
	pMap->m_Modified = true;

	m_pEditor->m_SelectedGroup = m_GroupIndex;

	return true;
}

bool CEditorDeleteGroupAction::Redo()
{
	// redo is deleting the group, that's easy
	CEditorMap *pMap = (CEditorMap *)m_pObject;
	pMap->DeleteGroup(m_GroupIndex);

	m_pEditor->m_SelectedGroup = maximum(0, m_GroupIndex);

	return true;
}

CEditorFillSelectionAction::CEditorFillSelectionAction(void *pObject, CLayerGroup *Original, CLayerGroup *Brush, int GroupIndex, std::vector<int> vLayers, CUIRect Rect) :
	CEditorAction(CEditorAction::EType::FILL_SELECTION, pObject, new CLayerGroup(*Original), new CLayerGroup(*Brush))
{
	m_Rect = Rect;
	m_vLayers = vLayers;
	m_GroupIndex = GroupIndex;
}

bool CEditorFillSelectionAction::Undo()
{
	bool OldDestructiveMode = m_pEditor->m_BrushDrawDestructive;
	m_pEditor->m_BrushDrawDestructive = true;

	// undo is filling back the original tiles
	CLayerGroup *Brush = m_ValueFrom;
	CLayerGroup *Group = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	size_t NumLayers = m_vLayers.size();
	for(size_t k = 0; k < NumLayers; k++)
	{
		int LayerIndex = m_vLayers[k];
		CLayer *pLayer = Group->m_vpLayers[LayerIndex];

		size_t BrushIndex = k;
		if(Brush->m_vpLayers.size() != NumLayers)
			BrushIndex = 0;
		CLayer *pBrush = Brush->IsEmpty() ? nullptr : Brush->m_vpLayers[BrushIndex];
		//dbg_msg("editor", "undoing layer %d, brush index=%d, brush=%p", k, BrushIndex, pBrush);

		pLayer->FillSelection(Brush->IsEmpty(), pBrush, m_Rect);
	}

	m_pEditor->m_BrushDrawDestructive = OldDestructiveMode;
	return true;
}

bool CEditorFillSelectionAction::Redo()
{
	bool OldDestructiveMode = m_pEditor->m_BrushDrawDestructive;
	m_pEditor->m_BrushDrawDestructive = true;

	// redo is redoing the filling with the new tiles
	CLayerGroup *Brush = m_ValueTo;
	CLayerGroup *Group = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	size_t NumLayers = m_vLayers.size();
	for(size_t k = 0; k < NumLayers; k++)
	{
		int LayerIndex = m_vLayers[k];
		CLayer *pLayer = Group->m_vpLayers[LayerIndex];

		size_t BrushIndex = k;
		if(Brush->m_vpLayers.size() != NumLayers)
			BrushIndex = 0;
		CLayer *pBrush = Brush->IsEmpty() ? nullptr : Brush->m_vpLayers[BrushIndex];

		pLayer->FillSelection(Brush->IsEmpty(), pBrush, m_Rect);
	}

	m_pEditor->m_BrushDrawDestructive = OldDestructiveMode;
	return true;
}

void CEditorFillSelectionAction::Print()
{
	dbg_msg("editor", "editor fill action, num layers=%d, original brush num layers=%d, modified brush num layers=%d", m_vLayers.size(), m_ValueFrom->m_vpLayers.size(), m_ValueTo->m_vpLayers.size());
	dbg_msg("editor", "rect: w=%f h=%f", m_Rect.w, m_Rect.h);
}

CEditorBrushDrawAction::CEditorBrushDrawAction(void *pObject, CLayerGroup *Original, CLayerGroup *Brush, int GroupIndex, std::vector<int> vLayers) :
	CEditorAction(EType::BRUSH_DRAW, pObject, new CLayerGroup(*Original), new CLayerGroup(*Brush))
{
	m_vLayers = vLayers;
	m_GroupIndex = GroupIndex;
}

bool CEditorBrushDrawAction::Undo()
{
	// undo is drawing with brush with original data
	return BrushDraw(m_ValueFrom);
}

bool CEditorBrushDrawAction::Redo()
{
	// redo is drawing with brush with modified data
	return BrushDraw(m_ValueTo);
}

bool CEditorBrushDrawAction::BrushDraw(CLayerGroup *Brush)
{
	bool OldDestructiveMode = m_pEditor->m_BrushDrawDestructive;
	m_pEditor->m_BrushDrawDestructive = true;

	CLayerGroup *Group = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];

	int NumLayers = m_vLayers.size();

	if(NumLayers != Brush->m_vpLayers.size())
		return false;

	for(size_t k = 0; k < NumLayers; k++)
	{
		int LayerIndex = m_vLayers[k];
		CLayer *pLayer = Group->m_vpLayers[LayerIndex];
		size_t BrushIndex = k;

		if(pLayer->m_Type == Brush->m_vpLayers[BrushIndex]->m_Type)
		{
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayerTiles = (CLayerTiles *)pLayer;
				CLayerTiles *pBrushLayer = (CLayerTiles *)Brush->m_vpLayers[BrushIndex];

				pLayerTiles->BrushDraw(pBrushLayer, 0, 0);
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CLayerQuads *pLayerQuads = (CLayerQuads *)pLayer;
				CLayerQuads *pBrushLayer = (CLayerQuads *)Brush->m_vpLayers[BrushIndex];

				pLayerQuads->m_vQuads.clear();
				pLayerQuads->m_vQuads = pBrushLayer->m_vQuads;
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				CLayerSounds *pLayerSounds = (CLayerSounds *)pLayer;
				CLayerSounds *pBrushLayer = (CLayerSounds *)Brush->m_vpLayers[BrushIndex];

				pLayerSounds->m_Sound = pBrushLayer->m_Sound;

				pLayerSounds->m_vSources.clear();
				pLayerSounds->m_vSources = pBrushLayer->m_vSources;
			}
		}
	}

	m_pEditor->m_BrushDrawDestructive = OldDestructiveMode;
	return true;
}

void CEditorBrushDrawAction::Print()
{
	CLayerGroup *Group = m_pEditor->m_Map.m_vpGroups[m_GroupIndex];
	dbg_msg("editor", "Undo action: brush draw.");
	//for(size_t k = 0; k < m_vLayers.size(); k++)
	//{
	//	dbg_msg("editor", "   k=%d, layer index=%d (%s)", k, m_vLayers[k], Group->m_vpLayers[m_vLayers[k]]->m_aName);
	//	dbg_msg("editor", "   brush layer (original) -> %s", m_ValueFrom->m_vpLayers[k]->m_aName);
	//	dbg_msg("editor", "   brush layer (modified) -> %s", m_ValueTo->m_vpLayers[k]->m_aName);
	//}
}

bool CEditorCommandAction::Undo()
{
	switch(m_Type)
	{
	case CEditorAction::EType::ADD_COMMAND:
	{
		m_pEditor->m_Map.m_vSettings.pop_back();
		*m_pSelectedCommand = m_pEditor->m_Map.m_vSettings.size() - 1;
		break;
	}
	case CEditorAction::EType::EDIT_COMMAND:
	{
		str_copy(m_pEditor->m_Map.m_vSettings[m_CommandIndex].m_aCommand, m_ValueFrom.c_str(), sizeof(m_pEditor->m_Map.m_vSettings[m_CommandIndex].m_aCommand));
		break;
	}
	case CEditorAction::EType::DELETE_COMMAND:
	{
		CEditorMap::CSetting Setting;
		str_copy(Setting.m_aCommand, m_ValueFrom.c_str(), sizeof(Setting.m_aCommand));
		m_pEditor->m_Map.m_vSettings.insert(m_pEditor->m_Map.m_vSettings.begin() + m_CommandIndex, Setting);
		*m_pSelectedCommand = m_CommandIndex;
		break;
	}
	case CEditorAction::EType::MOVE_COMMAND_UP:
	{
		std::swap(m_pEditor->m_Map.m_vSettings[m_CommandIndex], m_pEditor->m_Map.m_vSettings[m_CommandIndex - 1]);
		*m_pSelectedCommand = m_CommandIndex;
		break;
	}
	case CEditorAction::EType::MOVE_COMMAND_DOWN:
	{
		std::swap(m_pEditor->m_Map.m_vSettings[m_CommandIndex], m_pEditor->m_Map.m_vSettings[m_CommandIndex + 1]);
		*m_pSelectedCommand = m_CommandIndex;
		break;
	}
	default:
		return false;
	}
	return true;
}

bool CEditorCommandAction::Redo()
{
	switch(m_Type)
	{
	case CEditorAction::EType::ADD_COMMAND:
	{
		CEditorMap::CSetting Setting;
		str_copy(Setting.m_aCommand, m_ValueTo.c_str(), sizeof(Setting.m_aCommand));
		m_pEditor->m_Map.m_vSettings.push_back(Setting);
		*m_pSelectedCommand = m_pEditor->m_Map.m_vSettings.size() - 1;
		break;
	}
	case CEditorAction::EType::EDIT_COMMAND:
		str_copy(m_pEditor->m_Map.m_vSettings[m_CommandIndex].m_aCommand, m_ValueTo.c_str(), sizeof(m_pEditor->m_Map.m_vSettings[m_CommandIndex].m_aCommand));
		break;
	case CEditorAction::EType::DELETE_COMMAND:
	{
		m_pEditor->m_Map.m_vSettings.erase(m_pEditor->m_Map.m_vSettings.begin() + m_CommandIndex);

		if(*m_pSelectedCommand >= (int)m_pEditor->m_Map.m_vSettings.size())
			*m_pSelectedCommand = m_pEditor->m_Map.m_vSettings.size() - 1;

		break;
	}
	case CEditorAction::EType::MOVE_COMMAND_UP:
	{
		std::swap(m_pEditor->m_Map.m_vSettings[m_CommandIndex], m_pEditor->m_Map.m_vSettings[m_CommandIndex - 1]);
		*m_pSelectedCommand = m_CommandIndex;
		break;
	}
	case CEditorAction::EType::MOVE_COMMAND_DOWN:
	{
		std::swap(m_pEditor->m_Map.m_vSettings[m_CommandIndex], m_pEditor->m_Map.m_vSettings[m_CommandIndex + 1]);
		*m_pSelectedCommand = m_CommandIndex;
		break;
	}
	default:
		return false;
	}
	return true;
}

CEditorAddQuadAction::CEditorAddQuadAction(int GroupIndex, int LayerIndex, CQuad *pQuad) :
	CEditorLayerAction(EType::ADD_QUAD, nullptr, GroupIndex, LayerIndex, nullptr, new CQuad(*pQuad))
{
}

CEditorAddQuadAction::~CEditorAddQuadAction()
{
	delete m_ValueTo;
}

bool CEditorAddQuadAction::Undo()
{
	CLayerQuads *pLayer = GetLayer<CLayerQuads>();
	pLayer->m_vQuads.pop_back();

	m_pEditor->m_Map.m_Modified = true;

	return true;
}

bool CEditorAddQuadAction::Redo()
{
	CLayerQuads *pLayer = GetLayer<CLayerQuads>();
	pLayer->m_vQuads.push_back(*m_ValueTo);

	m_pEditor->m_Map.m_Modified = true;

	return true;
}

bool CEditorDeleteQuadsAction::Undo()
{
	CLayerQuads *pLayer = GetLayer<CLayerQuads>();
	pLayer->m_vQuads.resize(pLayer->m_vQuads.size() + m_vQuads.size());

	size_t k = 0;
	for(auto &Index : m_vQuadIndexes)
	{
		pLayer->m_vQuads[Index] = m_vQuads[k];
		k++;
	}

	return true;
}

bool CEditorDeleteQuadsAction::Redo()
{
	CLayerQuads *pLayer = GetLayer<CLayerQuads>();
	std::vector vSelectedQuads = m_vQuadIndexes;

	for(int i = 0; i < (int)vSelectedQuads.size(); ++i)
	{
		pLayer->m_vQuads.erase(pLayer->m_vQuads.begin() + vSelectedQuads[i]);
		for(int j = i + 1; j < (int)vSelectedQuads.size(); ++j)
			if(vSelectedQuads[j] > vSelectedQuads[i])
				vSelectedQuads[j]--;

		vSelectedQuads.erase(vSelectedQuads.begin() + i);
		i--;
	}

	return true;
}

CEditorEditQuadPositionAction::CEditorEditQuadPositionAction(int GroupIndex, int LayerIndex, std::vector<int> vQuads, const std::vector<CPoint> &OldPos, const std::vector<CPoint> &NewPos) :
	CEditorLayerQuadsAction(EType::EDIT_QUAD_POSITION, GroupIndex, LayerIndex, vQuads, OldPos, NewPos)
{
}

bool CEditorEditQuadPositionAction::Undo()
{
	Move(-1);
	return true;
}

bool CEditorEditQuadPositionAction::Redo()
{
	Move(1);
	return true;
}

void CEditorEditQuadPositionAction::Move(int Direction)
{
	CLayerQuads *pLayer = GetLayer();

	size_t k = 0;
	for(auto &QuadIndex : m_vQuads)
	{
		CQuad *pQuad = &pLayer->m_vQuads[QuadIndex];

		float OffsetX = m_ValueTo[k].x - m_ValueFrom[k].x;
		float OffsetY = m_ValueTo[k].y - m_ValueFrom[k].y;

		for(auto &QuadPoint : pQuad->m_aPoints)
		{
			QuadPoint.x += Direction * OffsetX;
			QuadPoint.y += Direction * OffsetY;
		}

		k++;
	}
}

bool CEditorEditQuadsPointsAction::Undo()
{
	CLayerQuads *pLayer = GetLayer();

	size_t k = 0;
	for(auto &QuadIndex : m_vQuads)
	{
		auto *pQuad = &pLayer->m_vQuads[QuadIndex];
		for(int v = 0; v < 4; v++)
		{
			if(m_Points & (1 << v))
			{
				if((m_ValueFrom[k].m_Modified & SEditQuadPoint::MODIFIED_COLOR) != 0)
					pQuad->m_aColors[v] = m_ValueFrom[k].m_Quad.m_aColors[v];
				if((m_ValueFrom[k].m_Modified & SEditQuadPoint::MODIFIED_POS_X) != 0)
					pQuad->m_aPoints[v].x = m_ValueFrom[k].m_Quad.m_aPoints[v].x;
				if((m_ValueFrom[k].m_Modified & SEditQuadPoint::MODIFIED_POS_Y) != 0)
					pQuad->m_aPoints[v].y = m_ValueFrom[k].m_Quad.m_aPoints[v].y;
				if((m_ValueFrom[k].m_Modified & SEditQuadPoint::MODIFIED_TEX_U) != 0)
					pQuad->m_aTexcoords[v].u = m_ValueFrom[k].m_Quad.m_aTexcoords[v].u;
				if((m_ValueFrom[k].m_Modified & SEditQuadPoint::MODIFIED_TEX_V) != 0)
					pQuad->m_aTexcoords[v].v = m_ValueFrom[k].m_Quad.m_aTexcoords[v].v;

				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_TEX_OFFSET) != 0)
				{
					pQuad->m_aTexcoords[v] = m_ValueFrom[k].m_Quad.m_aTexcoords[v];
					pQuad->m_aTexcoords[(v + 2) % 4].x = m_ValueFrom[k].m_Quad.m_aTexcoords[(v + 2) % 4].x;
					pQuad->m_aTexcoords[v ^ 1].y = m_ValueFrom[k].m_Quad.m_aTexcoords[v ^ 1].y;
				}
			}
		}
		k++;
	}

	return true;
}

bool CEditorEditQuadsPointsAction::Redo()
{
	CLayerQuads *pLayer = GetLayer();

	for(auto &QuadIndex : m_vQuads)
	{
		auto *pQuad = &pLayer->m_vQuads[QuadIndex];
		for(int v = 0; v < 4; v++)
		{
			if(m_Points & (1 << v))
			{
				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_COLOR) != 0)
					pQuad->m_aColors[v] = m_ValueTo[0].m_Quad.m_aColors[v];
				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_POS_X) != 0)
					pQuad->m_aPoints[v].x = m_ValueTo[0].m_Quad.m_aPoints[v].x;
				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_POS_Y) != 0)
					pQuad->m_aPoints[v].y = m_ValueTo[0].m_Quad.m_aPoints[v].y;
				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_TEX_U) != 0)
					pQuad->m_aTexcoords[v].u = m_ValueTo[0].m_Quad.m_aTexcoords[v].u;
				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_TEX_V) != 0)
					pQuad->m_aTexcoords[v].v = m_ValueTo[0].m_Quad.m_aTexcoords[v].v;

				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_OFFSET) != 0)
				{
					// use the stored offset here
					pQuad->m_aPoints[v].x += m_ValueTo[0].m_Quad.m_aPoints[0].x;
					pQuad->m_aPoints[v].y += m_ValueTo[0].m_Quad.m_aPoints[0].y;
				}

				if((m_ValueTo[0].m_Modified & SEditQuadPoint::MODIFIED_TEX_OFFSET) != 0)
				{
					// use the stored offset here
					pQuad->m_aTexcoords[v].u += m_ValueTo[0].m_Quad.m_aTexcoords[0].u;
					pQuad->m_aTexcoords[v].v += m_ValueTo[0].m_Quad.m_aTexcoords[0].v;

					pQuad->m_aTexcoords[(v + 2) % 4].x += m_ValueTo[0].m_Quad.m_aTexcoords[0].u;
					pQuad->m_aTexcoords[v ^ 1].y += m_ValueTo[0].m_Quad.m_aTexcoords[0].v;
				}
			}
		}
	}

	return true;
}

CEditorRotateQuadsAction::CEditorRotateQuadsAction(int GroupIndex, int LayerIndex, std::vector<int> vQuads, const float &PrevRotation, const float &NewRotation) :
	CEditorLayerQuadsAction(EType::ROTATE_QUAD, GroupIndex, LayerIndex, vQuads, PrevRotation, NewRotation)
{
}

bool CEditorRotateQuadsAction::Undo()
{
	RotateInternal(-1);
	return true;
}

bool CEditorRotateQuadsAction::Redo()
{
	RotateInternal(1);
	return true;
}

void CEditorRotateQuadsAction::RotateInternal(int Direction)
{
	CLayerQuads *pLayer = GetLayer();
	for(auto &QuadIndex : m_vQuads)
	{
		CQuad *pQuad = &pLayer->m_vQuads[QuadIndex];
		for(int v = 0; v < 4; v++)
			CEditor::Rotate(&pQuad->m_aPoints[4], &pQuad->m_aPoints[v], Direction == -1 ? m_ValueFrom : m_ValueTo);
	}
}

CEditorMoveQuadPivotAction::CEditorMoveQuadPivotAction(int GroupIndex, int LayerIndex, int Quad, const CPoint &OldPos, const CPoint &NewPos) :
	CEditorLayerQuadsAction(EType::EDIT_QUAD_CENTER, GroupIndex, LayerIndex, {Quad}, OldPos, NewPos)
{
}

bool CEditorMoveQuadPivotAction::Undo()
{
	CLayerQuads *pLayer = GetLayer();
	pLayer->m_vQuads[m_vQuads[0]].m_aPoints[4] = m_ValueFrom;

	return true;
}

bool CEditorMoveQuadPivotAction::Redo()
{
	CLayerQuads *pLayer = GetLayer();
	pLayer->m_vQuads[m_vQuads[0]].m_aPoints[4] = m_ValueTo;

	return true;
}

CEditorQuadOperationAction::CEditorQuadOperationAction(EType Type, int GroupIndex, int LayerIndex, std::vector<int> vQuads, const std::vector<SQuadOperationInfo> &OldInfos, const std::vector<SQuadOperationInfo> &NewInfos) :
	CEditorLayerQuadsAction(Type, GroupIndex, LayerIndex, vQuads, OldInfos, NewInfos)
{
}

bool CEditorQuadOperationAction::Undo()
{
	CLayerQuads *pLayer = GetLayer();

	size_t k = 0;
	for(auto &QuadIndex : m_vQuads)
	{
		CQuad *pQuad = &pLayer->m_vQuads[QuadIndex];

		for(int v = 0; v < 4; v++)
			pQuad->m_aPoints[v] = m_ValueFrom[k].m_aPoints[v];

		k++;
	}

	return true;
}

bool CEditorQuadOperationAction::Redo()
{
	CLayerQuads *pLayer = GetLayer();

	size_t k = 0;
	for(auto &QuadIndex : m_vQuads)
	{
		CQuad *pQuad = &pLayer->m_vQuads[QuadIndex];

		for(int v = 0; v < 4; v++)
			pQuad->m_aPoints[v] = m_ValueTo[k].m_aPoints[v];

		k++;
	}

	return true;
}