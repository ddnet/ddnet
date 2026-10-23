#ifndef GAME_EDITOR_MAPITEMS_LAYER_TUNE_H
#define GAME_EDITOR_MAPITEMS_LAYER_TUNE_H

#include "layer_tiles.h"

class CLayerTune : public CLayerTiles, public CLayerTuneValues
{
public:
	CLayerTune(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId = 0);
	CLayerTune(const CLayerTune &Other);
	~CLayerTune() override;

	CRenderTileSource<CTuneTile> TuneTilesForRender() const
	{
		return CRenderTileSource<CTuneTile>(m_TuneTiles);
	}
	unsigned char m_TuningNumber{};

	void Resize(int NewW, int NewH) override;
	void Shift(EShiftDirection Direction) override;
	[[nodiscard]] bool IsEmpty() const override;
	void BrushDraw(CLayer *pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
	int FindNextFreeNumber() const;
	bool ContainsElementWithId(int Id) const;
	void GetPos(int Number, int Offset, ivec2 &Pos);

	int m_GotoTuneOffset;
	ivec2 m_GotoTuneLastPos;

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;
};

#endif
