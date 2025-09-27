/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_SETTINGS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_MENUS_SETTINGS_CONTROLS_H

#include <game/client/component.h>
#include <game/client/components/binds.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

#include <vector>

enum class EBindOptionGroup
{
	MOVEMENT,
	WEAPON,
	VOTING,
	CHAT,
	DUMMY,
	MISCELLANEOUS,
	CUSTOM,
	NUM,
};

class CBindSlotUiElement
{
public:
	CBindSlot m_Bind;
	CButtonContainer m_KeyReaderButton;
	CButtonContainer m_KeyResetButton;
	bool m_ToBeDeleted = false;

	bool operator<(const CBindSlotUiElement &Other) const;
};

class CBindOption
{
public:
	EBindOptionGroup m_Group;
	const char *m_pLabel;
	std::string m_Command;
	std::vector<CBindSlotUiElement> m_vCurrentBinds;
	CButtonContainer m_AddBindButtonContainer;
	char m_TooltipButtonId;
	bool m_AddNewBind = false;
	bool m_AddNewBindActivate = false;
	bool m_ToBeDeleted = false;

	std::vector<CBindSlotUiElement>::iterator GetBindSlotElement(const CBindSlot &BindSlot);
	bool MatchesSearch(const char *pSearch) const;
};

class CMenusSettingsControls : public CComponentInterfaces
{
public:
	void OnInterfacesInit(CGameClient *pClient) override;
	void Render(CUIRect MainView);

private:
	bool m_aBindGroupExpanded[(int)EBindOptionGroup::NUM];
	CButtonContainer m_aBindGroupExpandButtons[(int)EBindOptionGroup::NUM];
	std::vector<CBindOption> m_vBindOptions;
	size_t m_NumPredefinedBindOptions;
	void UpdateBindOptions();

	CScrollRegion m_SettingsScrollRegion;
	CButtonContainer m_ResetToDefaultButton;
	CLineInputBuffered<128> m_FilterInput;
	int m_CurrentSearchMatch = 0;
	std::vector<int> m_vSearchMatches;
	bool m_SearchMatchReveal = false;
	void UpdateSearchMatches();

	void RenderSettingsBlock(float Height, CUIRect *pParentRect, const char *pTitle,
		bool *pExpanded, CButtonContainer *pExpandButton, const std::function<void(CUIRect Rect)> &RenderContentFunction);

	void RenderSettingsBindsBlock(EBindOptionGroup Group, CUIRect *pParentRect, const char *pTitle);
	float MeasureSettingsBindsHeight(EBindOptionGroup Group) const;
	void RenderSettingsBinds(EBindOptionGroup Group, CUIRect View);

	float MeasureSettingsMouseHeight() const;
	void RenderSettingsMouse(CUIRect View);

	std::vector<CButtonContainer> m_vJoystickIngameModeButtonContainers = {{}, {}};
	char m_aaJoystickAxisCheckboxIds[NUM_JOYSTICK_AXES][2]; // 2 for X and Y buttons
	CScrollRegion m_JoystickDropDownScrollRegion;
	CUi::SDropDownState m_JoystickDropDownState;
	float MeasureSettingsJoystickHeight() const;
	void RenderSettingsJoystick(CUIRect View);
	void RenderJoystickAxisPicker(CUIRect View);
	void RenderJoystickBar(const CUIRect *pRect, float Current, float Tolerance, bool Active);
};

#endif
