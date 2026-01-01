#ifndef GAME_CLIENT_COMPONENTS_MENUS_INGAME_TOUCH_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_MENUS_INGAME_TOUCH_CONTROLS_H
#include <game/client/component.h>
#include <game/client/components/touch_controls.h>
#include <game/client/ui.h>

#include <memory>

class CMenusIngameTouchControls : public CComponentInterfaces
{
public:
	static constexpr const float BUTTON_EDITOR_WIDTH = 700.0f;
	enum class EBehaviorType
	{
		BIND,
		BIND_TOGGLE,
		PREDEFINED,
		NUM_BEHAVIORS
	};

	enum class EPredefinedType
	{
		INGAME_MENU,
		EXTRA_MENU,
		EMOTICON,
		SPECTATE,
		SWAP_ACTION,
		USE_ACTION,
		JOYSTICK_ACTION,
		JOYSTICK_AIM,
		JOYSTICK_FIRE,
		JOYSTICK_HOOK,
		NUM_PREDEFINEDTYPES
	};

	// Which menu is selected.
	enum class EMenuType
	{
		MENU_FILE,
		MENU_BUTTONS,
		MENU_SETTINGS,
		MENU_PREVIEW,
		NUM_MENUS
	};
	EMenuType m_CurrentMenu = EMenuType::MENU_FILE;

	enum class ESortType
	{
		LABEL,
		X,
		Y,
		W,
		H,
		NUM_SORTS
	};
	ESortType m_SortType = ESortType::LABEL;

	enum class EElementType
	{
		LAYOUT,
		VISIBILITY,
		BEHAVIOR,
		NUM_ELEMENTS
	};
	EElementType m_EditElement = EElementType::LAYOUT;

	enum class EVisibilityType
	{
		EXCLUDE,
		INCLUDE,
		IGNORE,
		NUM_VISIBILITIES
	};
	std::array<EVisibilityType, (size_t)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> m_aCachedVisibilities; // 0:-, 1:+, 2:Ignored.

	// Mainly for passing values in popups.
	CTouchControls::CTouchButton *m_pOldSelectedButton = nullptr;
	CTouchControls::CTouchButton *m_pNewSelectedButton = nullptr;

	// Storing everything you are editing.
	CLineInputNumber m_InputX;
	CLineInputNumber m_InputY;
	CLineInputNumber m_InputW;
	CLineInputNumber m_InputH;
	CTouchControls::EButtonShape m_CachedShape;

	EBehaviorType m_EditBehaviorType = EBehaviorType::BIND;
	EPredefinedType m_PredefinedBehaviorType = EPredefinedType::EXTRA_MENU;
	int m_CachedExtraMenuNumber = 0;

	class CBehaviorElements
	{
	public:
		CLineInputBuffered<1024> m_InputCommand;
		CLineInputBuffered<1024> m_InputLabel;
		CTouchControls::CBindToggleTouchButtonBehavior::CCommand m_CachedCommands;
		CButtonContainer m_BindToggleAddButtons;
		CButtonContainer m_BindToggleDeleteButtons;
		CButtonContainer m_aLabelTypeRadios[(int)CTouchControls::CButtonLabel::EType::NUM_TYPES];

		CBehaviorElements() noexcept;
		CBehaviorElements(CBehaviorElements &&Other) noexcept;
		~CBehaviorElements();
		CBehaviorElements &operator=(CBehaviorElements &&Other) noexcept;

		CBehaviorElements &operator=(const CBehaviorElements &) = delete;
		CBehaviorElements(const CBehaviorElements &Other) = delete;

		std::string ParseLabel(const char *pLabel) const;
		void UpdateInputs();
		void UpdateLabel() { m_CachedCommands.m_Label = ParseLabel(m_InputLabel.GetString()); }
		void UpdateCommand() { m_CachedCommands.m_Command = m_InputCommand.GetString(); }
		void Reset();
	};
	std::vector<std::unique_ptr<CBehaviorElements>> m_vBehaviorElements;

	unsigned m_ColorActive = 0;
	unsigned m_ColorInactive = 0;

	// Used for creating ui elements.
	std::array<CButtonContainer, (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> m_aVisibilityIds = {};
	std::array<CButtonContainer, (unsigned)ESortType::NUM_SORTS> m_aSortHeaderIds = {};
	std::array<CButtonContainer, (unsigned)EElementType::NUM_ELEMENTS> m_aEditElementIds = {};

	// Functional variables.
	bool m_FirstEnter = true; // Execute something when first opening the editor.
	bool m_CloseMenu = false; // Decide if closing menu after the popup confirm.
	bool m_NeedUpdatePreview = true; // Whether to reload the button being previewed.
	bool m_NeedSort = true; // Whether to sort all previewed buttons.
	bool m_NeedFilter = false; // Whether to exclude some buttons from preview.
	std::vector<CTouchControls::CTouchButton *> m_vpButtons;
	std::vector<CTouchControls::CTouchButton *> m_vpMutableButtons;
	CLineInputBuffered<1024> m_FilterInput;
	int m_SelectedPreviewButtonIndex = -1;

	void RenderTouchButtonEditor(CUIRect MainView);
	bool RenderLayoutSettingBlock(CUIRect Block);
	bool RenderBehaviorSettingBlock(CUIRect Block);
	bool RenderVisibilitySettingBlock(CUIRect Block);
	void RenderTouchButtonBrowser(CUIRect MainView);
	void RenderPreviewButton(CUIRect MainView);

	void RenderSelectingTab(CUIRect SelectingTab);
	void RenderConfigSettings(CUIRect MainView);
	void RenderPreviewSettings(CUIRect MainView);
	void RenderTouchControlsEditor(CUIRect MainView);

	// Confirm, Cancel only decide if saving cached settings.
	void DoPopupType(CTouchControls::CPopupParam PopupParam);
	void ChangeSelectedButtonWhileHavingUnsavedChanges() const;
	void NoSpaceForOverlappingButton() const;
	void SelectedButtonNotVisible();

	// Getter and setters.
	bool UnsavedChanges() const;
	void SetUnsavedChanges(bool UnsavedChanges);

	// Convenient functions.
	bool CheckCachedSettings() const;
	void ResetCachedSettings();
	void CacheAllSettingsFromTarget(CTouchControls::CTouchButton *pTargetButton);
	void SaveCachedSettingsToTarget(CTouchControls::CTouchButton *pTargetButton) const;
	void SetPosInputs(CTouchControls::CUnitRect MyRect);
	CTouchControls::CUnitRect GetPosInputs() const;
	void InputPosFunction(CLineInputNumber *pInput);
	void UpdateSampleButton();
	void ResetButtonPointers();
	bool NewButton(CTouchControls::CUnitRect Rect, CTouchControls::EButtonShape Shape);
	void ResolveIssues();
	int CalculatePredefinedType(const char *pType) const;
	std::string DetermineTouchButtonCommandLabel(CTouchControls::CTouchButton *pButton) const;
	const char **VisibilityNames() const;
	const char **PredefinedNames() const;
	const char *HelpMessageForBehaviorType(EBehaviorType Type) const;
	const char *HelpMessageForPredefinedType(EPredefinedType Type) const;
	const char *HelpMessageForVisibilityType(CTouchControls::EButtonVisibility Type) const;

	class CBehaviorFactoryEditor
	{
	public:
		const char *m_pId;
		std::function<std::unique_ptr<CTouchControls::CPredefinedTouchButtonBehavior>()> m_Factory;
	};

	static const CBehaviorFactoryEditor BEHAVIOR_FACTORIES_EDITOR[10];
};

#endif
