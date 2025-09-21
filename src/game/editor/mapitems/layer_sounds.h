#ifndef GAME_EDITOR_MAPITEMS_LAYER_SOUNDS_H
#define GAME_EDITOR_MAPITEMS_LAYER_SOUNDS_H

#include "layer.h"

class CLayerSounds : public CLayer
{
public:
	explicit CLayerSounds(CEditor *pEditor);
	CLayerSounds(const CLayerSounds &Other);
	~CLayerSounds() override;

	void Render(bool Tileset = false) override;
	CSoundSource *NewSource(int x, int y);

	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) override;
	void BrushPlace(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) override;

	CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

	void ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction) override;
	void ModifySoundIndex(const FIndexModifyFunction &IndexModifyFunction) override;

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

	std::vector<CSoundSource> m_vSources;
	CMapItemLayerSounds m_LayerSounds;
};

#endif
