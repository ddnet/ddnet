#ifndef GAME_EDITOR_MAPITEMS_LAYER_SPEEDUP_H
#define GAME_EDITOR_MAPITEMS_LAYER_SPEEDUP_H

#include "layer_tiles.h"

class CLayerSpeedup : public CLayerTiles
{
public:
	CLayerSpeedup(CEditor *pEditor, int w, int h);
	~CLayerSpeedup();

	CSpeedupTile *m_pSpeedupTile;
	int m_SpeedupForce;
	int m_SpeedupMaxSpeed;
	int m_SpeedupAngle;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
};

#endif
