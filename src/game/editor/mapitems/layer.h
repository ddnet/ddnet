#ifndef GAME_EDITOR_MAPITEMS_LAYER_H
#define GAME_EDITOR_MAPITEMS_LAYER_H

#include "document_values.h"

#include <game/client/ui.h>
#include <game/client/ui_rect.h>
#include <game/editor/map_object.h>
#include <game/mapitems.h>

#include <memory>

using FDocumentReferenceFunction = std::function<void(CDocumentReference &Reference)>;

class CLayerGroup;

class CLayer : public CMapObject, public CLayerValues
{
public:
	void OnAttach(CEditorMap *pMap) override;
	explicit CLayer(CEditorMap *pMap, int Type, std::uint64_t RetainedId = 0);
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

	virtual void Render(const CEditorMap *pRenderMap) {}
	virtual CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) { return CUi::POPUP_KEEP_OPEN; }

	virtual bool IsEnvelopeUsed(int EnvelopeIndex) const { return false; }
	virtual bool IsImageUsed(int ImageIndex) const { return false; }
	virtual bool IsSoundUsed(int SoundIndex) const { return false; }

	virtual void VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction) {}
	virtual void VisitEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction) {}
	virtual void VisitSoundReferences(const FDocumentReferenceFunction &ReferenceFunction) {}

	virtual std::shared_ptr<CLayer> Duplicate() const = 0;
	virtual const char *TypeName() const = 0;

	virtual void GetSize(float *pWidth, float *pHeight)
	{
		*pWidth = 0;
		*pHeight = 0;
	}
	bool m_Readonly = false;
	bool m_Visible = true;
};

#endif
