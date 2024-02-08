#ifndef GAME_EDITOR_LAYER_SELECTOR_H
#define GAME_EDITOR_LAYER_SELECTOR_H

#include "component.h"

class CLayerSelector : public CEditorComponent
{
	int m_SelectionOffset;
	int m_MatchedGroup;
	int m_MatchedLayer;
	int m_Matches;
	bool m_Found;

public:
	void Init(CEditor *pEditor) override;
	void BeforeHoverTile() override;
	void OnHoverTile(int Group, int Layer, const CTile &Tile, int x, int y) override;
	bool SelectByTile();
};

#endif
