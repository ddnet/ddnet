#ifndef GAME_EDITOR_MAPITEMS_LAYER_TELE_H
#define GAME_EDITOR_MAPITEMS_LAYER_TELE_H

#include "layer_tiles.h"

class CLayerTele : public CLayerTiles, public CLayerTeleValues
{
public:
	CLayerTele(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId = 0);
	CLayerTele(const CLayerTele &Other);
	~CLayerTele() override;

	CRenderTileSource<CTeleTile> TeleTilesForRender() const
	{
		return CRenderTileSource<CTeleTile>(m_TeleTiles);
	}
	unsigned char m_TeleNumber{};
	unsigned char m_TeleCheckpointNumber{};

	void Resize(int NewW, int NewH) override;
	void Shift(EShiftDirection Direction) override;
	[[nodiscard]] bool IsEmpty() const override;
	void BrushDraw(CLayer *pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
	int FindNextFreeNumber(bool Checkpoint) const;
	bool ContainsElementWithId(int Id, bool Checkpoint) const;
	void GetPos(int Number, int Offset, int &TeleX, int &TeleY);

	int m_GotoTeleOffset;
	ivec2 m_GotoTeleLastPos;

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

private:
	friend class CLayerTiles;
};

#endif
