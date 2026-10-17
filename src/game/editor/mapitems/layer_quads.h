#ifndef GAME_EDITOR_MAPITEMS_LAYER_QUADS_H
#define GAME_EDITOR_MAPITEMS_LAYER_QUADS_H

#include "layer.h"

class CLayerQuads : public CLayer, public CLayerQuadsValues
{
public:
	void OnAttach(CEditorMap *pMap) override;
	explicit CLayerQuads(CEditorMap *pMap, std::uint64_t RetainedId = 0);
	CLayerQuads(const CLayerQuads &Other);
	~CLayerQuads() override;

	void Render(const CEditorMap *pRenderMap) override;
	CQuadValues *NewQuad(int x, int y, int Width, int Height);
	int SwapQuads(int Index0, int Index1);

	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) override;
	void BrushPlace(CLayer *pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

	bool IsEnvelopeUsed(int EnvelopeIndex) const override;
	bool IsImageUsed(int ImageIndex) const override;

	void VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction) override;
	void VisitEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction) override;

	void GetSize(float *pWidth, float *pHeight) override;
	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;
};

#endif
