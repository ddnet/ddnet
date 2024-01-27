#ifndef GAME_EDITOR_EDITOR_ACTIONS_H
#define GAME_EDITOR_EDITOR_ACTIONS_H

#include "editor.h"
#include "editor_action.h"

class CEditorActionLayerBase : public IEditorAction
{
public:
	CEditorActionLayerBase(CEditor *pEditor, int GroupIndex, int LayerIndex);

	virtual void Undo() override {}
	virtual void Redo() override {}

protected:
	int m_GroupIndex;
	int m_LayerIndex;
	std::shared_ptr<CLayer> m_pLayer;
};

class CEditorBrushDrawAction : public IEditorAction
{
public:
	CEditorBrushDrawAction(CEditor *pEditor, int Group);

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
	CEditorActionQuadPlace(CEditor *pEditor, int GroupIndex, int LayerIndex, std::vector<CQuad> &vBrush);

	void Undo() override;
	void Redo() override;

private:
	std::vector<CQuad> m_vBrush;
};

class CEditorActionSoundPlace : public CEditorActionLayerBase
{
public:
	CEditorActionSoundPlace(CEditor *pEditor, int GroupIndex, int LayerIndex, std::vector<CSoundSource> &vBrush);

	void Undo() override;
	void Redo() override;

private:
	std::vector<CSoundSource> m_vBrush;
};

// -------------------------------------------------------------

class CEditorActionDeleteQuad : public CEditorActionLayerBase
{
public:
	CEditorActionDeleteQuad(CEditor *pEditor, int GroupIndex, int LayerIndex, std::vector<int> const &vQuadsIndices, std::vector<CQuad> const &vDeletedQuads);

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
	CEditorActionEditQuadPoint(CEditor *pEditor, int GroupIndex, int LayerIndex, int QuadIndex, std::vector<CPoint> const &vPreviousPoints, std::vector<CPoint> const &vCurrentPoints);

	void Undo() override;
	void Redo() override;

private:
	int m_QuadIndex;
	std::vector<CPoint> m_vPreviousPoints;
	std::vector<CPoint> m_vCurrentPoints;
};

class CEditorActionEditQuadProp : public CEditorActionLayerBase
{
public:
	CEditorActionEditQuadProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int QuadIndex, EQuadProp Prop, int Previous, int Current);

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
	CEditorActionEditQuadPointProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int QuadIndex, int PointIndex, EQuadPointProp Prop, int Previous, int Current);

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
	CEditorActionBulk(CEditor *pEditor, const std::vector<std::shared_ptr<IEditorAction>> &vpActions, const char *pDisplay = nullptr, bool Reverse = false);

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
	CEditorActionTileChanges(CEditor *pEditor, int GroupIndex, int LayerIndex, const char *pAction, const EditorTileStateChangeHistory<STileStateChange> &Changes);

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
	CEditorActionAddLayer(CEditor *pEditor, int GroupIndex, int LayerIndex, bool Duplicate = false);

	void Undo() override;
	void Redo() override;

private:
	bool m_Duplicate;
};

class CEditorActionDeleteLayer : public CEditorActionLayerBase
{
public:
	CEditorActionDeleteLayer(CEditor *pEditor, int GroupIndex, int LayerIndex);

	void Undo() override;
	void Redo() override;
};

class CEditorActionGroup : public IEditorAction
{
public:
	CEditorActionGroup(CEditor *pEditor, int GroupIndex, bool Delete);

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
	CEditorActionEditGroupProp(CEditor *pEditor, int GroupIndex, EGroupProp Prop, int Previous, int Current);

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
	CEditorActionEditLayerPropBase(CEditor *pEditor, int GroupIndex, int LayerIndex, E Prop, int Previous, int Current);

	virtual void Undo() override {}
	virtual void Redo() override {}

protected:
	E m_Prop;
	int m_Previous;
	int m_Current;
};

class CEditorActionEditLayerProp : public CEditorActionEditLayerPropBase<ELayerProp>
{
public:
	CEditorActionEditLayerProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ELayerProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	void Apply(int Value);
};

class CEditorActionEditLayerTilesProp : public CEditorActionEditLayerPropBase<ETilesProp>
{
public:
	CEditorActionEditLayerTilesProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ETilesProp Prop, int Previous, int Current);

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
	CEditorActionEditLayerQuadsProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ELayerQuadsProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	void Apply(int Value);
};

class CEditorActionEditLayersGroupAndOrder : public IEditorAction
{
public:
	CEditorActionEditLayersGroupAndOrder(CEditor *pEditor, int GroupIndex, const std::vector<int> &LayerIndices, int NewGroupIndex, const std::vector<int> &NewLayerIndices);

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

public:
	CEditorActionAppendMap(CEditor *pEditor, const char *pMapName, const SPrevInfo &PrevInfo, std::vector<int> &vImageIndexMap);

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
	CEditorActionTileArt(CEditor *pEditor, int PreviousImageCount, const char *pTileArtFile, std::vector<int> &vImageIndexMap);

	void Undo() override;
	void Redo() override;

private:
	int m_PreviousImageCount;
	char m_aTileArtFile[IO_MAX_PATH_LENGTH];
	std::vector<int> m_vImageIndexMap;
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

	CEditorCommandAction(CEditor *pEditor, EType Type, int *pSelectedCommandIndex, int CommandIndex, const char *pPreviousCommand = nullptr, const char *pCurrentCommand = nullptr);

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
	CEditorActionEnvelopeAdd(CEditor *pEditor, const std::shared_ptr<CEnvelope> &pEnv);

	void Undo() override;
	void Redo() override;

private:
	std::shared_ptr<CEnvelope> m_pEnv;
};

class CEditorActionEveloppeDelete : public IEditorAction
{
public:
	CEditorActionEveloppeDelete(CEditor *pEditor, int EnvelopeIndex);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	std::shared_ptr<CEnvelope> m_pEnv;
};

class CEditorActionEnvelopeEdit : public IEditorAction
{
public:
	enum class EEditType
	{
		SYNC,
		ORDER
	};

	CEditorActionEnvelopeEdit(CEditor *pEditor, int EnvelopeIndex, EEditType EditType, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvelopeIndex;
	EEditType m_EditType;
	int m_Previous;
	int m_Current;
	std::shared_ptr<CEnvelope> m_pEnv;
};

class CEditorActionEnvelopeEditPoint : public IEditorAction
{
public:
	enum class EEditType
	{
		TIME,
		VALUE,
		CURVE_TYPE,
		HANDLE
	};

	CEditorActionEnvelopeEditPoint(CEditor *pEditor, int EnvelopeIndex, int PointIndex, int Channel, EEditType EditType, int Previous, int Current);

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
	CEditorActionAddEnvelopePoint(CEditor *pEditor, int EnvIndex, int Time, ColorRGBA Channels);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvIndex;
	int m_Time;
	ColorRGBA m_Channels;
};

class CEditorActionDeleteEnvelopePoint : public IEditorAction
{
public:
	CEditorActionDeleteEnvelopePoint(CEditor *pEditor, int EnvIndex, int PointIndex);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvIndex;
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

	CEditorActionEditEnvelopePointValue(CEditor *pEditor, int EnvIndex, int PointIndex, int Channel, EType Type, int OldTime, int OldValue, int NewTime, int NewValue);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvIndex;
	int m_PtIndex;
	int m_Channel;
	EType m_Type;
	int m_OldTime;
	int m_OldValue;
	int m_NewTime;
	int m_NewValue;

	void Apply(bool Undo);
};

class CEditorActionResetEnvelopePointTangent : public IEditorAction
{
public:
	CEditorActionResetEnvelopePointTangent(CEditor *pEditor, int EnvIndex, int PointIndex, int Channel, bool In);

	void Undo() override;
	void Redo() override;

private:
	int m_EnvIndex;
	int m_PointIndex;
	int m_Channel;
	bool m_In;
	int m_Previous[2];
};

class CEditorActionEditLayerSoundsProp : public CEditorActionEditLayerPropBase<ELayerSoundsProp>
{
public:
	CEditorActionEditLayerSoundsProp(CEditor *pEditor, int GroupIndex, int LayerIndex, ELayerSoundsProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	void Apply(int Value);
};

class CEditorActionDeleteSoundSource : public CEditorActionLayerBase
{
public:
	CEditorActionDeleteSoundSource(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;
	CSoundSource m_Source;
};

class CEditorActionEditSoundSource : public CEditorActionLayerBase
{
public:
	enum class EEditType
	{
		SHAPE
	};

	CEditorActionEditSoundSource(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, EEditType Type, int Value);
	~CEditorActionEditSoundSource() override;

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;
	EEditType m_EditType;
	int m_CurrentValue;

	std::vector<int> m_vOriginalValues;
	void *m_pSavedObject;

	void Save();
};

class CEditorActionEditSoundSourceProp : public CEditorActionEditLayerPropBase<ESoundProp>
{
public:
	CEditorActionEditSoundSourceProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, ESoundProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;

private:
	void Apply(int Value);
};

class CEditorActionEditRectSoundSourceShapeProp : public CEditorActionEditLayerPropBase<ERectangleShapeProp>
{
public:
	CEditorActionEditRectSoundSourceShapeProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, ERectangleShapeProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;

private:
	void Apply(int Value);
};

class CEditorActionEditCircleSoundSourceShapeProp : public CEditorActionEditLayerPropBase<ECircleShapeProp>
{
public:
	CEditorActionEditCircleSoundSourceShapeProp(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, ECircleShapeProp Prop, int Previous, int Current);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;

private:
	void Apply(int Value);
};

class CEditorActionNewEmptySound : public CEditorActionLayerBase
{
public:
	CEditorActionNewEmptySound(CEditor *pEditor, int GroupIndex, int LayerIndex, int x, int y);

	void Undo() override;
	void Redo() override;

private:
	int m_X;
	int m_Y;
};

class CEditorActionNewEmptyQuad : public CEditorActionLayerBase
{
public:
	CEditorActionNewEmptyQuad(CEditor *pEditor, int GroupIndex, int LayerIndex, int x, int y);

	void Undo() override;
	void Redo() override;

private:
	int m_X;
	int m_Y;
};

class CEditorActionNewQuad : public CEditorActionLayerBase
{
public:
	CEditorActionNewQuad(CEditor *pEditor, int GroupIndex, int LayerIndex);

	void Undo() override;
	void Redo() override;

private:
	CQuad m_Quad;
};

class CEditorActionMoveSoundSource : public CEditorActionLayerBase
{
public:
	CEditorActionMoveSoundSource(CEditor *pEditor, int GroupIndex, int LayerIndex, int SourceIndex, CPoint OriginalPosition, CPoint CurrentPosition);

	void Undo() override;
	void Redo() override;

private:
	int m_SourceIndex;
	CPoint m_OriginalPosition;
	CPoint m_CurrentPosition;
};

#endif
