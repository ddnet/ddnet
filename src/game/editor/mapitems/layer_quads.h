#ifndef GAME_EDITOR_MAPITEMS_LAYER_QUADS_H
#define GAME_EDITOR_MAPITEMS_LAYER_QUADS_H

#include "layer.h"

class CLayerQuads : public CLayer, public CMapItemLayerQuads
{
public:
	explicit CLayerQuads(CEditor *pEditor);
	CLayerQuads(const CLayerQuads &Other);
	~CLayerQuads() override;

	void Render(bool QuadPicker = false) override;
	CQuad *NewQuad(int x, int y, int Width, int Height);
	int SwapQuads(int Index0, int Index1);

	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) override;
	void BrushPlace(CLayer *pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

	void ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction) override;
	void ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction) override;

	void GetSize(float *pWidth, float *pHeight) override;
	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

	std::vector<CQuad> m_vQuads;
};

#endif
