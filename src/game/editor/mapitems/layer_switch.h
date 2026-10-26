#ifndef GAME_EDITOR_MAPITEMS_LAYER_SWITCH_H
#define GAME_EDITOR_MAPITEMS_LAYER_SWITCH_H

#include "layer_tiles.h"

class CLayerSwitch : public CLayerTiles, public CLayerSwitchValues
{
public:
	CLayerSwitch(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId = 0);
	CLayerSwitch(const CLayerSwitch &Other);
	~CLayerSwitch() override;

	CRenderTileSource<CSwitchTile> SwitchTilesForRender() const
	{
		return CRenderTileSource<CSwitchTile>(m_SwitchTiles);
	}
	unsigned char m_SwitchNumber{};
	unsigned char m_SwitchDelay{};

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
	void GetPos(int Number, int Offset, ivec2 &SwitchPos);

	int m_GotoSwitchOffset;
	ivec2 m_GotoSwitchLastPos;

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;
};

#endif
