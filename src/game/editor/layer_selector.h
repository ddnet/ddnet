#ifndef GAME_EDITOR_LAYER_SELECTOR_H
#define GAME_EDITOR_LAYER_SELECTOR_H

#include "component.h"

class CLayerSelector : public CEditorComponent
{
	int m_SelectionOffset;

public:
	void OnInit(CEditor *pEditor) override;
	bool SelectByTile();
};

#endif
