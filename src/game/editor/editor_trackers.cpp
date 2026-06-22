#include "editor_trackers.h"

#include "editor.h"
#include "editor_actions.h"

#include <game/editor/mapitems/layer_group.h>
#include <game/editor/mapitems/layer_tiles.h>

CQuadEditTracker::CQuadEditTracker(CEditorMap *pMap) :
	CMapObject(pMap),
	m_TrackedProp(EQuadProp::NONE) {}

bool CQuadEditTracker::QuadPointChanged(const std::vector<CPoint> &vCurrentPoints, int QuadIndex)
{
	for(size_t i = 0; i < vCurrentPoints.size(); i++)
	{
		if(vCurrentPoints[i] != m_InitialPoints[QuadIndex][i])
			return true;
	}
	return false;
}

bool CQuadEditTracker::QuadColorChanged(const std::vector<CColor> &vCurrentColors, int QuadIndex)
{
	for(size_t i = 0; i < vCurrentColors.size(); i++)
	{
		if(vCurrentColors[i] != m_InitialColors[QuadIndex][i])
			return true;
	}
	return false;
}

void CQuadEditTracker::BeginQuadTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int GroupIndex, int LayerIndex)
{
	if(m_Tracking)
		return;
	m_Tracking = true;
	m_vSelectedQuads.clear();
	m_pLayer = pLayer;
	m_GroupIndex = GroupIndex < 0 ? Map()->m_SelectedGroup : GroupIndex;
	m_LayerIndex = LayerIndex < 0 ? Map()->m_vSelectedLayers[0] : LayerIndex;
	// Init all points
	for(auto QuadIndex : vSelectedQuads)
	{
		auto &pQuad = pLayer->m_vQuads[QuadIndex];
		m_InitialPoints[QuadIndex] = std::vector<CPoint>(std::begin(pQuad.m_aPoints), std::end(pQuad.m_aPoints));
		m_vSelectedQuads.push_back(QuadIndex);
	}
}

void CQuadEditTracker::EndQuadTrack()
{
	if(!m_Tracking)
		return;
	m_Tracking = false;

	// Record all moved stuff
	std::vector<std::shared_ptr<IEditorAction>> vpActions;
	for(auto QuadIndex : m_vSelectedQuads)
	{
		auto &pQuad = m_pLayer->m_vQuads[QuadIndex];
		auto vCurrentPoints = std::vector<CPoint>(std::begin(pQuad.m_aPoints), std::end(pQuad.m_aPoints));
		if(QuadPointChanged(vCurrentPoints, QuadIndex))
			vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, m_InitialPoints[QuadIndex], vCurrentPoints));
	}

	if(!vpActions.empty())
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(Map(), vpActions));
}

void CQuadEditTracker::BeginQuadPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, EQuadProp Prop, int GroupIndex, int LayerIndex)
{
	if(m_TrackedProp != EQuadProp::NONE)
		return;
	m_TrackedProp = Prop;
	m_pLayer = pLayer;
	m_GroupIndex = GroupIndex < 0 ? Map()->m_SelectedGroup : GroupIndex;
	m_LayerIndex = LayerIndex < 0 ? Map()->m_vSelectedLayers[0] : LayerIndex;
	m_vSelectedQuads = vSelectedQuads;
	m_PreviousValues.clear();

	for(auto QuadIndex : vSelectedQuads)
	{
		auto &Quad = pLayer->m_vQuads[QuadIndex];
		if(Prop == EQuadProp::POS_X || Prop == EQuadProp::POS_Y)
			m_InitialPoints[QuadIndex] = std::vector<CPoint>(std::begin(Quad.m_aPoints), std::end(Quad.m_aPoints));
		else if(Prop == EQuadProp::POS_ENV)
			m_PreviousValues[QuadIndex] = Quad.m_PosEnv;
		else if(Prop == EQuadProp::POS_ENV_OFFSET)
			m_PreviousValues[QuadIndex] = Quad.m_PosEnvOffset;
		else if(Prop == EQuadProp::COLOR)
			m_InitialColors[QuadIndex] = std::vector<CColor>(std::begin(Quad.m_aColors), std::end(Quad.m_aColors));
		else if(Prop == EQuadProp::COLOR_ENV)
			m_PreviousValues[QuadIndex] = Quad.m_ColorEnv;
		else if(Prop == EQuadProp::COLOR_ENV_OFFSET)
			m_PreviousValues[QuadIndex] = Quad.m_ColorEnvOffset;
	}
}
void CQuadEditTracker::EndQuadPropTrack(EQuadProp Prop)
{
	if(m_TrackedProp != Prop)
		return;
	m_TrackedProp = EQuadProp::NONE;

	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	for(auto QuadIndex : m_vSelectedQuads)
	{
		auto &Quad = m_pLayer->m_vQuads[QuadIndex];
		if(Prop == EQuadProp::POS_X || Prop == EQuadProp::POS_Y)
		{
			auto vCurrentPoints = std::vector<CPoint>(std::begin(Quad.m_aPoints), std::end(Quad.m_aPoints));
			if(QuadPointChanged(vCurrentPoints, QuadIndex))
				vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, m_InitialPoints[QuadIndex], vCurrentPoints));
		}
		else if(Prop == EQuadProp::COLOR)
		{
			auto vCurrentColors = std::vector<CColor>(std::begin(Quad.m_aColors), std::end(Quad.m_aColors));
			if(QuadColorChanged(vCurrentColors, QuadIndex))
				vpActions.push_back(std::make_shared<CEditorActionEditQuadColor>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, m_InitialColors[QuadIndex], vCurrentColors));
		}
		else
		{
			int Value = 0;
			if(Prop == EQuadProp::POS_ENV)
				Value = Quad.m_PosEnv;
			else if(Prop == EQuadProp::POS_ENV_OFFSET)
				Value = Quad.m_PosEnvOffset;
			else if(Prop == EQuadProp::COLOR_ENV)
				Value = Quad.m_ColorEnv;
			else if(Prop == EQuadProp::COLOR_ENV_OFFSET)
				Value = Quad.m_ColorEnvOffset;

			if(Value != m_PreviousValues[QuadIndex])
				vpActions.push_back(std::make_shared<CEditorActionEditQuadProp>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, Prop, m_PreviousValues[QuadIndex], Value));
		}
	}

	if(!vpActions.empty())
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(Map(), vpActions));
}

void CQuadEditTracker::BeginQuadPointPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int SelectedQuadPoints, int GroupIndex, int LayerIndex)
{
	if(!m_vTrackedProps.empty())
		return;

	m_pLayer = pLayer;
	m_GroupIndex = GroupIndex < 0 ? Map()->m_SelectedGroup : GroupIndex;
	m_LayerIndex = LayerIndex < 0 ? Map()->m_vSelectedLayers[0] : LayerIndex;
	m_SelectedQuadPoints = SelectedQuadPoints;
	m_vSelectedQuads = vSelectedQuads;
	m_PreviousValuesPoint.clear();

	for(auto QuadIndex : vSelectedQuads)
	{
		m_PreviousValuesPoint[QuadIndex] = std::vector<std::map<EQuadPointProp, int>>(4);
	}
}

void CQuadEditTracker::AddQuadPointPropTrack(EQuadPointProp Prop)
{
	if(std::find(m_vTrackedProps.begin(), m_vTrackedProps.end(), Prop) != m_vTrackedProps.end())
		return;

	m_vTrackedProps.push_back(Prop);

	for(auto QuadIndex : m_vSelectedQuads)
	{
		auto &Quad = m_pLayer->m_vQuads[QuadIndex];
		if(Prop == EQuadPointProp::POS_X || Prop == EQuadPointProp::POS_Y)
			m_InitialPoints[QuadIndex] = std::vector<CPoint>(std::begin(Quad.m_aPoints), std::end(Quad.m_aPoints));
		else if(Prop == EQuadPointProp::COLOR)
		{
			for(int v = 0; v < 4; v++)
			{
				if(m_SelectedQuadPoints & (1 << v))
				{
					m_PreviousValuesPoint[QuadIndex][v][Prop] = PackColor(Quad.m_aColors[v]);
				}
			}
		}
		else if(Prop == EQuadPointProp::TEX_U)
		{
			for(int v = 0; v < 4; v++)
			{
				if(m_SelectedQuadPoints & (1 << v))
				{
					m_PreviousValuesPoint[QuadIndex][v][Prop] = Quad.m_aTexcoords[v].x;
				}
			}
		}
		else if(Prop == EQuadPointProp::TEX_V)
		{
			for(int v = 0; v < 4; v++)
			{
				if(m_SelectedQuadPoints & (1 << v))
				{
					m_PreviousValuesPoint[QuadIndex][v][Prop] = Quad.m_aTexcoords[v].y;
				}
			}
		}
	}
}

void CQuadEditTracker::EndQuadPointPropTrack(EQuadPointProp Prop)
{
	auto It = std::find(m_vTrackedProps.begin(), m_vTrackedProps.end(), Prop);
	if(It == m_vTrackedProps.end())
		return;

	m_vTrackedProps.erase(It);

	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	for(auto QuadIndex : m_vSelectedQuads)
	{
		auto &Quad = m_pLayer->m_vQuads[QuadIndex];
		if(Prop == EQuadPointProp::POS_X || Prop == EQuadPointProp::POS_Y)
		{
			auto vCurrentPoints = std::vector<CPoint>(std::begin(Quad.m_aPoints), std::end(Quad.m_aPoints));
			if(QuadPointChanged(vCurrentPoints, QuadIndex))
				vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, m_InitialPoints[QuadIndex], vCurrentPoints));
		}
		else
		{
			int Value = 0;
			for(int v = 0; v < 4; v++)
			{
				if(m_SelectedQuadPoints & (1 << v))
				{
					if(Prop == EQuadPointProp::COLOR)
					{
						Value = PackColor(Quad.m_aColors[v]);
					}
					else if(Prop == EQuadPointProp::TEX_U)
					{
						Value = Quad.m_aTexcoords[v].x;
					}
					else if(Prop == EQuadPointProp::TEX_V)
					{
						Value = Quad.m_aTexcoords[v].y;
					}

					if(Value != m_PreviousValuesPoint[QuadIndex][v][Prop])
						vpActions.push_back(std::make_shared<CEditorActionEditQuadPointProp>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, v, Prop, m_PreviousValuesPoint[QuadIndex][v][Prop], Value));
				}
			}
		}
	}

	if(!vpActions.empty())
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(Map(), vpActions));
}

void CQuadEditTracker::EndQuadPointPropTrackAll()
{
	std::vector<std::shared_ptr<IEditorAction>> vpActions;
	for(auto &Prop : m_vTrackedProps)
	{
		for(auto QuadIndex : m_vSelectedQuads)
		{
			auto &Quad = m_pLayer->m_vQuads[QuadIndex];
			if(Prop == EQuadPointProp::POS_X || Prop == EQuadPointProp::POS_Y)
			{
				auto vCurrentPoints = std::vector<CPoint>(std::begin(Quad.m_aPoints), std::end(Quad.m_aPoints));
				if(QuadPointChanged(vCurrentPoints, QuadIndex))
					vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, m_InitialPoints[QuadIndex], vCurrentPoints));
			}
			else
			{
				int Value = 0;
				for(int v = 0; v < 4; v++)
				{
					if(m_SelectedQuadPoints & (1 << v))
					{
						if(Prop == EQuadPointProp::COLOR)
						{
							Value = PackColor(Quad.m_aColors[v]);
						}
						else if(Prop == EQuadPointProp::TEX_U)
						{
							Value = Quad.m_aTexcoords[v].x;
						}
						else if(Prop == EQuadPointProp::TEX_V)
						{
							Value = Quad.m_aTexcoords[v].y;
						}

						if(Value != m_PreviousValuesPoint[QuadIndex][v][Prop])
							vpActions.push_back(std::make_shared<CEditorActionEditQuadPointProp>(Map(), m_GroupIndex, m_LayerIndex, QuadIndex, v, Prop, m_PreviousValuesPoint[QuadIndex][v][Prop], Value));
					}
				}
			}
		}
	}

	if(!vpActions.empty())
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(Map(), vpActions));

	m_vTrackedProps.clear();
}

// -----------------------------

void CEnvelopeEditorOperationTracker::Begin(EEnvelopeEditorOp Operation)
{
	if(m_TrackedOp == Operation)
		return;

	if(m_TrackedOp != EEnvelopeEditorOp::NONE)
	{
		Stop(true);
	}

	m_TrackedOp = Operation;

	if(Operation == EEnvelopeEditorOp::DRAG_POINT || Operation == EEnvelopeEditorOp::DRAG_POINT_X || Operation == EEnvelopeEditorOp::DRAG_POINT_Y || Operation == EEnvelopeEditorOp::SCALE)
	{
		HandlePointDragStart();
	}
}

void CEnvelopeEditorOperationTracker::Stop(bool Switch)
{
	if(m_TrackedOp == EEnvelopeEditorOp::NONE)
		return;

	if(m_TrackedOp == EEnvelopeEditorOp::DRAG_POINT || m_TrackedOp == EEnvelopeEditorOp::DRAG_POINT_X || m_TrackedOp == EEnvelopeEditorOp::DRAG_POINT_Y || m_TrackedOp == EEnvelopeEditorOp::SCALE)
	{
		HandlePointDragEnd(Switch);
	}

	m_TrackedOp = EEnvelopeEditorOp::NONE;
}

void CEnvelopeEditorOperationTracker::HandlePointDragStart()
{
	// Figure out which points are selected and which channels
	// Save their X and Y position (time and value)
	auto pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];

	for(auto [PointIndex, Channel] : Map()->m_vSelectedEnvelopePoints)
	{
		auto &Point = pEnvelope->m_vPoints[PointIndex];
		auto &Data = m_SavedValues[PointIndex];
		Data.m_Values[Channel] = Point.m_aValues[Channel];
		if(Data.m_Used)
			continue;
		Data.m_Time = Point.m_Time;
		Data.m_Used = true;
	}
}

void CEnvelopeEditorOperationTracker::HandlePointDragEnd(bool Switch)
{
	if(Switch && m_TrackedOp != EEnvelopeEditorOp::SCALE)
		return;

	int EnvelopeIndex = Map()->m_SelectedEnvelope;
	auto pEnvelope = Map()->m_vpEnvelopes[EnvelopeIndex];
	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	for(auto const &Entry : m_SavedValues)
	{
		int PointIndex = Entry.first;
		auto &Point = pEnvelope->m_vPoints[PointIndex];
		const auto &Data = Entry.second;

		if(Data.m_Time != Point.m_Time)
		{ // Save time
			vpActions.push_back(std::make_shared<CEditorActionEnvelopeEditPointTime>(Map(), EnvelopeIndex, PointIndex, Data.m_Time, Point.m_Time));
		}

		for(auto Value : Data.m_Values)
		{
			// Value.second is the saved value, Value.first is the channel
			int Channel = Value.first;
			if(Value.second != Point.m_aValues[Channel])
			{ // Save value
				vpActions.push_back(std::make_shared<CEditorActionEnvelopeEditPoint>(Map(), EnvelopeIndex, PointIndex, Channel, CEditorActionEnvelopeEditPoint::EEditType::VALUE, Value.second, Point.m_aValues[Channel]));
			}
		}
	}

	if(!vpActions.empty())
	{
		Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(Map(), vpActions, "Envelope point drag"));
	}

	m_SavedValues.clear();
}

// -----------------------------------------------------------------------
CSoundSourceOperationTracker::CSoundSourceOperationTracker(CEditorMap *pMap) :
	CMapObject(pMap),
	m_pSource(nullptr),
	m_TrackedOp(ESoundSourceOp::NONE),
	m_LayerIndex(-1)
{
}

void CSoundSourceOperationTracker::Begin(const CSoundSource *pSource, ESoundSourceOp Operation, int LayerIndex)
{
	if(m_TrackedOp == Operation || m_TrackedOp != ESoundSourceOp::NONE)
		return;

	m_TrackedOp = Operation;
	m_pSource = pSource;
	m_LayerIndex = LayerIndex;

	if(m_TrackedOp == ESoundSourceOp::MOVE)
	{
		m_Data.m_OriginalPoint = m_pSource->m_Position;
	}
}

void CSoundSourceOperationTracker::End()
{
	if(m_TrackedOp == ESoundSourceOp::NONE)
		return;

	if(m_TrackedOp == ESoundSourceOp::MOVE)
	{
		if(m_Data.m_OriginalPoint != m_pSource->m_Position)
		{
			Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionMoveSoundSource>(
				Map(), Map()->m_SelectedGroup, m_LayerIndex, Map()->m_SelectedSoundSource, m_Data.m_OriginalPoint, m_pSource->m_Position));
		}
	}

	m_TrackedOp = ESoundSourceOp::NONE;
}

// -----------------------------------------------------------------------

int CPropTrackerHelper::GetDefaultGroupIndex(CEditorMap *pMap)
{
	return pMap->m_SelectedGroup;
}

int CPropTrackerHelper::GetDefaultLayerIndex(CEditorMap *pMap)
{
	return pMap->m_vSelectedLayers[0];
}

// -----------------------------------------------------------------------

void CLayerPropTracker::OnEnd(ELayerProp Prop, int Value)
{
	if(Prop == ELayerProp::GROUP)
	{
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditLayersGroupAndOrder>(Map(), m_OriginalGroupIndex, std::vector<int>{m_OriginalLayerIndex}, m_CurrentGroupIndex, std::vector<int>{m_CurrentLayerIndex}));
	}
	else
	{
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditLayerProp>(Map(), m_CurrentGroupIndex, m_CurrentLayerIndex, Prop, m_OriginalValue, Value));
	}
}

int CLayerPropTracker::PropToValue(ELayerProp Prop)
{
	switch(Prop)
	{
	case ELayerProp::GROUP: return m_CurrentGroupIndex;
	case ELayerProp::HQ: return m_pObject->m_Flags;
	case ELayerProp::ORDER: return m_CurrentLayerIndex;
	default: return 0;
	}
}

// -----------------------------------------------------------------------

bool CLayerTilesPropTracker::EndChecker(ETilesProp Prop, int Value)
{
	return Value != m_OriginalValue || Prop == ETilesProp::SHIFT;
}

void CLayerTilesPropTracker::OnStart(ETilesProp Prop)
{
	if(Prop == ETilesProp::WIDTH || Prop == ETilesProp::HEIGHT)
	{
		m_SavedLayers[LAYERTYPE_TILES] = m_pObject->Duplicate();
		if(m_pObject->m_HasGame || m_pObject->m_HasFront || m_pObject->m_HasSwitch || m_pObject->m_HasSpeedup || m_pObject->m_HasTune || m_pObject->m_HasTele)
		{ // Need to save all entities layers when any entity layer
			if(Map()->m_pFrontLayer && !m_pObject->m_HasFront)
				m_SavedLayers[LAYERTYPE_FRONT] = Map()->m_pFrontLayer->Duplicate();
			if(Map()->m_pTeleLayer && !m_pObject->m_HasTele)
				m_SavedLayers[LAYERTYPE_TELE] = Map()->m_pTeleLayer->Duplicate();
			if(Map()->m_pSwitchLayer && !m_pObject->m_HasSwitch)
				m_SavedLayers[LAYERTYPE_SWITCH] = Map()->m_pSwitchLayer->Duplicate();
			if(Map()->m_pSpeedupLayer && !m_pObject->m_HasSpeedup)
				m_SavedLayers[LAYERTYPE_SPEEDUP] = Map()->m_pSpeedupLayer->Duplicate();
			if(Map()->m_pTuneLayer && !m_pObject->m_HasTune)
				m_SavedLayers[LAYERTYPE_TUNE] = Map()->m_pTuneLayer->Duplicate();
			if(!m_pObject->m_HasGame)
				m_SavedLayers[LAYERTYPE_GAME] = Map()->m_pGameLayer->Duplicate();
		}
	}
	else if(Prop == ETilesProp::SHIFT)
	{
		m_SavedLayers[LAYERTYPE_TILES] = m_pObject->Duplicate();
	}
}

void CLayerTilesPropTracker::OnEnd(ETilesProp Prop, int Value)
{
	auto pAction = std::make_shared<CEditorActionEditLayerTilesProp>(Map(), m_OriginalGroupIndex, m_OriginalLayerIndex, Prop, m_OriginalValue, Value);

	pAction->SetSavedLayers(m_SavedLayers);
	m_SavedLayers.clear();

	Map()->m_EditorHistory.RecordAction(pAction);
}

int CLayerTilesPropTracker::PropToValue(ETilesProp Prop)
{
	switch(Prop)
	{
	case ETilesProp::AUTOMAPPER: return m_pObject->m_AutoMapperConfig;
	case ETilesProp::LIVE_GAMETILES: return m_pObject->m_LiveGameTiles;
	case ETilesProp::COLOR: return PackColor(m_pObject->m_Color);
	case ETilesProp::COLOR_ENV: return m_pObject->m_ColorEnv;
	case ETilesProp::COLOR_ENV_OFFSET: return m_pObject->m_ColorEnvOffset;
	case ETilesProp::HEIGHT: return m_pObject->m_Height;
	case ETilesProp::WIDTH: return m_pObject->m_Width;
	case ETilesProp::IMAGE: return m_pObject->m_Image;
	case ETilesProp::SEED: return m_pObject->m_Seed;
	case ETilesProp::SHIFT_BY: return Map()->m_ShiftBy;
	default: return 0;
	}
}

// ------------------------------

void CLayerTilesCommonPropTracker::OnStart(ETilesCommonProp Prop)
{
	for(auto &pLayer : m_vpLayers)
	{
		if(Prop == ETilesCommonProp::SHIFT)
		{
			m_SavedLayers[pLayer][LAYERTYPE_TILES] = pLayer->Duplicate();
		}
	}
}

void CLayerTilesCommonPropTracker::OnEnd(ETilesCommonProp Prop, int Value)
{
	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	static std::map<ETilesCommonProp, ETilesProp> s_PropMap{
		{ETilesCommonProp::COLOR, ETilesProp::COLOR},
		{ETilesCommonProp::SHIFT, ETilesProp::SHIFT},
		{ETilesCommonProp::SHIFT_BY, ETilesProp::SHIFT_BY}};

	int j = 0;
	for(auto &pLayer : m_vpLayers)
	{
		int LayerIndex = m_vLayerIndices[j++];
		auto pAction = std::make_shared<CEditorActionEditLayerTilesProp>(Map(), m_OriginalGroupIndex, LayerIndex, s_PropMap[Prop], m_OriginalValue, Value);
		pAction->SetSavedLayers(m_SavedLayers[pLayer]);
		vpActions.push_back(pAction);
	}

	char aDisplay[256];
	static const char *s_apNames[] = {
		"width",
		"height",
		"shift",
		"shift by",
		"color",
	};

	str_format(aDisplay, sizeof(aDisplay), "Edit %d layers common property: %s", (int)m_vpLayers.size(), s_apNames[(int)Prop]);
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(Map(), vpActions, aDisplay));
}

bool CLayerTilesCommonPropTracker::EndChecker(ETilesCommonProp Prop, int Value)
{
	return Value != m_OriginalValue || Prop == ETilesCommonProp::SHIFT;
}

int CLayerTilesCommonPropTracker::PropToValue(ETilesCommonProp Prop)
{
	if(Prop == ETilesCommonProp::SHIFT_BY)
		return Map()->m_ShiftBy;
	return 0;
}

// ------------------------------

void CLayerGroupPropTracker::OnEnd(EGroupProp Prop, int Value)
{
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditGroupProp>(Map(), Map()->m_SelectedGroup, Prop, m_OriginalValue, Value));
}

int CLayerGroupPropTracker::PropToValue(EGroupProp Prop)
{
	switch(Prop)
	{
	case EGroupProp::ORDER: return Map()->m_SelectedGroup;
	case EGroupProp::POS_X: return m_pObject->m_OffsetX;
	case EGroupProp::POS_Y: return m_pObject->m_OffsetY;
	case EGroupProp::PARA_X: return m_pObject->m_ParallaxX;
	case EGroupProp::PARA_Y: return m_pObject->m_ParallaxY;
	case EGroupProp::USE_CLIPPING: return m_pObject->m_UseClipping;
	case EGroupProp::CLIP_X: return m_pObject->m_ClipX;
	case EGroupProp::CLIP_Y: return m_pObject->m_ClipY;
	case EGroupProp::CLIP_W: return m_pObject->m_ClipW;
	case EGroupProp::CLIP_H: return m_pObject->m_ClipH;
	default: return 0;
	}
}

// ------------------------------------------------------------------

void CLayerQuadsPropTracker::OnEnd(ELayerQuadsProp Prop, int Value)
{
	auto pAction = std::make_shared<CEditorActionEditLayerQuadsProp>(Map(), m_OriginalGroupIndex, m_OriginalLayerIndex, Prop, m_OriginalValue, Value);
	Map()->m_EditorHistory.RecordAction(pAction);
}

int CLayerQuadsPropTracker::PropToValue(ELayerQuadsProp Prop)
{
	if(Prop == ELayerQuadsProp::IMAGE)
		return m_pObject->m_Image;
	return 0;
}

// -------------------------------------------------------------------

void CLayerSoundsPropTracker::OnEnd(ELayerSoundsProp Prop, int Value)
{
	auto pAction = std::make_shared<CEditorActionEditLayerSoundsProp>(Map(), m_OriginalGroupIndex, m_OriginalLayerIndex, Prop, m_OriginalValue, Value);
	Map()->m_EditorHistory.RecordAction(pAction);
}

int CLayerSoundsPropTracker::PropToValue(ELayerSoundsProp Prop)
{
	if(Prop == ELayerSoundsProp::SOUND)
		return m_pObject->m_Sound;
	return 0;
}

// ----

void CSoundSourcePropTracker::OnEnd(ESoundProp Prop, int Value)
{
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditSoundSourceProp>(Map(), m_OriginalGroupIndex, m_OriginalLayerIndex, Map()->m_SelectedSoundSource, Prop, m_OriginalValue, Value));
}

int CSoundSourcePropTracker::PropToValue(ESoundProp Prop)
{
	switch(Prop)
	{
	case ESoundProp::POS_X: return m_pObject->m_Position.x;
	case ESoundProp::POS_Y: return m_pObject->m_Position.y;
	case ESoundProp::LOOP: return m_pObject->m_Loop;
	case ESoundProp::PAN: return m_pObject->m_Pan;
	case ESoundProp::TIME_DELAY: return m_pObject->m_TimeDelay;
	case ESoundProp::FALLOFF: return m_pObject->m_Falloff;
	case ESoundProp::POS_ENV: return m_pObject->m_PosEnv;
	case ESoundProp::POS_ENV_OFFSET: return m_pObject->m_PosEnvOffset;
	case ESoundProp::SOUND_ENV: return m_pObject->m_SoundEnv;
	case ESoundProp::SOUND_ENV_OFFSET: return m_pObject->m_SoundEnvOffset;
	default: return 0;
	}
}

// ----

void CSoundSourceRectShapePropTracker::OnEnd(ERectangleShapeProp Prop, int Value)
{
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditRectSoundSourceShapeProp>(Map(), m_OriginalGroupIndex, m_OriginalLayerIndex, Map()->m_SelectedSoundSource, Prop, m_OriginalValue, Value));
}

int CSoundSourceRectShapePropTracker::PropToValue(ERectangleShapeProp Prop)
{
	switch(Prop)
	{
	case ERectangleShapeProp::RECTANGLE_WIDTH: return m_pObject->m_Shape.m_Rectangle.m_Width;
	case ERectangleShapeProp::RECTANGLE_HEIGHT: return m_pObject->m_Shape.m_Rectangle.m_Height;
	default: return 0;
	}
}

void CSoundSourceCircleShapePropTracker::OnEnd(ECircleShapeProp Prop, int Value)
{
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditCircleSoundSourceShapeProp>(Map(), m_OriginalGroupIndex, m_OriginalLayerIndex, Map()->m_SelectedSoundSource, Prop, m_OriginalValue, Value));
}

int CSoundSourceCircleShapePropTracker::PropToValue(ECircleShapeProp Prop)
{
	switch(Prop)
	{
	case ECircleShapeProp::CIRCLE_RADIUS: return m_pObject->m_Shape.m_Circle.m_Radius;
	default: return 0;
	}
}
