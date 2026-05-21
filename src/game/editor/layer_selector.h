#ifndef GAME_EDITOR_LAYER_SELECTOR_H
#define GAME_EDITOR_LAYER_SELECTOR_H

#include "component.h"

#include <game/mapitems.h>

class CHoverTile
{
public:
	CHoverTile(int Group, int Layer, int x, int y, const CTile Tile) :
		m_Group(Group),
		m_Layer(Layer),
		m_X(x),
		m_Y(y),
		m_Tile(Tile)
	{
	}

	int m_Group;
	int m_Layer;
	int m_X;
	int m_Y;
	const CTile m_Tile;
};

class CLayerSelector : public CEditorComponent
{
	int m_SelectionOffset;
	std::vector<CHoverTile> m_vHoverTiles;

public:
	void OnInit(CEditor *pEditor) override;
	bool SelectByTile();
	void UpdateHoveredTiles();
};

#endif
