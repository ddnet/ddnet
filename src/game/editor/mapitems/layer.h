#ifndef GAME_EDITOR_MAPITEMS_LAYER_H
#define GAME_EDITOR_MAPITEMS_LAYER_H

#include <game/editor/component.h>

class CLayerGroup;

using FIndexModifyFunction = std::function<void(int *pIndex)>;

class CLayer : public CEditorComponent
{
public:
	CLayer();
	CLayer(const CLayer &Other);

	virtual void BrushSelecting(CUIRect Rect) {}
	virtual int BrushGrab(const std::shared_ptr<CLayerGroup> &pBrush, CUIRect Rect) { return 0; }
	virtual void FillSelection(bool Empty, const std::shared_ptr<CLayer> &pBrush, CUIRect Rect) {}
	virtual void BrushDraw(const std::shared_ptr<CLayer> &pBrush, float x, float y) {}
	virtual void BrushPlace(const std::shared_ptr<CLayer> &pBrush, float x, float y) {}
	virtual void BrushFlipX() {}
	virtual void BrushFlipY() {}
	virtual void BrushRotate(float Amount) {}

	virtual bool IsEntitiesLayer() const { return false; }

	virtual void Render(bool Tileset = false) {}
	virtual CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) { return CUI::POPUP_KEEP_OPEN; }

	virtual void ModifyImageIndex(const FIndexModifyFunction &pfnFunc) {}
	virtual void ModifyEnvelopeIndex(const FIndexModifyFunction &pfnFunc) {}
	virtual void ModifySoundIndex(const FIndexModifyFunction &pfnFunc) {}

	virtual std::shared_ptr<CLayer> Duplicate() const = 0;

	virtual void GetSize(float *pWidth, float *pHeight);

	char m_aName[12];
	int m_Type;
	int m_Flags;

	bool m_Readonly;
	bool m_Visible;
};

#endif
