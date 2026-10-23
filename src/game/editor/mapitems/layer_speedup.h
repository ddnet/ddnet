#ifndef GAME_EDITOR_MAPITEMS_LAYER_SPEEDUP_H
#define GAME_EDITOR_MAPITEMS_LAYER_SPEEDUP_H

#include "layer_tiles.h"

class CLayerSpeedup : public CLayerTiles, public CLayerSpeedupValues
{
public:
	CLayerSpeedup(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId = 0);
	CLayerSpeedup(const CLayerSpeedup &Other);
	~CLayerSpeedup() override;

	CRenderTileSource<CSpeedupTile> SpeedupTilesForRender() const
	{
		return CRenderTileSource<CSpeedupTile>(m_SpeedupTiles);
	}
	int m_SpeedupForce{};
	int m_SpeedupMaxSpeed{};
	int m_SpeedupAngle{};

	void Resize(int NewW, int NewH) override;
	void Shift(EShiftDirection Direction) override;
	[[nodiscard]] bool IsEmpty() const override;
	void BrushDraw(CLayer *pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;
};

#endif
