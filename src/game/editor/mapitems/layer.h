#ifndef GAME_EDITOR_MAPITEMS_LAYER_H
#define GAME_EDITOR_MAPITEMS_LAYER_H

#include <base/system.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>
#include <game/mapitems.h>

#include <memory>

using FIndexModifyFunction = std::function<void(int *pIndex)>;

class CLayerGroup;

class CLayer
{
public:
	class CEditor *m_pEditor;
	class IGraphics *Graphics();
	class ITextRender *TextRender();

	explicit CLayer(CEditor *pEditor)
	{
		m_Type = LAYERTYPE_INVALID;
		str_copy(m_aName, "(invalid)");
		m_Visible = true;
		m_Readonly = false;
		m_Flags = 0;
		m_pEditor = pEditor;
	}

	CLayer(const CLayer &Other)
	{
		str_copy(m_aName, Other.m_aName);
		m_Flags = Other.m_Flags;
		m_pEditor = Other.m_pEditor;
		m_Type = Other.m_Type;
		m_Visible = true;
		m_Readonly = false;
	}

	virtual ~CLayer()
	{
	}

	virtual void BrushSelecting(CUIRect Rect) {}
	virtual int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) { return 0; }
	virtual void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) {}
	virtual void BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) {}
	virtual void BrushPlace(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) {}
	virtual void BrushFlipX() {}
	virtual void BrushFlipY() {}
	virtual void BrushRotate(float Amount) {}

	virtual bool IsEntitiesLayer() const { return false; }

	virtual void Render(bool Tileset = false) {}
	virtual CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) { return CUi::POPUP_KEEP_OPEN; }

	virtual void ModifyImageIndex(FIndexModifyFunction pfnFunc) {}
	virtual void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc) {}
	virtual void ModifySoundIndex(FIndexModifyFunction pfnFunc) {}

	virtual std::shared_ptr<CLayer> Duplicate() const = 0;
	virtual const char *TypeName() const = 0;

	virtual void GetSize(float *pWidth, float *pHeight)
	{
		*pWidth = 0;
		*pHeight = 0;
	}

	char m_aName[12];
	int m_Type;
	int m_Flags;

	bool m_Readonly;
	bool m_Visible;
};

#endif
