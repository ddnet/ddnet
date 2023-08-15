#ifndef GAME_EDITOR_MAPITEMS_LAYER_SWITCH_H
#define GAME_EDITOR_MAPITEMS_LAYER_SWITCH_H

#include "layer_tiles.h"

class CLayerSwitch : public CLayerTiles
{
public:
	CLayerSwitch(int w, int h);
	~CLayerSwitch();

	CSwitchTile *m_pSwitchTile;
	unsigned char m_SwitchNumber;
	unsigned char m_SwitchDelay;

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
