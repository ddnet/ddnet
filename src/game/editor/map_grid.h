#ifndef GAME_EDITOR_MAP_GRID_H
#define GAME_EDITOR_MAP_GRID_H

#include "component.h"

class CMapGrid : public CEditorComponent
{
public:
	void OnReset() override;
	void OnRender(CUIRect View) override;

	void SnapToGrid(float &x, float &y);
	int GridLineDistance() const;

	/**
	 * Returns wether the grid is rendered.
	 */
	bool IsEnabled() const;

	void Toggle();

	bool Factor() const;
	void ResetFactor();
	void IncreaseFactor();
	void DecreaseFactor();

private:
	bool m_GridActive;
	int m_GridFactor;
};

#endif
