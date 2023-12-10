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

	void BeginQuadTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads);
	void EndQuadTrack();

	void BeginQuadPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, EQuadProp Prop);
	void EndQuadPropTrack(EQuadProp Prop);

	void BeginQuadPointPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int SelectedQuadPoints);
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
};

class CEditorPropTracker
{
public:
	CEditorPropTracker() = default;

	void BeginPropTrack(int Prop, int Value);
	void StopPropTrack(int Prop, int Value);
	inline void Reset() { m_TrackedProp = -1; }

	CEditor *m_pEditor;
	int m_PreviousValue;
	int m_CurrentValue;

private:
	int m_TrackedProp = -1;
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

template<typename T, typename E>
class CPropTracker
{
public:
	CPropTracker(CEditor *pEditor) :
		m_pEditor(pEditor), m_OriginalValue(0) {}
	CEditor *m_pEditor;

	void Begin(T *pObject, E Prop, EEditState State)
	{
		if(Prop == static_cast<E>(-1))
			return;
		m_pObject = pObject;
		int Value = PropToValue(Prop);
		if(StartChecker(Prop, State, Value))
		{
			m_OriginalValue = Value;
			OnStart(Prop);
		}
	}

	void End(E Prop, EEditState State)
	{
		if(Prop == static_cast<E>(-1))
			return;
		int Value = PropToValue(Prop);
		if(EndChecker(Prop, State, Value))
		{
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
