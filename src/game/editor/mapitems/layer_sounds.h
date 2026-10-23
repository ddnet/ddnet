#ifndef GAME_EDITOR_MAPITEMS_LAYER_SOUNDS_H
#define GAME_EDITOR_MAPITEMS_LAYER_SOUNDS_H

#include "layer.h"

class CLayerSounds : public CLayer, public CLayerSoundsValues
{
public:
	void OnAttach(CEditorMap *pMap) override;
	explicit CLayerSounds(CEditorMap *pMap, std::uint64_t RetainedId = 0);
	CLayerSounds(const CLayerSounds &Other);
	~CLayerSounds() override;

	void Render(const CEditorMap *pRenderMap) override;
	CSoundSourceValues *NewSource(int x, int y);

	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) override;
	void BrushPlace(CLayer *pBrush, vec2 WorldPos) override;

	CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

	bool IsEnvelopeUsed(int EnvelopeIndex) const override;
	bool IsSoundUsed(int SoundIndex) const override;

	void VisitEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction) override;
	void VisitSoundReferences(const FDocumentReferenceFunction &ReferenceFunction) override;

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;
};

#endif
