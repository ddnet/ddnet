/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_SETTINGS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_MENUS_SETTINGS_CONTROLS_H

#include <game/client/component.h>
#include <game/client/ui.h>

#include <vector>

class CMenusSettingsControls : public CComponentInterfaces
{
public:
	void Render(CUIRect MainView);

private:
	void RenderSettingsBinds(int Start, int Stop, CUIRect View);
	float RenderSettingsJoystick(CUIRect View);
	void RenderJoystickAxisPicker(CUIRect View);
	void RenderJoystickBar(const CUIRect *pRect, float Current, float Tolerance, bool Active);

	std::vector<CButtonContainer> m_vButtonContainersJoystickAbsolute = {{}, {}};
};

#endif
