#ifndef GAME_EDITOR_MAPITEMS_LAYER_TUNE_H
#define GAME_EDITOR_MAPITEMS_LAYER_TUNE_H

#include "layer_tiles.h"

class CLayerTune : public CLayerTiles
{
public:
	CLayerTune(int w, int h);
	~CLayerTune();

	CTuneTile *m_pTuneTile;
	unsigned char m_TuningNumber;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(const std::shared_ptr<CLayer> &pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, const std::shared_ptr<CLayer> &pBrush, CUIRect Rect) override;
};

#endif
