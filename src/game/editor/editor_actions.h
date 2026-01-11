#ifndef GAME_EDITOR_EDITOR_ACTIONS_H
#define GAME_EDITOR_EDITOR_ACTIONS_H

#include "editor_action.h"

#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/layer_speedup.h>
#include <game/editor/mapitems/layer_switch.h>
#include <game/editor/mapitems/layer_tele.h>
#include <game/editor/mapitems/layer_tiles.h>
#include <game/editor/mapitems/layer_tune.h>
#include <game/editor/quadart.h>
#include <game/mapitems.h>

#include <memory>
#include <string>
#include <vector>

class CEditorMap;
class IEditorEnvelopeReference;

class CEditorActionLayerBase : public IEditorAction
{
public:
	CEditorActionLayerBase(CEditorMap *pMap, int GroupIndex, int LayerIndex);

protected:
	int m_GroupIndex;
	int m_LayerIndex;
	std::shared_ptr<CLayer> m_pLayer;
};

class CEditorBrushDrawAction : public IEditorAction
{
public:
	CEditorBrushDrawAction(CEditorMap *pMap, int Group);

	void Undo() override;
	void Redo() override;
	bool IsEmpty() override;

private:
	int m_Group;
	// m_vTileChanges is a list of changes for each layer that was modified.
	// The std::pair is used to pair one layer (index) with its history (2D map).
	// EditorTileStateChangeHistory<T> is a 2D map, storing a change item at a specific y,x position.
	std::vector<std::pair<int, EditorTileStateChangeHistory<STileStateChange>>> m_vTileChanges;
	EditorTileStateChangeHistory<STeleTileStateChange> m_TeleTileChanges;
	EditorTileStateChangeHistory<SSpeedupTileStateChange> m_SpeedupTileChanges;
	EditorTileStateChangeHistory<SSwitchTileStateChange> m_SwitchTileChanges;
	EditorTileStateChangeHistory<STuneTileStateChange> m_TuneTileChanges;

	int m_TotalTilesDrawn;
	int m_TotalLayers;

	void Apply(bool Undo);
	void SetInfos();
};

// ---------------------------------------------------------

class CEditorActionQuadPlace : public CEditorActionLayerBase
{
public:
	CEditorActionQuadPlace(CEditorMap *pMap, int GroupIndex, int LayerIndex, std::vector<CQuad> &vBrush);

	void Undo() override;
	void Redo() override;

private:
	std::vector<CQuad> m_vBrush;
};

class CEditorActionSoundPlace : public CEditorActionLayerBase
{
public:
	CEditorActionSoundPlace(CEditorMap *pMap, int GroupIndex, int LayerIndex, std::vector<CSoundSource> &vBrush);

	void Undo() override;
	void Redo() override;

private:
	std::vector<CSoundSource> m_vBrush;
};

// -------------------------------------------------------------

class CEditorActionDeleteQuad : public CEditorActionLayerBase
{
public:
	CEditorActionDeleteQuad(CEditorMap *pMap, int GroupIndex, int LayerIndex, std::vector<int> const &vQuadsIndices, std::vector<CQuad> const &vDeletedQuads);

	void Undo() override;
	void Redo() override;

private:
	std::vector<int> m_vQuadsIndices;
	std::vector<CQuad> m_vDeletedQuads;
};

// -------------------------------------------------------------

class CEditorActionEditQuadPoint : public CEditorActionLayerBase
{
public:
	CEditorActionEditQuadPoint(CEditorMap *pMap, int GroupIndex, int LayerIndex, int QuadIndex, std::vector<CPoint> const &vPreviousPoints, std::vector<CPoint> const &vCurrentPoints);

	void Undo() override;
	void Redo() override;

private:
	int m_QuadIndex;
	std::vector<CPoint> m_vPreviousPoints;
	std::vector<CPoint> m_vCurrentPoints;

	void Apply(const std::vector<CPoint> &vValue);
};

class CEditorActionEditQuadColor : public CEditorActionLayerBase
{
public:
	CEditorActionEditQuadColor(CEditorMap *pMap, int GroupIndex, int LayerIndex, int QuadIndex, std::vector<CColor> const &vPreviousColors, std::vector<CColor> const &vCurrentColors);

	void Undo() override;
	void Redo() override;

private:
	int m_QuadIndex;
	std::vector<CColor> m_vPreviousColors;
	std::vector<CColor> m_vCurrentColors;

	void Apply(std::vector<CColor> &vValue);
};

class CEditorActionEditQuadProp : public CEditorActionLayerBase
{
public:
	CEditorActionEditQuadProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, int QuadIndex, EQuadProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_QuadIndex;
	EQuadProp m_Prop;
	int m_Previous;
	int m_Current;

	void Apply(int Value);
};

class CEditorActionEditQuadPointProp : public CEditorActionLayerBase
{
public:
	CEditorActionEditQuadPointProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, int QuadIndex, int PointIndex, EQuadPointProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_QuadIndex;
	int m_PointIndex;
	EQuadPointProp m_Prop;
	int m_Previous;
	int m_Current;

	void Apply(int Value);
};

// -------------------------------------------------------------

class CEditorActionBulk : public IEditorAction
{
public:
	CEditorActionBulk(CEditorMap *pMap, const std::vector<std::shared_ptr<IEditorAction>> &vpActions, const char *pDisplay = nullptr, bool Reverse = false);

	void Undo() override;
	void Redo() override;

private:
	std::vector<std::shared_ptr<IEditorAction>> m_vpActions;
	std::string m_Display;
	bool m_Reverse;
};

//

class CEditorActionTileChanges : public CEditorActionLayerBase
{
public:
	CEditorActionTileChanges(CEditorMap *pMap, int GroupIndex, int LayerIndex, const char *pAction, const EditorTileStateChangeHistory<STileStateChange> &Changes);

	void Undo() override;
	void Redo() override;

private:
	EditorTileStateChangeHistory<STileStateChange> m_Changes;
	int m_TotalChanges;

	void ComputeInfos();
	void Apply(bool Undo);
};

// -------------------------------------------------------------

class CEditorActionAddLayer : public CEditorActionLayerBase
{
public:
	CEditorActionAddLayer(CEditorMap *pMap, int GroupIndex, int LayerIndex, bool Duplicate = false);

	void Undo() override;
	void Redo() override;

private:
	bool m_Duplicate;
};

class CEditorActionDeleteLayer : public CEditorActionLayerBase
{
public:
	CEditorActionDeleteLayer(CEditorMap *pMap, int GroupIndex, int LayerIndex);

	void Undo() override;
	void Redo() override;
};

class CEditorActionGroup : public IEditorAction
{
public:
	CEditorActionGroup(CEditorMap *pMap, int GroupIndex, bool Delete);

	void Undo() override;
	void Redo() override;

private:
	int m_GroupIndex;
	bool m_Delete;
	std::shared_ptr<CLayerGroup> m_pGroup;
};

class CEditorActionEditGroupProp : public IEditorAction
{
public:
	CEditorActionEditGroupProp(CEditorMap *pMap, int GroupIndex, EGroupProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_GroupIndex;
	EGroupProp m_Prop;
	int m_Previous;
	int m_Current;

	void Apply(int Value);
};

template<typename E>
class CEditorActionEditLayerPropBase : public CEditorActionLayerBase
{
public:
	CEditorActionEditLayerPropBase(CEditorMap *pMap, int GroupIndex, int LayerIndex, E Prop, int Previous, int Current);

protected:
	E m_Prop;
	int m_Previous;
	int m_Current;
};

class CEditorActionEditLayerProp : public CEditorActionEditLayerPropBase<ELayerProp>
{
public:
	CEditorActionEditLayerProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, ELayerProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	void Apply(int Value);
};

class CEditorActionEditLayerTilesProp : public CEditorActionEditLayerPropBase<ETilesProp>
{
public:
	CEditorActionEditLayerTilesProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, ETilesProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

	void SetSavedLayers(const std::map<int, std::shared_ptr<CLayer>> &SavedLayers);

private:
	std::map<int, std::shared_ptr<CLayer>> m_SavedLayers;

	void RestoreLayer(int Layer, const std::shared_ptr<CLayerTiles> &pLayerTiles);
};

class CEditorActionEditLayerQuadsProp : public CEditorActionEditLayerPropBase<ELayerQuadsProp>
{
public:
	CEditorActionEditLayerQuadsProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, ELayerQuadsProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	void Apply(int Value);
};

class CEditorActionEditLayersGroupAndOrder : public IEditorAction
{
public:
	CEditorActionEditLayersGroupAndOrder(CEditorMap *pMap, int GroupIndex, const std::vector<int> &LayerIndices, int NewGroupIndex, const std::vector<int> &NewLayerIndices);

	void Undo() override;
	void Redo() override;

private:
	int m_GroupIndex;
	std::vector<int> m_LayerIndices;
	int m_NewGroupIndex;
	std::vector<int> m_NewLayerIndices;
};

// --------------

class CEditorActionAppendMap : public IEditorAction
{
public:
	struct SPrevInfo
	{
		int m_Groups;
		int m_Images;
		int m_Sounds;
		int m_Envelopes;
	};

	CEditorActionAppendMap(CEditorMap *pMap, const char *pMapName, const SPrevInfo &PrevInfo, std::vector<int> &vImageIndexMap);

	void Undo() override;
	void Redo() override;

private:
	char m_aMapName[IO_MAX_PATH_LENGTH];
	SPrevInfo m_PrevInfo;
	std::vector<int> m_vImageIndexMap;
};

// --------------

class CEditorActionTileArt : public IEditorAction
{
public:
	CEditorActionTileArt(CEditorMap *pMap, int PreviousImageCount, const char *pTileArtFile, std::vector<int> &vImageIndexMap);

	void Undo() override;
	void Redo() override;

private:
	int m_PreviousImageCount;
	char m_aTileArtFile[IO_MAX_PATH_LENGTH];
	std::vector<int> m_vImageIndexMap;
};

// --------------

class CEditorActionQuadArt : public IEditorAction
{
public:
	CEditorActionQuadArt(CEditorMap *pMap, CQuadArtParameters Parameters);

	void Undo() override;
	void Redo() override;

private:
	CQuadArtParameters m_Parameters;
};

// ----------------------

class CEditorCommandAction : public IEditorAction
{
public:
	enum class EType
	{
		DELETE,
		ADD,
		EDIT,
		MOVE_UP,
		MOVE_DOWN
	};

	CEditorCommandAction(CEditorMap *pMap, EType Type, int *pSelectedCommandIndex, int CommandIndex, const char *pPreviousCommand = nullptr, const char *pCurrentCommand = nullptr);

	void Undo() override;
	void Redo() override;

private:
	EType m_Type;
	int *m_pSelectedCommandIndex;
	int m_CommandIndex;
	std::string m_PreviousCommand;
	std::string m_CurrentCommand;
};

// ------------------------------

class CEditorActionEnvelopeAdd : public IEditorAction
{
public:
	CEditorActionEnvelopeAdd(CEditorMap *pMap, CEnvelope::EType EnvelopeType);

	void Undo() override;
	void Redo() override;

private:
	CEnvelope::EType m_EnvelopeType;
	int m_PreviousSelectedEnvelope;
};

class CEditorActionEnvelopeDelete : public IEditorAction
{
public:
	CEditorActionEnvelopeDelete(CEditorMap *pMap, int EnvelopeIndex, std::vector<std::shared_ptr<IEditorEnvelopeReference>> &vpObjectReferences, std::shared_ptr<CEnvelope> &pEnvelope);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	std::shared_ptr<CEnvelope> m_pEnv;
	std::vector<std::shared_ptr<IEditorEnvelopeReference>> m_vpObjectReferences;
};

class CEditorActionEnvelopeEdit : public IEditorAction
{
public:
	enum class EEditType
	{
		SYNC,
		ORDER
	};

	CEditorActionEnvelopeEdit(CEditorMap *pMap, int EnvelopeIndex, EEditType EditType, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	EEditType m_EditType;
	int m_Previous;
	int m_Current;
	std::shared_ptr<CEnvelope> m_pEnv;
};

class CEditorActionEnvelopeEditPointTime : public IEditorAction
{
public:
	CEditorActionEnvelopeEditPointTime(CEditorMap *pMap, int EnvelopeIndex, int PointIndex, CFixedTime Previous, CFixedTime Current);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	int m_PointIndex;
	CFixedTime m_Previous;
	CFixedTime m_Current;
	std::shared_ptr<CEnvelope> m_pEnv;

	void Apply(CFixedTime Value);
};

class CEditorActionEnvelopeEditPoint : public IEditorAction
{
public:
	enum class EEditType
	{
		VALUE,
		CURVE_TYPE,
	};

	CEditorActionEnvelopeEditPoint(CEditorMap *pMap, int EnvelopeIndex, int PointIndex, int Channel, EEditType EditType, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	int m_PointIndex;
	int m_Channel;
	EEditType m_EditType;
	int m_Previous;
	int m_Current;
	std::shared_ptr<CEnvelope> m_pEnv;

	void Apply(int Value);
};

class CEditorActionAddEnvelopePoint : public IEditorAction
{
public:
	CEditorActionAddEnvelopePoint(CEditorMap *pMap, int EnvelopeIndex, CFixedTime Time, ColorRGBA Channels);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	CFixedTime m_Time;
	ColorRGBA m_Channels;
};

class CEditorActionDeleteEnvelopePoint : public IEditorAction
{
public:
	CEditorActionDeleteEnvelopePoint(CEditorMap *pMap, int EnvelopeIndex, int PointIndex);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	int m_PointIndex;
	CEnvPoint_runtime m_Point;
};

class CEditorActionEditEnvelopePointValue : public IEditorAction
{
public:
	enum class EType
	{
		TANGENT_IN,
		TANGENT_OUT,
		POINT
	};

	CEditorActionEditEnvelopePointValue(CEditorMap *pMap, int EnvelopeIndex, int PointIndex, int Channel, EType Type, CFixedTime OldTime, int OldValue, CFixedTime NewTime, int NewValue);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	int m_PointIndex;
	int m_Channel;
	EType m_Type;
	CFixedTime m_OldTime;
	int m_OldValue;
	CFixedTime m_NewTime;
	int m_NewValue;

	void Apply(bool Undo);
};

class CEditorActionResetEnvelopePointTangent : public CEditorActionEditEnvelopePointValue
{
public:
	CEditorActionResetEnvelopePointTangent(CEditorMap *pMap, int EnvelopeIndex, int PointIndex, int Channel, bool In, CFixedTime OldTime, int OldValue);
};

class CEditorActionEditLayerSoundsProp : public CEditorActionEditLayerPropBase<ELayerSoundsProp>
{
public:
	CEditorActionEditLayerSoundsProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, ELayerSoundsProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	void Apply(int Value);
};

class CEditorActionDeleteSoundSource : public CEditorActionLayerBase
{
public:
	CEditorActionDeleteSoundSource(CEditorMap *pMap, int GroupIndex, int LayerIndex, int SourceIndex);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;
	CSoundSource m_Source;
};

class CEditorActionEditSoundSourceShape : public CEditorActionLayerBase
{
public:
	CEditorActionEditSoundSourceShape(CEditorMap *pMap, int GroupIndex, int LayerIndex, int SourceIndex, int Value);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;
	int m_CurrentValue;

	std::vector<int> m_vOriginalValues;
	CSoundShape m_SavedShape;

	void Save();
};

class CEditorActionEditSoundSourceProp : public CEditorActionEditLayerPropBase<ESoundProp>
{
public:
	CEditorActionEditSoundSourceProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, int SourceIndex, ESoundProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;

	void Apply(int Value);
};

class CEditorActionEditRectSoundSourceShapeProp : public CEditorActionEditLayerPropBase<ERectangleShapeProp>
{
public:
	CEditorActionEditRectSoundSourceShapeProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, int SourceIndex, ERectangleShapeProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;

	void Apply(int Value);
};

class CEditorActionEditCircleSoundSourceShapeProp : public CEditorActionEditLayerPropBase<ECircleShapeProp>
{
public:
	CEditorActionEditCircleSoundSourceShapeProp(CEditorMap *pMap, int GroupIndex, int LayerIndex, int SourceIndex, ECircleShapeProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;

	void Apply(int Value);
};

class CEditorActionNewEmptySound : public CEditorActionLayerBase
{
public:
	CEditorActionNewEmptySound(CEditorMap *pMap, int GroupIndex, int LayerIndex, int x, int y);

	void Undo() override;
	void Redo() override;

private:
	int m_X;
	int m_Y;
};

class CEditorActionNewEmptyQuad : public CEditorActionLayerBase
{
public:
	CEditorActionNewEmptyQuad(CEditorMap *pMap, int GroupIndex, int LayerIndex, int x, int y);

	void Undo() override;
	void Redo() override;

private:
	int m_X;
	int m_Y;
};

class CEditorActionNewQuad : public CEditorActionLayerBase
{
public:
	CEditorActionNewQuad(CEditorMap *pMap, int GroupIndex, int LayerIndex);

	void Undo() override;
	void Redo() override;

private:
	CQuad m_Quad;
};

class CEditorActionMoveSoundSource : public CEditorActionLayerBase
{
public:
	CEditorActionMoveSoundSource(CEditorMap *pMap, int GroupIndex, int LayerIndex, int SourceIndex, CPoint OriginalPosition, CPoint CurrentPosition);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;
	CPoint m_OriginalPosition;
	CPoint m_CurrentPosition;
};

#endif
