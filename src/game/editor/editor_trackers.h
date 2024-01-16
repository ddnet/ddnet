#ifndef GAME_EDITOR_EDITOR_TRACKERS_H
#define GAME_EDITOR_EDITOR_TRACKERS_H

#include <game/editor/mapitems.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/mapitems.h>

#include <map>
#include <memory>
#include <vector>

class CEditor;
class CLayerTiles;
class CLayerGroup;
class CLayerSounds;
struct CSoundSource;

class CQuadEditTracker
{
public:
	CQuadEditTracker();
	~CQuadEditTracker();

	void BeginQuadTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int GroupIndex = -1, int LayerIndex = -1);
	void EndQuadTrack();

	void BeginQuadPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, EQuadProp Prop, int GroupIndex = -1, int LayerIndex = -1);
	void EndQuadPropTrack(EQuadProp Prop);

	void BeginQuadPointPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int SelectedQuadPoints, int GroupIndex = -1, int LayerIndex = -1);
	void AddQuadPointPropTrack(EQuadPointProp Prop);
	void EndQuadPointPropTrack(EQuadPointProp Prop);
	void EndQuadPointPropTrackAll();

	CEditor *m_pEditor;

private:
	std::vector<int> m_vSelectedQuads;
	int m_SelectedQuadPoints;
	std::map<int, std::vector<CPoint>> m_InitalPoints;

	bool m_Tracking = false;
	std::shared_ptr<CLayerQuads> m_pLayer;

	EQuadProp m_TrackedProp;
	std::vector<EQuadPointProp> m_vTrackedProps;
	std::map<int, int> m_PreviousValues;
	std::map<int, std::vector<std::map<EQuadPointProp, int>>> m_PreviousValuesPoint;
	int m_LayerIndex;
	int m_GroupIndex;
};

enum class EEnvelopeEditorOp
{
	OP_NONE = 0,
	OP_SELECT,
	OP_DRAG_POINT,
	OP_DRAG_POINT_X,
	OP_DRAG_POINT_Y,
	OP_CONTEXT_MENU,
	OP_BOX_SELECT,
	OP_SCALE
};

enum class ESoundSourceOp
{
	OP_NONE = 0,
	OP_MOVE,
	OP_CONTEXT_MENU,
};

class CEnvelopeEditorOperationTracker
{
public:
	CEnvelopeEditorOperationTracker() = default;

	void Begin(EEnvelopeEditorOp Operation);
	void Stop(bool Switch = true);
	inline void Reset() { m_TrackedOp = EEnvelopeEditorOp::OP_NONE; }

	CEditor *m_pEditor;

private:
	EEnvelopeEditorOp m_TrackedOp = EEnvelopeEditorOp::OP_NONE;

	struct SPointData
	{
		bool m_Used;
		int m_Time;
		std::map<int, int> m_Values;
	};

	std::map<int, SPointData> m_SavedValues;

	void HandlePointDragStart();
	void HandlePointDragEnd(bool Switch);
};

class CSoundSourceOperationTracker
{
public:
	CSoundSourceOperationTracker(CEditor *pEditor);

	void Begin(CSoundSource *pSource, ESoundSourceOp Operation, int LayerIndex);
	void End();

private:
	CEditor *m_pEditor;
	CSoundSource *m_pSource;
	ESoundSourceOp m_TrackedOp;
	int m_LayerIndex;

	struct SData
	{
		CPoint m_OriginalPoint;
	};
	SData m_Data;

	enum EState
	{
		STATE_BEGIN,
		STATE_EDITING,
		STATE_END
	};
	void HandlePointMove(EState State);
};

struct SPropTrackerHelper
{
	static int GetDefaultGroupIndex(CEditor *pEditor);
	static int GetDefaultLayerIndex(CEditor *pEditor);
};

template<typename T, typename E>
class CPropTracker
{
public:
	CPropTracker(CEditor *pEditor) :
		m_pEditor(pEditor), m_OriginalValue(0), m_pObject(nullptr), m_OriginalLayerIndex(-1), m_OriginalGroupIndex(-1), m_CurrentLayerIndex(-1), m_CurrentGroupIndex(-1), m_Tracking(false) {}
	CEditor *m_pEditor;

	void Begin(T *pObject, E Prop, EEditState State, int GroupIndex = -1, int LayerIndex = -1)
	{
		if(m_Tracking || Prop == static_cast<E>(-1))
			return;
		m_pObject = pObject;

		m_OriginalGroupIndex = GroupIndex < 0 ? SPropTrackerHelper::GetDefaultGroupIndex(m_pEditor) : GroupIndex;
		m_OriginalLayerIndex = LayerIndex < 0 ? SPropTrackerHelper::GetDefaultLayerIndex(m_pEditor) : LayerIndex;
		m_CurrentGroupIndex = m_OriginalGroupIndex;
		m_CurrentLayerIndex = m_OriginalLayerIndex;

		int Value = PropToValue(Prop);
		if(StartChecker(Prop, State, Value))
		{
			m_Tracking = true;
			m_OriginalValue = Value;
			OnStart(Prop);
		}
	}

	void End(E Prop, EEditState State, int GroupIndex = -1, int LayerIndex = -1)
	{
		if(!m_Tracking || Prop == static_cast<E>(-1))
			return;

		m_CurrentGroupIndex = GroupIndex < 0 ? SPropTrackerHelper::GetDefaultGroupIndex(m_pEditor) : GroupIndex;
		m_CurrentLayerIndex = LayerIndex < 0 ? SPropTrackerHelper::GetDefaultLayerIndex(m_pEditor) : LayerIndex;

		int Value = PropToValue(Prop);
		if(EndChecker(Prop, State, Value))
		{
			m_Tracking = false;
			OnEnd(Prop, Value);
		}
	}

protected:
	virtual void OnStart(E Prop) {}
	virtual void OnEnd(E Prop, int Value) {}
	virtual int PropToValue(E Prop) { return 0; }
	virtual bool StartChecker(E Prop, EEditState State, int Value)
	{
		return State == EEditState::START || State == EEditState::ONE_GO;
	}
	virtual bool EndChecker(E Prop, EEditState State, int Value)
	{
		return (State == EEditState::END || State == EEditState::ONE_GO) && (Value != m_OriginalValue);
	}

	int m_OriginalValue;
	T *m_pObject;
	int m_OriginalLayerIndex;
	int m_OriginalGroupIndex;
	int m_CurrentLayerIndex;
	int m_CurrentGroupIndex;
	bool m_Tracking;
};

class CLayerPropTracker : public CPropTracker<CLayer, ELayerProp>
{
public:
	CLayerPropTracker(CEditor *pEditor) :
		CPropTracker<CLayer, ELayerProp>(pEditor){};

protected:
	void OnEnd(ELayerProp Prop, int Value) override;
	int PropToValue(ELayerProp Prop) override;
};

class CLayerTilesPropTracker : public CPropTracker<CLayerTiles, ETilesProp>
{
public:
	CLayerTilesPropTracker(CEditor *pEditor) :
		CPropTracker<CLayerTiles, ETilesProp>(pEditor){};

protected:
	void OnStart(ETilesProp Prop) override;
	void OnEnd(ETilesProp Prop, int Value) override;
	bool EndChecker(ETilesProp Prop, EEditState State, int Value) override;

	int PropToValue(ETilesProp Prop) override;

private:
	std::map<int, std::shared_ptr<CLayer>> m_SavedLayers;
};

class CLayerTilesCommonPropTracker : public CPropTracker<CLayerTiles, ETilesCommonProp>
{
public:
	CLayerTilesCommonPropTracker(CEditor *pEditor) :
		CPropTracker<CLayerTiles, ETilesCommonProp>(pEditor){};

protected:
	void OnStart(ETilesCommonProp Prop) override;
	void OnEnd(ETilesCommonProp Prop, int Value) override;
	bool EndChecker(ETilesCommonProp Prop, EEditState State, int Value) override;

	int PropToValue(ETilesCommonProp Prop) override;

private:
	std::map<std::shared_ptr<CLayerTiles>, std::map<int, std::shared_ptr<CLayer>>> m_SavedLayers;

public:
	std::vector<std::shared_ptr<CLayerTiles>> m_vpLayers;
	std::vector<int> m_vLayerIndices;
};

class CLayerGroupPropTracker : public CPropTracker<CLayerGroup, EGroupProp>
{
public:
	CLayerGroupPropTracker(CEditor *pEditor) :
		CPropTracker<CLayerGroup, EGroupProp>(pEditor){};

protected:
	void OnEnd(EGroupProp Prop, int Value) override;
	int PropToValue(EGroupProp Prop) override;
};

class CLayerQuadsPropTracker : public CPropTracker<CLayerQuads, ELayerQuadsProp>
{
public:
	CLayerQuadsPropTracker(CEditor *pEditor) :
		CPropTracker<CLayerQuads, ELayerQuadsProp>(pEditor){};

protected:
	void OnEnd(ELayerQuadsProp Prop, int Value) override;
	int PropToValue(ELayerQuadsProp Prop) override;
};

class CLayerSoundsPropTracker : public CPropTracker<CLayerSounds, ELayerSoundsProp>
{
public:
	CLayerSoundsPropTracker(CEditor *pEditor) :
		CPropTracker<CLayerSounds, ELayerSoundsProp>(pEditor){};

protected:
	void OnEnd(ELayerSoundsProp Prop, int Value) override;
	int PropToValue(ELayerSoundsProp Prop) override;
};

class CSoundSourcePropTracker : public CPropTracker<CSoundSource, ESoundProp>
{
public:
	CSoundSourcePropTracker(CEditor *pEditor) :
		CPropTracker<CSoundSource, ESoundProp>(pEditor) {}

protected:
	void OnEnd(ESoundProp Prop, int Value) override;
	int PropToValue(ESoundProp Prop) override;
};

class CSoundSourceRectShapePropTracker : public CPropTracker<CSoundSource, ERectangleShapeProp>
{
public:
	CSoundSourceRectShapePropTracker(CEditor *pEditor) :
		CPropTracker<CSoundSource, ERectangleShapeProp>(pEditor) {}

protected:
	void OnEnd(ERectangleShapeProp Prop, int Value) override;
	int PropToValue(ERectangleShapeProp Prop) override;
};

class CSoundSourceCircleShapePropTracker : public CPropTracker<CSoundSource, ECircleShapeProp>
{
public:
	CSoundSourceCircleShapePropTracker(CEditor *pEditor) :
		CPropTracker<CSoundSource, ECircleShapeProp>(pEditor) {}

protected:
	void OnEnd(ECircleShapeProp Prop, int Value) override;
	int PropToValue(ECircleShapeProp Prop) override;
};

#endif
