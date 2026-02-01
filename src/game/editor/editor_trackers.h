#ifndef GAME_EDITOR_EDITOR_TRACKERS_H
#define GAME_EDITOR_EDITOR_TRACKERS_H

#include <game/client/ui.h>
#include <game/editor/map_object.h>
#include <game/editor/mapitems.h>
#include <game/mapitems.h>

#include <map>
#include <memory>
#include <vector>

class CLayer;
class CLayerGroup;
class CLayerQuads;
class CLayerSounds;
class CLayerTiles;
class CSoundSource;

class CQuadEditTracker : public CMapObject
{
public:
	explicit CQuadEditTracker(CEditorMap *pMap);

	bool QuadPointChanged(const std::vector<CPoint> &vCurrentPoints, int QuadIndex);
	bool QuadColorChanged(const std::vector<CColor> &vCurrentColors, int QuadIndex);

	void BeginQuadTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int GroupIndex = -1, int LayerIndex = -1);
	void EndQuadTrack();

	void BeginQuadPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, EQuadProp Prop, int GroupIndex = -1, int LayerIndex = -1);
	void EndQuadPropTrack(EQuadProp Prop);

	void BeginQuadPointPropTrack(const std::shared_ptr<CLayerQuads> &pLayer, const std::vector<int> &vSelectedQuads, int SelectedQuadPoints, int GroupIndex = -1, int LayerIndex = -1);
	void AddQuadPointPropTrack(EQuadPointProp Prop);
	void EndQuadPointPropTrack(EQuadPointProp Prop);
	void EndQuadPointPropTrackAll();

private:
	std::vector<int> m_vSelectedQuads;
	int m_SelectedQuadPoints;
	std::map<int, std::vector<CPoint>> m_InitialPoints;
	std::map<int, std::vector<CColor>> m_InitialColors;

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
	NONE = 0,
	SELECT,
	DRAG_POINT,
	DRAG_POINT_X,
	DRAG_POINT_Y,
	CONTEXT_MENU,
	BOX_SELECT,
	SCALE,
};

enum class ESoundSourceOp
{
	NONE = 0,
	MOVE,
	CONTEXT_MENU,
};

class CEnvelopeEditorOperationTracker : public CMapObject
{
public:
	explicit CEnvelopeEditorOperationTracker(CEditorMap *pMap) :
		CMapObject(pMap) {}

	void Begin(EEnvelopeEditorOp Operation);
	void Stop(bool Switch = true);
	void Reset() { m_TrackedOp = EEnvelopeEditorOp::NONE; }

private:
	EEnvelopeEditorOp m_TrackedOp = EEnvelopeEditorOp::NONE;

	class CPointData
	{
	public:
		bool m_Used;
		CFixedTime m_Time;
		std::map<int, int> m_Values;
	};

	std::map<int, CPointData> m_SavedValues;

	void HandlePointDragStart();
	void HandlePointDragEnd(bool Switch);
};

class CSoundSourceOperationTracker : public CMapObject
{
public:
	explicit CSoundSourceOperationTracker(CEditorMap *pMap);

	void Begin(const CSoundSource *pSource, ESoundSourceOp Operation, int LayerIndex);
	void End();

private:
	const CSoundSource *m_pSource;
	ESoundSourceOp m_TrackedOp;
	int m_LayerIndex;

	class CData
	{
	public:
		CPoint m_OriginalPoint;
	};
	CData m_Data;
};

class CPropTrackerHelper
{
public:
	static int GetDefaultGroupIndex(CEditorMap *pMap);
	static int GetDefaultLayerIndex(CEditorMap *pMap);
};

template<typename T, typename E>
class CPropTracker : public CMapObject
{
public:
	explicit CPropTracker(CEditorMap *pMap) :
		CMapObject(pMap),
		m_OriginalValue(0),
		m_pObject(nullptr),
		m_OriginalLayerIndex(-1),
		m_OriginalGroupIndex(-1),
		m_CurrentLayerIndex(-1),
		m_CurrentGroupIndex(-1),
		m_Tracking(false) {}

	void Begin(const T *pObject, E Prop, EEditState State, int GroupIndex = -1, int LayerIndex = -1)
	{
		if(m_Tracking || Prop == static_cast<E>(-1))
			return;
		m_pObject = pObject;

		m_OriginalGroupIndex = GroupIndex < 0 ? CPropTrackerHelper::GetDefaultGroupIndex(Map()) : GroupIndex;
		m_OriginalLayerIndex = LayerIndex < 0 ? CPropTrackerHelper::GetDefaultLayerIndex(Map()) : LayerIndex;
		m_CurrentGroupIndex = m_OriginalGroupIndex;
		m_CurrentLayerIndex = m_OriginalLayerIndex;

		int Value = PropToValue(Prop);
		if(State == EEditState::START || State == EEditState::ONE_GO)
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

		m_CurrentGroupIndex = GroupIndex < 0 ? CPropTrackerHelper::GetDefaultGroupIndex(Map()) : GroupIndex;
		m_CurrentLayerIndex = LayerIndex < 0 ? CPropTrackerHelper::GetDefaultLayerIndex(Map()) : LayerIndex;

		if(State == EEditState::END || State == EEditState::ONE_GO)
		{
			m_Tracking = false;
			int Value = PropToValue(Prop);
			if(EndChecker(Prop, Value))
				OnEnd(Prop, Value);
		}
	}

protected:
	virtual void OnStart(E Prop) {}
	virtual void OnEnd(E Prop, int Value) {}
	virtual int PropToValue(E Prop) { return 0; }
	virtual bool EndChecker(E Prop, int Value)
	{
		return Value != m_OriginalValue;
	}

	int m_OriginalValue;
	const T *m_pObject;
	int m_OriginalLayerIndex;
	int m_OriginalGroupIndex;
	int m_CurrentLayerIndex;
	int m_CurrentGroupIndex;
	bool m_Tracking;
};

class CLayerPropTracker : public CPropTracker<CLayer, ELayerProp>
{
public:
	explicit CLayerPropTracker(CEditorMap *pMap) :
		CPropTracker<CLayer, ELayerProp>(pMap) {}

protected:
	void OnEnd(ELayerProp Prop, int Value) override;
	int PropToValue(ELayerProp Prop) override;
};

class CLayerTilesPropTracker : public CPropTracker<CLayerTiles, ETilesProp>
{
public:
	explicit CLayerTilesPropTracker(CEditorMap *pMap) :
		CPropTracker<CLayerTiles, ETilesProp>(pMap) {}

protected:
	void OnStart(ETilesProp Prop) override;
	void OnEnd(ETilesProp Prop, int Value) override;
	bool EndChecker(ETilesProp Prop, int Value) override;

	int PropToValue(ETilesProp Prop) override;

private:
	std::map<int, std::shared_ptr<CLayer>> m_SavedLayers;
};

class CLayerTilesCommonPropTracker : public CPropTracker<CLayerTiles, ETilesCommonProp>
{
public:
	explicit CLayerTilesCommonPropTracker(CEditorMap *pMap) :
		CPropTracker<CLayerTiles, ETilesCommonProp>(pMap) {}

protected:
	void OnStart(ETilesCommonProp Prop) override;
	void OnEnd(ETilesCommonProp Prop, int Value) override;
	bool EndChecker(ETilesCommonProp Prop, int Value) override;

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
	explicit CLayerGroupPropTracker(CEditorMap *pMap) :
		CPropTracker<CLayerGroup, EGroupProp>(pMap) {}

protected:
	void OnEnd(EGroupProp Prop, int Value) override;
	int PropToValue(EGroupProp Prop) override;
};

class CLayerQuadsPropTracker : public CPropTracker<CLayerQuads, ELayerQuadsProp>
{
public:
	explicit CLayerQuadsPropTracker(CEditorMap *pMap) :
		CPropTracker<CLayerQuads, ELayerQuadsProp>(pMap) {}

protected:
	void OnEnd(ELayerQuadsProp Prop, int Value) override;
	int PropToValue(ELayerQuadsProp Prop) override;
};

class CLayerSoundsPropTracker : public CPropTracker<CLayerSounds, ELayerSoundsProp>
{
public:
	explicit CLayerSoundsPropTracker(CEditorMap *pMap) :
		CPropTracker<CLayerSounds, ELayerSoundsProp>(pMap) {}

protected:
	void OnEnd(ELayerSoundsProp Prop, int Value) override;
	int PropToValue(ELayerSoundsProp Prop) override;
};

class CSoundSourcePropTracker : public CPropTracker<CSoundSource, ESoundProp>
{
public:
	explicit CSoundSourcePropTracker(CEditorMap *pMap) :
		CPropTracker<CSoundSource, ESoundProp>(pMap) {}

protected:
	void OnEnd(ESoundProp Prop, int Value) override;
	int PropToValue(ESoundProp Prop) override;
};

class CSoundSourceRectShapePropTracker : public CPropTracker<CSoundSource, ERectangleShapeProp>
{
public:
	explicit CSoundSourceRectShapePropTracker(CEditorMap *pMap) :
		CPropTracker<CSoundSource, ERectangleShapeProp>(pMap) {}

protected:
	void OnEnd(ERectangleShapeProp Prop, int Value) override;
	int PropToValue(ERectangleShapeProp Prop) override;
};

class CSoundSourceCircleShapePropTracker : public CPropTracker<CSoundSource, ECircleShapeProp>
{
public:
	explicit CSoundSourceCircleShapePropTracker(CEditorMap *pMap) :
		CPropTracker<CSoundSource, ECircleShapeProp>(pMap) {}

protected:
	void OnEnd(ECircleShapeProp Prop, int Value) override;
	int PropToValue(ECircleShapeProp Prop) override;
};

#endif
