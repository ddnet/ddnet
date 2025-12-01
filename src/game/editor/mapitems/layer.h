#ifndef GAME_EDITOR_MAPITEMS_LAYER_H
#define GAME_EDITOR_MAPITEMS_LAYER_H

#include <game/client/ui.h>
#include <game/client/ui_rect.h>
#include <game/editor/map_object.h>
#include <game/mapitems.h>

#include <memory>

using FIndexModifyFunction = std::function<void(int *pIndex)>;

class CLayerGroup;

class CLayer : public CMapObject
{
public:
	explicit CLayer(CEditorMap *pMap, int Type);
	CLayer(const CLayer &Other);

	virtual void BrushSelecting(CUIRect Rect) {}
	virtual int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) { return 0; }
	virtual void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) {}
	virtual void BrushDraw(CLayer *pBrush, vec2 WorldPos) {}
	virtual void BrushPlace(CLayer *pBrush, vec2 WorldPos) {}
	virtual void BrushFlipX() {}
	virtual void BrushFlipY() {}
	virtual void BrushRotate(float Amount) {}

	virtual bool IsEntitiesLayer() const { return false; }

	virtual void Render(bool Tileset = false) {}
	virtual CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) { return CUi::POPUP_KEEP_OPEN; }

	virtual void ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction) {}
	virtual void ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction) {}
	virtual void ModifySoundIndex(const FIndexModifyFunction &IndexModifyFunction) {}

	virtual std::shared_ptr<CLayer> Duplicate() const = 0;
	virtual const char *TypeName() const = 0;

	virtual void GetSize(float *pWidth, float *pHeight)
	{
		*pWidth = 0;
		*pHeight = 0;
	}
	int m_Type;
	char m_aName[12] = "";
	int m_Flags = 0;

	bool m_Readonly = false;
	bool m_Visible = true;
};

#endif
