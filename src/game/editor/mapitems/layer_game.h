#ifndef GAME_EDITOR_MAPITEMS_LAYER_GAME_H
#define GAME_EDITOR_MAPITEMS_LAYER_GAME_H

#include "layer_tiles.h"

class CLayerGame : public CLayerTiles
{
public:
	CLayerGame(CEditorMap *pMap, int w, int h);
	~CLayerGame() override;

	[[nodiscard]] CTile GetTile(int x, int y) const override;
	void SetTile(int x, int y, CTile Tile) override;
	[[nodiscard]] bool IsEmpty() const override;

	const char *TypeName() const override;

	CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;
};

#endif
