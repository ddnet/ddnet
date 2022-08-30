#ifndef GAME_EDITOR_EDITOR_ACTIONS_H
#define GAME_EDITOR_EDITOR_ACTIONS_H

#include <game/editor/editor.h>
#include <game/mapitems.h>

class CLayerTiles;
class CEditorMap;
struct CLayerTiles_SCommonPropState;
struct RECTi;
class CLayerQuads;
class CLayer;

class IEditorAction
{
public:
	IEditorAction()
	{
		m_pEditor = nullptr;
	}

	virtual ~IEditorAction() = default;

	virtual bool Undo() = 0;
	virtual bool Redo() = 0;

	virtual void Print()
	{
	}

	virtual const char *Name() const = 0;

	class CEditor *m_pEditor;

	enum class EType
	{
		CHANGE_COLOR_TILE,
		CHANGE_COLOR_QUAD,

		EDIT_MULTIPLE_LAYERS,

		ADD_LAYER,
		DELETE_LAYER,

		ADD_GROUP,
		DELETE_GROUP,

		EDIT_LAYER_GROUP,
		EDIT_LAYER_ORDER,
		EDIT_LAYER_WIDTH,
		EDIT_LAYER_HEIGHT,
		EDIT_LAYER_IMAGE,
		EDIT_LAYER_COLOR_ENV,
		EDIT_LAYER_COLOR_TO,

		ADD_QUAD,
		DELETE_QUAD,
		EDIT_QUAD_POSITION,
		EDIT_QUAD_CENTER,
		EDIT_QUAD_VERTEX,
		ROTATE_QUAD,
		QUAD_OPERATION_SQUARE,
		QUAD_OPERATION_ALIGN,
		QUAD_OPERATION_ASPECT_RATIO,

		//SET_TILE,
		FILL_SELECTION,
		BRUSH_DRAW,

		EDIT_GROUP_ORDER,
		EDIT_GROUP_PROPERTY,
		//EDIT_GROUP_POS_X,
		//EDIT_GROUP_POS_Y,
		//EDIT_GROUP_PARA_X,
		//EDIT_GROUP_PARA_Y,
		//EDIT_GROUP_CLIP_X,
		//EDIT_GROUP_CLIP_Y,
		//EDIT_GROUP_CLIP_W,
		//EDIT_GROUP_CLIP_H,

		ADD_ENVELOPPE,
		EDIT_ENV_NAME,
		EDIT_ENV_POINT_VALUE,
		EDIT_ENV_POINT_TIME,
		EDIT_ENV_SYNC_CHECKBOX,
		DELETE_ENVELOPPE,

		ADD_COMMAND,
		EDIT_COMMAND,
		DELETE_COMMAND,
		MOVE_COMMAND_UP,
		MOVE_COMMAND_DOWN,

		//ADD_IMAGE,
		//ADD_SOUND,

		CHANGE_ORIENTATION_VALUE
	};
};

template<typename T>
class CEditorAction : public IEditorAction
{
public:
	CEditorAction(IEditorAction::EType Type, void *pObject, const T &From, const T &To) :
		IEditorAction()
	{
		m_ValueFrom = From;
		m_ValueTo = To;
		m_pObject = pObject;
		m_Type = Type;
	}

	virtual ~CEditorAction() = default;

	const char *Name() const override
	{
		switch(m_Type)
		{
		case EType::CHANGE_COLOR_TILE: return "CHANGE_COLOR_TILE";
		case EType::CHANGE_COLOR_QUAD: return "CHANGE_COLOR_QUAD";
		case EType::EDIT_MULTIPLE_LAYERS: return "EDIT_MULTIPLE_LAYERS";
		case EType::ADD_LAYER: return "ADD_LAYER";
		case EType::DELETE_LAYER: return "DELETE_LAYER";
		case EType::ADD_GROUP: return "ADD_GROUP";
		case EType::DELETE_GROUP: return "DELETE_GROUP";
		case EType::EDIT_LAYER_GROUP: return "EDIT_LAYER_GROUP";
		case EType::EDIT_LAYER_ORDER: return "EDIT_LAYER_ORDER";
		case EType::EDIT_LAYER_WIDTH: return "EDIT_LAYER_WIDTH";
		case EType::EDIT_LAYER_HEIGHT: return "EDIT_LAYER_HEIGHT";
		case EType::EDIT_LAYER_IMAGE: return "EDIT_LAYER_IMAGE";
		case EType::EDIT_LAYER_COLOR_ENV: return "EDIT_LAYER_COLOR_ENV";
		case EType::EDIT_LAYER_COLOR_TO: return "EDIT_LAYER_COLOR_TO";
		case EType::EDIT_QUAD_POSITION: return "EDIT_QUAD_POSITION";
		case EType::EDIT_QUAD_CENTER: return "EDIT_QUAD_CENTER";
		case EType::FILL_SELECTION: return "FILL_SELECTION";
		case EType::BRUSH_DRAW: return "BRUSH_DRAW";
		case EType::EDIT_GROUP_ORDER: return "EDIT_GROUP_ORDER";
		case EType::EDIT_GROUP_PROPERTY: return "EDIT_GROUP_PROPERTY";
		//case EType::EDIT_GROUP_POS_X: return "EDIT_GROUP_POS_X";
		//case EType::EDIT_GROUP_POS_Y: return "EDIT_GROUP_POS_Y";
		//case EType::EDIT_GROUP_PARA_X: return "EDIT_GROUP_PARA_X";
		//case EType::EDIT_GROUP_PARA_Y: return "EDIT_GROUP_PARA_Y";
		//case EType::EDIT_GROUP_CLIP_X: return "EDIT_GROUP_CLIP_X";
		//case EType::EDIT_GROUP_CLIP_Y: return "EDIT_GROUP_CLIP_Y";
		//case EType::EDIT_GROUP_CLIP_W: return "EDIT_GROUP_CLIP_W";
		//case EType::EDIT_GROUP_CLIP_H: return "EDIT_GROUP_CLIP_H";
		case EType::ADD_ENVELOPPE: return "ADD_ENVELOPPE";
		case EType::EDIT_ENV_NAME: return "EDIT_ENV_NAME";
		case EType::EDIT_ENV_POINT_VALUE: return "EDIT_ENV_POINT_VALUE";
		case EType::EDIT_ENV_POINT_TIME: return "EDIT_ENV_POINT_TIME";
		case EType::EDIT_ENV_SYNC_CHECKBOX: return "EDIT_ENV_SYNC_CHECKBOX";
		case EType::DELETE_ENVELOPPE: return "DELETE_ENVELOPPE";
		case EType::ADD_COMMAND: return "ADD_COMMAND";
		case EType::EDIT_COMMAND: return "EDIT_COMMAND";
		case EType::DELETE_COMMAND: return "DELETE_COMMAND";
		case EType::MOVE_COMMAND_UP: return "MOVE_COMMAND_UP";
		case EType::MOVE_COMMAND_DOWN: return "MOVE_COMMAND_DOWN";
		//case EType::ADD_IMAGE: return "ADD_IMAGE";
		//case EType::ADD_SOUND: return "ADD_SOUND";
		case EType::CHANGE_ORIENTATION_VALUE: return "CHANGE_ORIENTATION_VALUE";
		case EType::ADD_QUAD: return "ADD_QUAD";
		case EType::DELETE_QUAD: return "DELETE_QUAD";
		case EType::EDIT_QUAD_VERTEX: return "EDIT_QUAD_VERTEX";
		case EType::ROTATE_QUAD: return "ROTATE_QUAD";
		case EType::QUAD_OPERATION_ALIGN: return "QUAD_OPERATION_ALIGN";
		case EType::QUAD_OPERATION_ASPECT_RATIO: return "QUAD_OPERATION_ASPECT_RATIO";
		case EType::QUAD_OPERATION_SQUARE: return "QUAD_OPERATION_SQUARE";
		default: return "UNKNOWN";
		}
	}

	EType m_Type;

protected:
	T m_ValueFrom;
	T m_ValueTo;
	void *m_pObject;
};

template<typename T>
class CEditorLayerAction : public CEditorAction<T>
{
public:
	CEditorLayerAction(IEditorAction::EType Type, void *pObject, int GroupIndex, int LayerIndex, const T &From, const T &To) :
		CEditorAction<T>(Type, pObject, From, To)
	{
		m_GroupIndex = GroupIndex;
		m_LayerIndex = LayerIndex;
	}

	virtual bool Undo() override = 0;
	virtual bool Redo() override = 0;
	virtual void Print() override
	{
	}

	template<typename K>
	K *GetLayer();

protected:
	int m_GroupIndex;
	int m_LayerIndex;
};

template<typename T>
class CEditorLayerQuadsAction : public CEditorLayerAction<T>
{
public:
	CEditorLayerQuadsAction(IEditorAction::EType Type, int GroupIndex, int LayerIndex, const std::vector<int> &vQuads, const T &Old, const T &New) :
		CEditorLayerAction<T>(Type, nullptr, GroupIndex, LayerIndex, Old, New)
	{
		m_vQuads = vQuads;
	}

	CLayerQuads *GetLayer();

	virtual bool Undo() override = 0;
	virtual bool Redo() override = 0;
	virtual void Print() override
	{
	}

protected:
	std::vector<int> m_vQuads;
};

class CEditorChangeColorTileAction : public CEditorLayerAction<CColor>
{
public:
	CEditorChangeColorTileAction(int GroupIndex, int LayerIndex, const CColor &OldColor, const CColor &NewColor) :
		CEditorLayerAction(IEditorAction::EType::CHANGE_COLOR_TILE, nullptr, GroupIndex, LayerIndex, OldColor, NewColor)
	{
	}

	bool Undo() override;
	bool Redo() override;

	//void Print() override
	//{
	//	dbg_msg("editor", "Editor action: Change tile color, prev=%d %d %d %d, new=%d %d %d %d", m_ValueFrom.r, m_ValueFrom.g, m_ValueFrom.b, m_ValueFrom.a, m_ValueTo.r, m_ValueTo.g, m_ValueTo.b, m_ValueTo.a);
	//}
};

class CEditorAddLayerAction : public CEditorAction<CLayer *>
{
public:
	CEditorAddLayerAction(int GroupIndex, int LayerIndex, const std::function<void()> &fnAddLayer) :
		CEditorAction(IEditorAction::EType::ADD_LAYER, nullptr, nullptr, nullptr)
	{
		m_GroupIndex = GroupIndex;
		m_LayerIndex = LayerIndex;
		m_fnAddLayer = fnAddLayer;
	}

	~CEditorAddLayerAction() = default;

	bool Undo() override;

	bool Redo() override
	{
		// call the add layer function, needed for special layers
		m_fnAddLayer();
		return true;
	}

	void Print() override
	{
		//dbg_msg("editor", "Editor action: Add layer, type=%d name=%s", m_ValueTo->m_Type, m_ValueTo->m_aName);
	}

private:
	int m_LayerIndex;
	int m_GroupIndex;
	std::function<void()> m_fnAddLayer;
};

class CEditorDeleteLayerAction : public CEditorAction<CLayer *>
{
public:
	CEditorDeleteLayerAction(CEditor *pEditor, int GroupIndex, int LayerIndex);
	~CEditorDeleteLayerAction();

	bool Undo() override;
	bool Redo() override;

private:
	int m_LayerIndex;
	int m_GroupIndex;
};

class CEditorEditMultipleLayersAction : public CEditorAction<std::vector<CLayerTiles_SCommonPropState>>
{
public:
	CEditorEditMultipleLayersAction(void *pObject, const std::vector<CLayerTiles_SCommonPropState> &vOriginals, const CLayerTiles_SCommonPropState &State, int GroupIndex, const std::vector<int> &vLayers);

	bool Undo() override;
	bool Redo() override;

	void Print() override
	{
		dbg_msg("editor", "Editor action: Edit multiple layers");
	}

private:
	std::vector<int> m_vLayers;
	int m_GroupIndex;
};

class CEditorAddGroupAction : public CEditorAction<int>
{
public:
	CEditorAddGroupAction(CEditorMap *pObject, int GroupIndex);

	bool Undo() override;
	bool Redo() override;
};

class CLayerGroup;

class CEditorDeleteGroupAction : public CEditorAction<CLayerGroup *>
{
public:
	CEditorDeleteGroupAction(CEditorMap *pObject, int GroupIndex);
	~CEditorDeleteGroupAction();

	bool Undo() override;
	bool Redo() override;

private:
	int m_GroupIndex;
};

class CEditorFillSelectionAction : public CEditorAction<CLayerGroup *>
{
public:
	CEditorFillSelectionAction(void *pObject, CLayerGroup *Original, CLayerGroup *Brush, int GroupIndex, const std::vector<int> &vLayers, const CUIRect &Rect);
	~CEditorFillSelectionAction();

	bool Undo() override;
	bool Redo() override;

	void Print() override;

private:
	CUIRect m_Rect;
	int m_GroupIndex;
	std::vector<int> m_vLayers;
};

class CEditorBrushDrawAction : public CEditorAction<CLayerGroup *>
{
public:
	CEditorBrushDrawAction(void *pObject, CLayerGroup *Original, CLayerGroup *Brush, int GroupIndex, const std::vector<int> &vLayers);
	~CEditorBrushDrawAction();

	bool Undo() override;
	bool Redo() override;

	void Print() override;

private:
	std::vector<int> m_vLayers;
	int m_GroupIndex;

	bool BrushDraw(CLayerGroup *Brush);
};

class CEditorCommandAction : public CEditorAction<std::string>
{
public:
	CEditorCommandAction(IEditorAction::EType Action, int *pSelectedCommand, int CommandIndex, const std::string &Old, const std::string &New) :
		CEditorAction(Action, nullptr, Old, New)
	{
		m_pSelectedCommand = pSelectedCommand;
		m_CommandIndex = CommandIndex;
	}

	bool Undo() override;
	bool Redo() override;

private:
	int m_CommandIndex;
	int *m_pSelectedCommand;
};

class CEditorAddQuadAction : public CEditorLayerAction<CQuad *>
{
public:
	CEditorAddQuadAction(int GroupIndex, int LayerIndex, CQuad *pQuad);
	~CEditorAddQuadAction();

	bool Undo() override;
	bool Redo() override;
};

class CEditorDeleteQuadsAction : public CEditorLayerAction<void *>
{
public:
	CEditorDeleteQuadsAction(int GroupIndex, int LayerIndex, const std::vector<int> &vQuadIndexes, const std::vector<CQuad> &vQuads) :
		CEditorLayerAction(IEditorAction::EType::DELETE_QUAD, nullptr, GroupIndex, LayerIndex, nullptr, nullptr)
	{
		m_vQuadIndexes = vQuadIndexes;
		m_vQuads = vQuads;
	}

	bool Undo() override;
	bool Redo() override;

private:
	std::vector<int> m_vQuadIndexes;
	std::vector<CQuad> m_vQuads;
};

class CEditorEditQuadPositionAction : public CEditorLayerQuadsAction<std::vector<CPoint>>
{
public:
	CEditorEditQuadPositionAction(int GroupIndex, int LayerIndex, const std::vector<int> &vQuads, const std::vector<CPoint> &OldPos, const std::vector<CPoint> &NewPos);

	bool Undo() override;
	bool Redo() override;

private:
	void Move(int Direction);
};

struct SEditQuadPoint
{
	enum
	{
		MODIFIED_COLOR = 1 << 0,
		MODIFIED_POS_X = 1 << 1,
		MODIFIED_POS_Y = 1 << 2,
		MODIFIED_TEX_U = 1 << 3,
		MODIFIED_TEX_V = 1 << 4,

		MODIFIED_OFFSET = 1 << 5,
		MODIFIED_TEX_OFFSET = 1 << 5
	};

	CQuad m_Quad;
	int m_Modified = 0;
};

class CEditorEditQuadsPointsAction : public CEditorLayerQuadsAction<std::vector<SEditQuadPoint>>
{
public:
	CEditorEditQuadsPointsAction(int GroupIndex, int LayerIndex, const std::vector<int> &vQuads, int Points, const std::vector<SEditQuadPoint> &Old, const SEditQuadPoint &New) :
		CEditorLayerQuadsAction(IEditorAction::EType::EDIT_QUAD_VERTEX, GroupIndex, LayerIndex, vQuads, Old, {New})
	{
		m_Points = Points;
	}

	bool Undo() override;
	bool Redo() override;

private:
	int m_Points;
};

template<typename T>
class CEditorMultipleActions : public CEditorLayerAction<void *>
{
public:
	CEditorMultipleActions(std::vector<T *> vpActions) :
		CEditorLayerAction(IEditorAction::EType::EDIT_QUAD_POSITION, nullptr, -1, -1, nullptr, nullptr)
	{
		m_vpActions = vpActions;
	}
	~CEditorMultipleActions()
	{
		for(auto pAction : m_vpActions)
		{
			delete pAction;
		}
		m_vpActions.clear();
	}

	bool Undo() override
	{
		bool Undid = true;
		for(auto pAction : m_vpActions)
		{
			pAction->m_pEditor = m_pEditor;
			Undid = Undid && pAction->Undo();
		}
		return Undid;
	}

	bool Redo() override
	{
		bool Redid = true;
		for(auto pAction : m_vpActions)
		{
			pAction->m_pEditor = m_pEditor;
			Redid = Redid && pAction->Redo();
		}
		return Redid;
	}

private:
	std::vector<T *> m_vpActions;
};

class CEditorRotateQuadsAction : public CEditorLayerQuadsAction<float>
{
public:
	CEditorRotateQuadsAction(int GroupIndex, int LayerIndex, const std::vector<int> &vQuads, const float &PrevRotation, const float &NewRotation);

	bool Undo() override;
	bool Redo() override;

private:
	void RotateInternal(int Direction);
};

class CEditorMoveQuadPivotAction : public CEditorLayerQuadsAction<CPoint>
{
public:
	CEditorMoveQuadPivotAction(int GroupIndex, int LayerIndex, int Quad, const CPoint &OldPos, const CPoint &NewPos);

	bool Undo() override;
	bool Redo() override;
};

struct SQuadOperationInfo
{
	CPoint m_aPoints[4];
};

class CEditorQuadOperationAction : public CEditorLayerQuadsAction<std::vector<SQuadOperationInfo>>
{
public:
	CEditorQuadOperationAction(IEditorAction::EType Type, int GroupIndex, int LayerIndex, const std::vector<int> &vQuads, const std::vector<SQuadOperationInfo> &OldInfos, const std::vector<SQuadOperationInfo> &NewInfos);

	bool Undo() override;
	bool Redo() override;
};

struct SEditGroupInfo
{
	int m_OffsetX;
	int m_OffsetY;

	int m_ParallaxX;
	int m_ParallaxY;
	int m_CustomParallaxZoom;
	int m_ParallaxZoom;

	int m_UseClipping;
	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	int m_Order;

	enum
	{
		ORDER = 1 << 0,
		OFFSET_X = 1 << 1,
		OFFSET_Y = 1 << 2,
		PARA_X = 1 << 3,
		PARA_Y = 1 << 4,
		CUSTOM_PARA_ZOOM = 1 << 5,
		PARA_ZOOM = 1 << 6,
		USE_CLIPPING = 1 << 7,
		CLIP_X = 1 << 8,
		CLIP_Y = 1 << 9,
		CLIP_W = 1 << 10,
		CLIP_H = 1 << 11
	};
	int m_Modified;
};
class CEditorEditGroupProperty : public CEditorAction<SEditGroupInfo>
{
public:
	CEditorEditGroupProperty(int GroupIndex, const SEditGroupInfo &OldInfo, const SEditGroupInfo &NewInfo);

	bool Undo() override;
	bool Redo() override;

private:
	int m_GroupIndex;

	void ApplyInfo(const SEditGroupInfo &Info);
};


#endif