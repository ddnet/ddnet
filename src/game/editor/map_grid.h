#ifndef GAME_EDITOR_MAP_GRID_H
#define GAME_EDITOR_MAP_GRID_H

#include "component.h"

#include <game/client/ui.h>

class CMapGrid : public CEditorComponent
{
public:
	class CState
	{
	public:
		bool m_GridActive;
		int m_GridFactor;

		void Reset();
	};

	void OnRender(CUIRect View) override;

	void SnapToGrid(vec2 &Position) const;
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
	SPopupMenuId m_PopupGridSettingsId;
	static CUi::EPopupMenuFunctionResult PopupGridSettings(void *pContext, CUIRect View, bool Active);
};

#endif
