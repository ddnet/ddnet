#ifndef GAME_EDITOR_MAPITEMS_LAYER_TELE_H
#define GAME_EDITOR_MAPITEMS_LAYER_TELE_H

#include "layer_tiles.h"

class CLayerTele : public CLayerTiles
{
public:
	CLayerTele(int w, int h);
	~CLayerTele();

	CTeleTile *m_pTeleTile;
	unsigned char m_TeleNum;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(const std::shared_ptr<CLayer> &pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, const std::shared_ptr<CLayer> &pBrush, CUIRect Rect) override;
	virtual bool ContainsElementWithId(int Id);
};

#endif
