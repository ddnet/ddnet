#ifndef GAME_EDITOR_MAP_GRID_H
#define GAME_EDITOR_MAP_GRID_H

#include "component.h"

#include <game/client/ui.h>

class CMapGrid : public CEditorComponent
{
public:
	void OnReset() override;
	void OnRender(CUIRect View) override;

	void SnapToGrid(float &x, float &y) const;
	int GridLineDistance() const;

	/**
	 * Returns whether the grid is rendered.
	 */
	bool IsEnabled() const;

	void Toggle();

	int Factor() const;
	void SetFactor(int Factor);

	void DoSettingsPopup(vec2 Position);

private:
	bool m_GridActive;
	int m_GridFactor;

	SPopupMenuId m_PopupGridSettingsId;
	static CUi::EPopupMenuFunctionResult PopupGridSettings(void *pContext, CUIRect View, bool Active);
};

#endif
