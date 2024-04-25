#include "editor_trackers.h"

#include <game/editor/mapitems/layer_group.h>
#include <game/editor/mapitems/layer_tiles.h>

#include "editor.h"
#include "editor_actions.h"

CQuadEditTracker::CQuadEditTracker() :
	m_pEditor(nullptr), m_TrackedProp(EQuadProp::PROP_NONE) {}

CQuadEditTracker::~CQuadEditTracker()
{
	m_InitalPoints.clear();
	m_vSelectedQuads.clear();
}

void CQuadEditTracker::BeginQuadTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int GroupIndex, int LayerIndex)
{
	if(m_Tracking)
		return;
	m_Tracking = true;
	m_vSelectedQuads.clear();
	m_pLayer = pLayer;
	m_GroupIndex = GroupIndex < 0 ? m_pEditor->m_SelectedGroup : GroupIndex;
	m_LayerIndex = LayerIndex < 0 ? m_pEditor->m_vSelectedLayers[0] : LayerIndex;
	// Init all points
	for(auto QuadIndex : vSelectedQuads)
	{
		auto &pQuad = pLayer->m_vQuads[QuadIndex];
		m_InitalPoints[QuadIndex] = std::vector<CPoint>(pQuad.m_aPoints, pQuad.m_aPoints + 5);
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
		auto vCurrentPoints = std::vector<CPoint>(pQuad.m_aPoints, pQuad.m_aPoints + 5);
		vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(m_pEditor, m_GroupIndex, m_LayerIndex, QuadIndex, m_InitalPoints[QuadIndex], vCurrentPoints));
	}
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions));
}

void CQuadEditTracker::BeginQuadPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, EQuadProp Prop, int GroupIndex, int LayerIndex)
{
	if(m_TrackedProp != EQuadProp::PROP_NONE)
		return;
	m_TrackedProp = Prop;
	m_pLayer = pLayer;
	m_GroupIndex = GroupIndex < 0 ? m_pEditor->m_SelectedGroup : GroupIndex;
	m_LayerIndex = LayerIndex < 0 ? m_pEditor->m_vSelectedLayers[0] : LayerIndex;
	m_vSelectedQuads = vSelectedQuads;
	m_PreviousValues.clear();

	for(auto QuadIndex : vSelectedQuads)
	{
		auto &Quad = pLayer->m_vQuads[QuadIndex];
		if(Prop == EQuadProp::PROP_POS_X || Prop == EQuadProp::PROP_POS_Y)
			m_InitalPoints[QuadIndex] = std::vector<CPoint>(Quad.m_aPoints, Quad.m_aPoints + 5);
		else if(Prop == EQuadProp::PROP_POS_ENV)
			m_PreviousValues[QuadIndex] = Quad.m_PosEnv;
		else if(Prop == EQuadProp::PROP_POS_ENV_OFFSET)
			m_PreviousValues[QuadIndex] = Quad.m_PosEnvOffset;
		else if(Prop == EQuadProp::PROP_COLOR_ENV)
			m_PreviousValues[QuadIndex] = Quad.m_ColorEnv;
		else if(Prop == EQuadProp::PROP_COLOR_ENV_OFFSET)
			m_PreviousValues[QuadIndex] = Quad.m_ColorEnvOffset;
	}
}
void CQuadEditTracker::EndQuadPropTrack(EQuadProp Prop)
{
	if(m_TrackedProp != Prop)
		return;
	m_TrackedProp = EQuadProp::PROP_NONE;

	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	for(auto QuadIndex : m_vSelectedQuads)
	{
		auto &Quad = m_pLayer->m_vQuads[QuadIndex];
		if(Prop == EQuadProp::PROP_POS_X || Prop == EQuadProp::PROP_POS_Y)
		{
			auto vCurrentPoints = std::vector<CPoint>(Quad.m_aPoints, Quad.m_aPoints + 5);
			vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(m_pEditor, m_GroupIndex, m_LayerIndex, QuadIndex, m_InitalPoints[QuadIndex], vCurrentPoints));
		}
		else
		{
			int Value = 0;
			if(Prop == EQuadProp::PROP_POS_ENV)
				Value = Quad.m_PosEnv;
			else if(Prop == EQuadProp::PROP_POS_ENV_OFFSET)
				Value = Quad.m_PosEnvOffset;
			else if(Prop == EQuadProp::PROP_COLOR_ENV)
				Value = Quad.m_ColorEnv;
			else if(Prop == EQuadProp::PROP_COLOR_ENV_OFFSET)
				Value = Quad.m_ColorEnvOffset;

			if(Value != m_PreviousValues[QuadIndex])
				vpActions.push_back(std::make_shared<CEditorActionEditQuadProp>(m_pEditor, m_GroupIndex, m_LayerIndex, QuadIndex, Prop, m_PreviousValues[QuadIndex], Value));
		}
	}

	if(!vpActions.empty())
		m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions));
}

void CQuadEditTracker::BeginQuadPointPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int SelectedQuadPoints, int GroupIndex, int LayerIndex)
{
	if(!m_vTrackedProps.empty())
		return;

	m_pLayer = pLayer;
	m_GroupIndex = GroupIndex < 0 ? m_pEditor->m_SelectedGroup : GroupIndex;
	m_LayerIndex = LayerIndex < 0 ? m_pEditor->m_vSelectedLayers[0] : LayerIndex;
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
		if(Prop == EQuadPointProp::PROP_POS_X || Prop == EQuadPointProp::PROP_POS_Y)
			m_InitalPoints[QuadIndex] = std::vector<CPoint>(Quad.m_aPoints, Quad.m_aPoints + 5);
		else if(Prop == EQuadPointProp::PROP_COLOR)
		{
			for(int v = 0; v < 4; v++)
			{
				if(m_SelectedQuadPoints & (1 << v))
				{
					int Color = PackColor(Quad.m_aColors[v]);
					m_PreviousValuesPoint[QuadIndex][v][Prop] = Color;
				}
			}
		}
		else if(Prop == EQuadPointProp::PROP_TEX_U)
		{
			for(int v = 0; v < 4; v++)
			{
				if(m_SelectedQuadPoints & (1 << v))
				{
					m_PreviousValuesPoint[QuadIndex][v][Prop] = Quad.m_aTexcoords[v].x;
				}
			}
		}
		else if(Prop == EQuadPointProp::PROP_TEX_V)
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
		if(Prop == EQuadPointProp::PROP_POS_X || Prop == EQuadPointProp::PROP_POS_Y)
		{
			auto vCurrentPoints = std::vector<CPoint>(Quad.m_aPoints, Quad.m_aPoints + 5);
			vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(m_pEditor, m_GroupIndex, m_LayerIndex, QuadIndex, m_InitalPoints[QuadIndex], vCurrentPoints));
		}
		else
		{
			int Value = 0;
			for(int v = 0; v < 4; v++)
			{
				if(m_SelectedQuadPoints & (1 << v))
				{
					if(Prop == EQuadPointProp::PROP_COLOR)
					{
						Value = PackColor(Quad.m_aColors[v]);
					}
					else if(Prop == EQuadPointProp::PROP_TEX_U)
					{
						Value = Quad.m_aTexcoords[v].x;
					}
					else if(Prop == EQuadPointProp::PROP_TEX_V)
					{
						Value = Quad.m_aTexcoords[v].y;
					}

					if(Value != m_PreviousValuesPoint[QuadIndex][v][Prop])
						vpActions.push_back(std::make_shared<CEditorActionEditQuadPointProp>(m_pEditor, m_GroupIndex, m_LayerIndex, QuadIndex, v, Prop, m_PreviousValuesPoint[QuadIndex][v][Prop], Value));
				}
			}
		}
	}

	if(!vpActions.empty())
		m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions));
}

void CQuadEditTracker::EndQuadPointPropTrackAll()
{
	std::vector<std::shared_ptr<IEditorAction>> vpActions;
	for(auto &Prop : m_vTrackedProps)
	{
		for(auto QuadIndex : m_vSelectedQuads)
		{
			auto &Quad = m_pLayer->m_vQuads[QuadIndex];
			if(Prop == EQuadPointProp::PROP_POS_X || Prop == EQuadPointProp::PROP_POS_Y)
			{
				auto vCurrentPoints = std::vector<CPoint>(Quad.m_aPoints, Quad.m_aPoints + 5);
				vpActions.push_back(std::make_shared<CEditorActionEditQuadPoint>(m_pEditor, m_GroupIndex, m_LayerIndex, QuadIndex, m_InitalPoints[QuadIndex], vCurrentPoints));
			}
			else
			{
				int Value = 0;
				for(int v = 0; v < 4; v++)
				{
					if(m_SelectedQuadPoints & (1 << v))
					{
						if(Prop == EQuadPointProp::PROP_COLOR)
						{
							Value = PackColor(Quad.m_aColors[v]);
						}
						else if(Prop == EQuadPointProp::PROP_TEX_U)
						{
							Value = Quad.m_aTexcoords[v].x;
						}
						else if(Prop == EQuadPointProp::PROP_TEX_V)
						{
							Value = Quad.m_aTexcoords[v].y;
						}

						if(Value != m_PreviousValuesPoint[QuadIndex][v][Prop])
							vpActions.push_back(std::make_shared<CEditorActionEditQuadPointProp>(m_pEditor, m_GroupIndex, m_LayerIndex, QuadIndex, v, Prop, m_PreviousValuesPoint[QuadIndex][v][Prop], Value));
					}
				}
			}
		}
	}

	if(!vpActions.empty())
		m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions));

	m_vTrackedProps.clear();
}

// -----------------------------

void CEnvelopeEditorOperationTracker::Begin(EEnvelopeEditorOp Operation)
{
	if(m_TrackedOp == Operation)
		return;

	if(m_TrackedOp != EEnvelopeEditorOp::OP_NONE)
	{
		Stop(true);
	}

	m_TrackedOp = Operation;

	if(Operation == EEnvelopeEditorOp::OP_DRAG_POINT || Operation == EEnvelopeEditorOp::OP_DRAG_POINT_X || Operation == EEnvelopeEditorOp::OP_DRAG_POINT_Y || Operation == EEnvelopeEditorOp::OP_SCALE)
	{
		HandlePointDragStart();
	}
}

void CEnvelopeEditorOperationTracker::Stop(bool Switch)
{
	if(m_TrackedOp == EEnvelopeEditorOp::OP_NONE)
		return;

	if(m_TrackedOp == EEnvelopeEditorOp::OP_DRAG_POINT || m_TrackedOp == EEnvelopeEditorOp::OP_DRAG_POINT_X || m_TrackedOp == EEnvelopeEditorOp::OP_DRAG_POINT_Y || m_TrackedOp == EEnvelopeEditorOp::OP_SCALE)
	{
		HandlePointDragEnd(Switch);
	}

	m_TrackedOp = EEnvelopeEditorOp::OP_NONE;
}

void CEnvelopeEditorOperationTracker::HandlePointDragStart()
{
	// Figure out which points are selected and which channels
	// Save their X and Y position (time and value)
	auto pEnv = m_pEditor->m_Map.m_vpEnvelopes[m_pEditor->m_SelectedEnvelope];

	for(auto [PointIndex, Channel] : m_pEditor->m_vSelectedEnvelopePoints)
	{
		auto &Point = pEnv->m_vPoints[PointIndex];
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
	if(Switch && m_TrackedOp != EEnvelopeEditorOp::OP_SCALE)
		return;

	int EnvIndex = m_pEditor->m_SelectedEnvelope;
	auto pEnv = m_pEditor->m_Map.m_vpEnvelopes[EnvIndex];
	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	for(auto const &Entry : m_SavedValues)
	{
		int PointIndex = Entry.first;
		auto &Point = pEnv->m_vPoints[PointIndex];
		const auto &Data = Entry.second;

		if(Data.m_Time != Point.m_Time)
		{ // Save time
			vpActions.push_back(std::make_shared<CEditorActionEnvelopeEditPoint>(m_pEditor, EnvIndex, PointIndex, 0, CEditorActionEnvelopeEditPoint::EEditType::TIME, Data.m_Time, Point.m_Time));
		}

		for(auto Value : Data.m_Values)
		{
			// Value.second is the saved value, Value.first is the channel
			int Channel = Value.first;
			if(Value.second != Point.m_aValues[Channel])
			{ // Save value
				vpActions.push_back(std::make_shared<CEditorActionEnvelopeEditPoint>(m_pEditor, EnvIndex, PointIndex, Channel, CEditorActionEnvelopeEditPoint::EEditType::VALUE, Value.second, Point.m_aValues[Channel]));
			}
		}
	}

	if(!vpActions.empty())
	{
		m_pEditor->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions, "Envelope point drag"));
	}

	m_SavedValues.clear();
}

// -----------------------------------------------------------------------
CSoundSourceOperationTracker::CSoundSourceOperationTracker(CEditor *pEditor) :
	m_pEditor(pEditor), m_pSource(nullptr), m_TrackedOp(ESoundSourceOp::OP_NONE), m_LayerIndex(-1)
{
}

void CSoundSourceOperationTracker::Begin(CSoundSource *pSource, ESoundSourceOp Operation, int LayerIndex)
{
	if(m_TrackedOp == Operation || m_TrackedOp != ESoundSourceOp::OP_NONE)
		return;

	m_TrackedOp = Operation;
	m_pSource = pSource;
	m_LayerIndex = LayerIndex;

	if(m_TrackedOp == ESoundSourceOp::OP_MOVE)
		HandlePointMove(EState::STATE_BEGIN);
}

void CSoundSourceOperationTracker::End()
{
	if(m_TrackedOp == ESoundSourceOp::OP_NONE)
		return;

	if(m_TrackedOp == ESoundSourceOp::OP_MOVE)
		HandlePointMove(EState::STATE_END);

	m_TrackedOp = ESoundSourceOp::OP_NONE;
}

void CSoundSourceOperationTracker::HandlePointMove(EState State)
{
	if(State == EState::STATE_BEGIN)
	{
		m_Data.m_OriginalPoint = m_pSource->m_Position;
	}
	else if(State == EState::STATE_END)
	{
		m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionMoveSoundSource>(m_pEditor, m_pEditor->m_SelectedGroup, m_LayerIndex, m_pEditor->m_SelectedSource, m_Data.m_OriginalPoint, m_pSource->m_Position));
	}
}

// -----------------------------------------------------------------------

int SPropTrackerHelper::GetDefaultGroupIndex(CEditor *pEditor)
{
	return pEditor->m_SelectedGroup;
}

int SPropTrackerHelper::GetDefaultLayerIndex(CEditor *pEditor)
{
	return pEditor->m_vSelectedLayers[0];
}

// -----------------------------------------------------------------------

void CLayerPropTracker::OnEnd(ELayerProp Prop, int Value)
{
	if(Prop == ELayerProp::PROP_GROUP)
	{
		m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditLayersGroupAndOrder>(m_pEditor, m_OriginalGroupIndex, std::vector<int>{m_OriginalLayerIndex}, m_CurrentGroupIndex, std::vector<int>{m_CurrentLayerIndex}));
	}
	else
	{
		m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditLayerProp>(m_pEditor, m_CurrentGroupIndex, m_CurrentLayerIndex, Prop, m_OriginalValue, Value));
	}
}

int CLayerPropTracker::PropToValue(ELayerProp Prop)
{
	switch(Prop)
	{
	case ELayerProp::PROP_GROUP: return m_CurrentGroupIndex;
	case ELayerProp::PROP_HQ: return m_pObject->m_Flags;
	case ELayerProp::PROP_ORDER: return m_CurrentLayerIndex;
	default: return 0;
	}
}

// -----------------------------------------------------------------------

bool CLayerTilesPropTracker::EndChecker(ETilesProp Prop, EEditState State, int Value)
{
	return (State == EEditState::END || State == EEditState::ONE_GO) && (Value != m_OriginalValue || Prop == ETilesProp::PROP_SHIFT);
}

void CLayerTilesPropTracker::OnStart(ETilesProp Prop)
{
	if(Prop == ETilesProp::PROP_WIDTH || Prop == ETilesProp::PROP_HEIGHT)
	{
		m_SavedLayers[LAYERTYPE_TILES] = m_pObject->Duplicate();
		if(m_pObject->m_Game || m_pObject->m_Front || m_pObject->m_Switch || m_pObject->m_Speedup || m_pObject->m_Tune || m_pObject->m_Tele)
		{ // Need to save all entities layers when any entity layer
			if(m_pEditor->m_Map.m_pFrontLayer && !m_pObject->m_Front)
				m_SavedLayers[LAYERTYPE_FRONT] = m_pEditor->m_Map.m_pFrontLayer->Duplicate();
			if(m_pEditor->m_Map.m_pTeleLayer && !m_pObject->m_Tele)
				m_SavedLayers[LAYERTYPE_TELE] = m_pEditor->m_Map.m_pTeleLayer->Duplicate();
			if(m_pEditor->m_Map.m_pSwitchLayer && !m_pObject->m_Switch)
				m_SavedLayers[LAYERTYPE_SWITCH] = m_pEditor->m_Map.m_pSwitchLayer->Duplicate();
			if(m_pEditor->m_Map.m_pSpeedupLayer && !m_pObject->m_Speedup)
				m_SavedLayers[LAYERTYPE_SPEEDUP] = m_pEditor->m_Map.m_pSpeedupLayer->Duplicate();
			if(m_pEditor->m_Map.m_pTuneLayer && !m_pObject->m_Tune)
				m_SavedLayers[LAYERTYPE_TUNE] = m_pEditor->m_Map.m_pTuneLayer->Duplicate();
			if(!m_pObject->m_Game)
				m_SavedLayers[LAYERTYPE_GAME] = m_pEditor->m_Map.m_pGameLayer->Duplicate();
		}
	}
	else if(Prop == ETilesProp::PROP_SHIFT)
	{
		m_SavedLayers[LAYERTYPE_TILES] = m_pObject->Duplicate();
	}
}

void CLayerTilesPropTracker::OnEnd(ETilesProp Prop, int Value)
{
	auto pAction = std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_OriginalGroupIndex, m_OriginalLayerIndex, Prop, m_OriginalValue, Value);

	pAction->SetSavedLayers(m_SavedLayers);
	m_SavedLayers.clear();

	m_pEditor->m_EditorHistory.RecordAction(pAction);
}

int CLayerTilesPropTracker::PropToValue(ETilesProp Prop)
{
	switch(Prop)
	{
	case ETilesProp::PROP_AUTOMAPPER: return m_pObject->m_AutoMapperConfig;
	case ETilesProp::PROP_COLOR: return PackColor(m_pObject->m_Color);
	case ETilesProp::PROP_COLOR_ENV: return m_pObject->m_ColorEnv;
	case ETilesProp::PROP_COLOR_ENV_OFFSET: return m_pObject->m_ColorEnvOffset;
	case ETilesProp::PROP_HEIGHT: return m_pObject->m_Height;
	case ETilesProp::PROP_WIDTH: return m_pObject->m_Width;
	case ETilesProp::PROP_IMAGE: return m_pObject->m_Image;
	case ETilesProp::PROP_SEED: return m_pObject->m_Seed;
	case ETilesProp::PROP_SHIFT_BY: return m_pEditor->m_ShiftBy;
	default: return 0;
	}
}

// ------------------------------

void CLayerTilesCommonPropTracker::OnStart(ETilesCommonProp Prop)
{
	for(auto &pLayer : m_vpLayers)
	{
		if(Prop == ETilesCommonProp::PROP_SHIFT)
		{
			m_SavedLayers[pLayer][LAYERTYPE_TILES] = pLayer->Duplicate();
		}
	}
}

void CLayerTilesCommonPropTracker::OnEnd(ETilesCommonProp Prop, int Value)
{
	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	static std::map<ETilesCommonProp, ETilesProp> s_PropMap{
		{ETilesCommonProp::PROP_COLOR, ETilesProp::PROP_COLOR},
		{ETilesCommonProp::PROP_SHIFT, ETilesProp::PROP_SHIFT},
		{ETilesCommonProp::PROP_SHIFT_BY, ETilesProp::PROP_SHIFT_BY}};

	int j = 0;
	for(auto &pLayer : m_vpLayers)
	{
		int LayerIndex = m_vLayerIndices[j++];
		auto pAction = std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_OriginalGroupIndex, LayerIndex, s_PropMap[Prop], m_OriginalValue, Value);
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
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions, aDisplay));
}

bool CLayerTilesCommonPropTracker::EndChecker(ETilesCommonProp Prop, EEditState State, int Value)
{
	return (State == EEditState::END || State == EEditState::ONE_GO) && (Value != m_OriginalValue || Prop == ETilesCommonProp::PROP_SHIFT);
}

int CLayerTilesCommonPropTracker::PropToValue(ETilesCommonProp Prop)
{
	if(Prop == ETilesCommonProp::PROP_SHIFT_BY)
		return m_pEditor->m_ShiftBy;
	return 0;
}

// ------------------------------

void CLayerGroupPropTracker::OnEnd(EGroupProp Prop, int Value)
{
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditGroupProp>(m_pEditor, m_pEditor->m_SelectedGroup, Prop, m_OriginalValue, Value));
}

int CLayerGroupPropTracker::PropToValue(EGroupProp Prop)
{
	switch(Prop)
	{
	case EGroupProp::PROP_ORDER: return m_pEditor->m_SelectedGroup;
	case EGroupProp::PROP_POS_X: return m_pObject->m_OffsetX;
	case EGroupProp::PROP_POS_Y: return m_pObject->m_OffsetY;
	case EGroupProp::PROP_PARA_X: return m_pObject->m_ParallaxX;
	case EGroupProp::PROP_PARA_Y: return m_pObject->m_ParallaxY;
	case EGroupProp::PROP_USE_CLIPPING: return m_pObject->m_UseClipping;
	case EGroupProp::PROP_CLIP_X: return m_pObject->m_ClipX;
	case EGroupProp::PROP_CLIP_Y: return m_pObject->m_ClipY;
	case EGroupProp::PROP_CLIP_W: return m_pObject->m_ClipW;
	case EGroupProp::PROP_CLIP_H: return m_pObject->m_ClipH;
	default: return 0;
	}
}

// ------------------------------------------------------------------

void CLayerQuadsPropTracker::OnEnd(ELayerQuadsProp Prop, int Value)
{
	auto pAction = std::make_shared<CEditorActionEditLayerQuadsProp>(m_pEditor, m_OriginalGroupIndex, m_OriginalLayerIndex, Prop, m_OriginalValue, Value);
	m_pEditor->m_EditorHistory.RecordAction(pAction);
}

int CLayerQuadsPropTracker::PropToValue(ELayerQuadsProp Prop)
{
	if(Prop == ELayerQuadsProp::PROP_IMAGE)
		return m_pObject->m_Image;
	return 0;
}

// -------------------------------------------------------------------

void CLayerSoundsPropTracker::OnEnd(ELayerSoundsProp Prop, int Value)
{
	auto pAction = std::make_shared<CEditorActionEditLayerSoundsProp>(m_pEditor, m_OriginalGroupIndex, m_OriginalLayerIndex, Prop, m_OriginalValue, Value);
	m_pEditor->m_EditorHistory.RecordAction(pAction);
}

int CLayerSoundsPropTracker::PropToValue(ELayerSoundsProp Prop)
{
	if(Prop == ELayerSoundsProp::PROP_SOUND)
		return m_pObject->m_Sound;
	return 0;
}

// ----

void CSoundSourcePropTracker::OnEnd(ESoundProp Prop, int Value)
{
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditSoundSourceProp>(m_pEditor, m_OriginalGroupIndex, m_OriginalLayerIndex, m_pEditor->m_SelectedSource, Prop, m_OriginalValue, Value));
}

int CSoundSourcePropTracker::PropToValue(ESoundProp Prop)
{
	switch(Prop)
	{
	case ESoundProp::PROP_POS_X: return m_pObject->m_Position.x;
	case ESoundProp::PROP_POS_Y: return m_pObject->m_Position.y;
	case ESoundProp::PROP_LOOP: return m_pObject->m_Loop;
	case ESoundProp::PROP_PAN: return m_pObject->m_Pan;
	case ESoundProp::PROP_TIME_DELAY: return m_pObject->m_TimeDelay;
	case ESoundProp::PROP_FALLOFF: return m_pObject->m_Falloff;
	case ESoundProp::PROP_POS_ENV: return m_pObject->m_PosEnv;
	case ESoundProp::PROP_POS_ENV_OFFSET: return m_pObject->m_PosEnvOffset;
	case ESoundProp::PROP_SOUND_ENV: return m_pObject->m_SoundEnv;
	case ESoundProp::PROP_SOUND_ENV_OFFSET: return m_pObject->m_SoundEnvOffset;
	default: return 0;
	}
}

// ----

void CSoundSourceRectShapePropTracker::OnEnd(ERectangleShapeProp Prop, int Value)
{
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditRectSoundSourceShapeProp>(m_pEditor, m_OriginalGroupIndex, m_OriginalLayerIndex, m_pEditor->m_SelectedSource, Prop, m_OriginalValue, Value));
}

int CSoundSourceRectShapePropTracker::PropToValue(ERectangleShapeProp Prop)
{
	switch(Prop)
	{
	case ERectangleShapeProp::PROP_RECTANGLE_WIDTH: return m_pObject->m_Shape.m_Rectangle.m_Width;
	case ERectangleShapeProp::PROP_RECTANGLE_HEIGHT: return m_pObject->m_Shape.m_Rectangle.m_Height;
	default: return 0;
	}
}

void CSoundSourceCircleShapePropTracker::OnEnd(ECircleShapeProp Prop, int Value)
{
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditCircleSoundSourceShapeProp>(m_pEditor, m_OriginalGroupIndex, m_OriginalLayerIndex, m_pEditor->m_SelectedSource, Prop, m_OriginalValue, Value));
}

int CSoundSourceCircleShapePropTracker::PropToValue(ECircleShapeProp Prop)
{
	switch(Prop)
	{
	case ECircleShapeProp::PROP_CIRCLE_RADIUS: return m_pObject->m_Shape.m_Circle.m_Radius;
	default: return 0;
	}
}
