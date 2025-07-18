#ifndef GAME_EDITOR_MAPITEMS_LAYER_FRONT_H
#define GAME_EDITOR_MAPITEMS_LAYER_FRONT_H

#include "layer_tiles.h"

class CLayerFront : public CLayerTiles
{
public:
	CLayerFront(CEditor *pEditor, int w, int h);

	void SetTile(int x, int y, CTile Tile) override;
	void Resize(int NewW, int NewH) override;
	[[nodiscard]] bool IsEmpty() const override;

	const char *TypeName() const override;
};

#endif
