#ifndef GAME_EDITOR_MAPITEMS_LAYER_QUADS_H
#define GAME_EDITOR_MAPITEMS_LAYER_QUADS_H

#include "layer.h"

class CLayerQuads : public CLayer
{
public:
	explicit CLayerQuads(CEditor *pEditor);
	CLayerQuads(const CLayerQuads &Other);
	~CLayerQuads();

	void Render(bool QuadPicker = false) override;
	CQuad *NewQuad(int x, int y, int Width, int Height);
	int SwapQuads(int Index0, int Index1);

	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) override;
	void BrushPlace(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

	void ModifyImageIndex(FIndexModifyFunction pfnFunc) override;
	void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc) override;

	void GetSize(float *pWidth, float *pHeight) override;
	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

	int m_Image;
	std::vector<CQuad> m_vQuads;
};

#endif
