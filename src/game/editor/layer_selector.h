#ifndef GAME_EDITOR_LAYER_SELECTOR_H
#define GAME_EDITOR_LAYER_SELECTOR_H

#include "component.h"

class CHoverTile
{
public:
	CHoverTile(int Group, int Layer) :
		m_Group(Group),
		m_Layer(Layer)
	{
	}

	int m_Group;
	int m_Layer;
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
