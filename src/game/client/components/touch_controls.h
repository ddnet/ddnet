#ifndef GAME_CLIENT_COMPONENTS_TOUCH_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_TOUCH_CONTROLS_H

#include <base/vmath.h>

#include <engine/input.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class CJsonWriter;
typedef struct _json_value json_value;

class CTouchControls : public CComponent
{
public:
	enum class EDirectTouchIngameMode
	{
		DISABLED,
		ACTION,
		AIM,
		FIRE,
		HOOK,
		NUM_STATES
	};
	enum class EDirectTouchSpectateMode
	{
		DISABLED,
		AIM,
		NUM_STATES
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnWindowResize() override;
	bool OnTouchState(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates) override;
	void OnRender() override;

	bool LoadConfigurationFromFile(int StorageType);
	bool LoadConfigurationFromClipboard();
	bool SaveConfigurationToFile();
	void SaveConfigurationToClipboard();

	EDirectTouchIngameMode DirectTouchIngame() const { return m_DirectTouchIngame; }
	void SetDirectTouchIngame(EDirectTouchIngameMode DirectTouchIngame)
	{
		m_DirectTouchIngame = DirectTouchIngame;
		m_EditingChanges = true;
	}
	EDirectTouchSpectateMode DirectTouchSpectate() const { return m_DirectTouchSpectate; }
	void SetDirectTouchSpectate(EDirectTouchSpectateMode DirectTouchSpectate)
	{
		m_DirectTouchSpectate = DirectTouchSpectate;
		m_EditingChanges = true;
	}
	bool IsEditingActive() const { return m_EditingActive; }
	void SetEditingActive(bool EditingActive) { m_EditingActive = EditingActive; }
	bool HasEditingChanges() const { return m_EditingChanges; }
	void SetEditingChanges(bool EditingChanges) { m_EditingChanges = EditingChanges; }

private:
	static constexpr const char *const DIRECT_TOUCH_INGAME_MODE_NAMES[(int)EDirectTouchIngameMode::NUM_STATES] = {"disabled", "action", "aim", "fire", "hook"};
	static constexpr const char *const DIRECT_TOUCH_SPECTATE_MODE_NAMES[(int)EDirectTouchSpectateMode::NUM_STATES] = {"disabled", "aim"};

	enum class EButtonShape
	{
		RECT,
		CIRCLE,
		NUM_SHAPES
	};

	static constexpr const char *const SHAPE_NAMES[(int)EButtonShape::NUM_SHAPES] = {"rect", "circle"};

	enum class EButtonVisibility
	{
		INGAME,
		ZOOM_ALLOWED,
		VOTE_ACTIVE,
		DUMMY_ALLOWED,
		DUMMY_CONNECTED,
		RCON_AUTHED,
		DEMO_PLAYER,
		EXTRA_MENU_1,
		EXTRA_MENU_2,
		EXTRA_MENU_3,
		EXTRA_MENU_4,
		EXTRA_MENU_5,
		NUM_VISIBILITIES
	};

	class CButtonVisibility
	{
	public:
		EButtonVisibility m_Type;
		bool m_Parity;

		CButtonVisibility(EButtonVisibility Type, bool Parity) :
			m_Type(Type), m_Parity(Parity) {}
	};

	class CButtonVisibilityData
	{
	public:
		const char *m_pId;
		std::function<bool()> m_Function;
	};

	CButtonVisibilityData m_aVisibilityFunctions[(int)EButtonVisibility::NUM_VISIBILITIES];

	enum
	{
		ACTION_AIM,
		ACTION_FIRE,
		ACTION_HOOK,
		NUM_ACTIONS
	};

	class CButtonLabel
	{
	public:
		enum class EType
		{
			/**
			 * Label is used as is.
			 */
			PLAIN,
			/**
			 * Label is localized. Only usable for default button labels for which there must be
			 * corresponding `Localizable`-calls in code and string in the translation files.
			 */
			LOCALIZED,
			/**
			 * Icon font is used for the label.
			 */
			ICON,
			/**
			 * Number of label types.
			 */
			NUM_TYPES
		};

		EType m_Type;
		const char *m_pLabel;
	};

	static constexpr const char *const LABEL_TYPE_NAMES[(int)CButtonLabel::EType::NUM_TYPES] = {"plain", "localized", "icon"};

	class CUnitRect
	{
	public:
		int m_X;
		int m_Y;
		int m_W;
		int m_H;
	};

	class CTouchButtonBehavior;

	class CTouchButton
	{
	public:
		CTouchButton(CTouchControls *pTouchControls);
		CTouchButton(CTouchButton &&Other) noexcept;
		CTouchButton(const CTouchButton &Other) = delete;

		CTouchButton &operator=(const CTouchButton &Other) = delete;
		CTouchButton &operator=(CTouchButton &&Other) noexcept;

		CTouchControls *m_pTouchControls;

		CUnitRect m_UnitRect;
		CUIRect m_ScreenRect;

		EButtonShape m_Shape;
		int m_BackgroundCorners; // only used with EButtonShape::RECT

		std::vector<CButtonVisibility> m_vVisibilities;
		std::unique_ptr<CTouchButtonBehavior> m_pBehavior;

		bool m_VisibilityCached;
		std::chrono::nanoseconds m_VisibilityStartTime;

		void UpdatePointers();
		void UpdateScreenFromUnitRect();
		void UpdateBackgroundCorners();

		vec2 ClampTouchPosition(vec2 TouchPosition) const;
		bool IsInside(vec2 TouchPosition) const;
		void UpdateVisibility();
		bool IsVisible() const;
		void Render() const;
		void WriteToConfiguration(CJsonWriter *pWriter);
	};

	class CTouchButtonBehavior
	{
	public:
		CTouchButton *m_pTouchButton;
		CTouchControls *m_pTouchControls;

		bool m_Active; // variables below must only be used when active
		IInput::CTouchFinger m_Finger;
		vec2 m_ActivePosition;
		vec2 m_AccumulatedDelta;
		std::chrono::nanoseconds m_ActivationStartTime;

		virtual ~CTouchButtonBehavior() = default;
		virtual void Init(CTouchButton *pTouchButton);

		void Reset();
		void SetActive(const IInput::CTouchFingerState &FingerState);
		void SetInactive();
		bool IsActive() const;
		bool IsActive(const IInput::CTouchFinger &Finger) const;

		virtual CButtonLabel GetLabel() const = 0;
		virtual void OnActivate() {}
		virtual void OnDeactivate() {}
		virtual void OnUpdate() {}
		virtual void WriteToConfiguration(CJsonWriter *pWriter) = 0;
	};

	/**
	 * Abstract class for predefined behaviors.
	 *
	 * Subclasses must implemented the concrete behavior and provide the label.
	 */
	class CPredefinedTouchButtonBehavior : public CTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_TYPE = "predefined";

		CPredefinedTouchButtonBehavior(const char *pId) :
			m_pId(pId) {}

		/**
		 * Implements the serialization for predefined behaviors. Subclasses
		 * may override this, but they should call the parent function first.
		 */
		void WriteToConfiguration(CJsonWriter *pWriter) override;

	private:
		const char *m_pId;
	};

	class CIngameMenuTouchButtonBehavior : public CPredefinedTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "ingame-menu";

		CIngameMenuTouchButtonBehavior() :
			CPredefinedTouchButtonBehavior(BEHAVIOR_ID) {}

		CButtonLabel GetLabel() const override;
		void OnDeactivate() override;
	};

	class CExtraMenuTouchButtonBehavior : public CPredefinedTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "extra-menu";

		CExtraMenuTouchButtonBehavior(int Number);

		CButtonLabel GetLabel() const override;
		void OnDeactivate() override;
		void WriteToConfiguration(CJsonWriter *pWriter) override;

	private:
		int m_Number;
		char m_aLabel[16];
	};

	class CEmoticonTouchButtonBehavior : public CPredefinedTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "emoticon";

		CEmoticonTouchButtonBehavior() :
			CPredefinedTouchButtonBehavior(BEHAVIOR_ID) {}

		CButtonLabel GetLabel() const override;
		void OnDeactivate() override;
	};

	class CSpectateTouchButtonBehavior : public CPredefinedTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "spectate";

		CSpectateTouchButtonBehavior() :
			CPredefinedTouchButtonBehavior(BEHAVIOR_ID) {}

		CButtonLabel GetLabel() const override;
		void OnDeactivate() override;
	};

	class CSwapActionTouchButtonBehavior : public CPredefinedTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "swap-action";

		CSwapActionTouchButtonBehavior() :
			CPredefinedTouchButtonBehavior(BEHAVIOR_ID) {}

		CButtonLabel GetLabel() const override;
		void OnActivate() override;
		void OnDeactivate() override;

	private:
		int m_ActiveAction = NUM_ACTIONS;
	};

	class CUseActionTouchButtonBehavior : public CPredefinedTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "use-action";

		CUseActionTouchButtonBehavior() :
			CPredefinedTouchButtonBehavior(BEHAVIOR_ID) {}

		CButtonLabel GetLabel() const override;
		void OnActivate() override;
		void OnDeactivate() override;

	private:
		int m_ActiveAction = NUM_ACTIONS;
	};

	class CJoystickTouchButtonBehavior : public CPredefinedTouchButtonBehavior
	{
	public:
		CJoystickTouchButtonBehavior(const char *pId) :
			CPredefinedTouchButtonBehavior(pId) {}

		CButtonLabel GetLabel() const override;
		void OnActivate() override;
		void OnDeactivate() override;
		void OnUpdate() override;
		int ActiveAction() const { return m_ActiveAction; }
		virtual int SelectedAction() const = 0;

	private:
		int m_ActiveAction = NUM_ACTIONS;
	};

	class CJoystickActionTouchButtonBehavior : public CJoystickTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "joystick-action";

		CJoystickActionTouchButtonBehavior() :
			CJoystickTouchButtonBehavior(BEHAVIOR_ID) {}

		void Init(CTouchButton *pTouchButton) override;
		int SelectedAction() const override;
	};

	class CJoystickAimTouchButtonBehavior : public CJoystickTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "joystick-aim";

		CJoystickAimTouchButtonBehavior() :
			CJoystickTouchButtonBehavior(BEHAVIOR_ID) {}

		int SelectedAction() const override;
	};

	class CJoystickFireTouchButtonBehavior : public CJoystickTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "joystick-fire";

		CJoystickFireTouchButtonBehavior() :
			CJoystickTouchButtonBehavior(BEHAVIOR_ID) {}

		int SelectedAction() const override;
	};

	class CJoystickHookTouchButtonBehavior : public CJoystickTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_ID = "joystick-hook";

		CJoystickHookTouchButtonBehavior() :
			CJoystickTouchButtonBehavior(BEHAVIOR_ID) {}

		int SelectedAction() const override;
	};

	/**
	 * Generic behavior implementation that executes a console command like a bind.
	 */
	class CBindTouchButtonBehavior : public CTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_TYPE = "bind";

		CBindTouchButtonBehavior(const char *pLabel, CButtonLabel::EType LabelType, const char *pCommand) :
			m_Label(pLabel),
			m_LabelType(LabelType),
			m_Command(pCommand) {}

		CButtonLabel GetLabel() const override;
		void OnActivate() override;
		void OnDeactivate() override;
		void OnUpdate() override;
		void WriteToConfiguration(CJsonWriter *pWriter) override;

	private:
		std::string m_Label;
		CButtonLabel::EType m_LabelType;
		std::string m_Command;

		bool m_Repeating = false;
		std::chrono::nanoseconds m_LastUpdateTime;
		std::chrono::nanoseconds m_AccumulatedRepeatingTime;
	};

	/**
	 * Generic behavior implementation that switches between executing one of two or more console commands.
	 */
	class CBindToggleTouchButtonBehavior : public CTouchButtonBehavior
	{
	public:
		static constexpr const char *const BEHAVIOR_TYPE = "bind-toggle";

		class CCommand
		{
		public:
			std::string m_Label;
			CButtonLabel::EType m_LabelType;
			std::string m_Command;

			CCommand(const char *pLabel, CButtonLabel::EType LabelType, const char *pCommand) :
				m_Label(pLabel),
				m_LabelType(LabelType),
				m_Command(pCommand) {}
		};

		CBindToggleTouchButtonBehavior(std::vector<CCommand> &&vCommands) :
			m_vCommands(std::move(vCommands)) {}

		CButtonLabel GetLabel() const override;
		void OnActivate() override;
		void WriteToConfiguration(CJsonWriter *pWriter) override;

	private:
		std::vector<CCommand> m_vCommands;
		size_t m_ActiveCommandIndex = 0;
	};

	/**
	 * Mode of direct touch input while ingame.
	 *
	 * Saved to the touch controls configuration.
	 */
	EDirectTouchIngameMode m_DirectTouchIngame = EDirectTouchIngameMode::ACTION;

	/**
	 * Mode of direct touch input while spectating.
	 *
	 * Saved to the touch controls configuration.
	 */
	EDirectTouchSpectateMode m_DirectTouchSpectate = EDirectTouchSpectateMode::AIM;

	/**
	 * All touch buttons.
	 *
	 * Saved to the touch controls configuration.
	 */
	std::vector<CTouchButton> m_vTouchButtons;

	/**
	 * The activation states of the different extra menus which are toggle by the extra menu button behavior.
	 */
	bool m_aExtraMenuActive[(int)EButtonVisibility::EXTRA_MENU_5 - (int)EButtonVisibility::EXTRA_MENU_1 + 1] = {false};

	/**
	 * The currently selected action which is used for direct touch and is changed and used by some button behaviors.
	 */
	int m_ActionSelected = ACTION_FIRE;

	/**
	 * The action that was last activated with direct touch input, which will determine the finger that will
	 * be used to update the mouse position from direct touch input.
	 */
	int m_DirectTouchLastAction = ACTION_FIRE;

	class CActionState
	{
	public:
		bool m_Active = false;
		IInput::CTouchFinger m_Finger;
	};

	/**
	 * The states of the different actions for direct touch input.
	 */
	CActionState m_aDirectTouchActionStates[NUM_ACTIONS];

	/**
	 * A pointer to the action joystick, if any exists in the current configuration, or `nullptr` if none.
	 * This is set by @link CJoystickActionTouchButtonBehavior @endlink when it is initialized and always
	 * cleared before loading a new touch button configuration.
	 */
	CJoystickActionTouchButtonBehavior *m_pPrimaryJoystickTouchButtonBehavior;

	/**
	 * Whether editing mode is currently active.
	 */
	bool m_EditingActive = false;

	/**
	 * Whether there are changes to the current configuration in editing mode.
	 */
	bool m_EditingChanges = false;

	void InitVisibilityFunctions();
	int NextActiveAction(int Action) const;
	int NextDirectTouchAction() const;
	void UpdateButtons(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates);
	void ResetButtons();
	void RenderButtons();
	vec2 CalculateScreenSize() const;

	bool ParseConfiguration(const void *pFileData, unsigned FileLength);
	std::optional<EDirectTouchIngameMode> ParseDirectTouchIngameMode(const json_value *pModeValue);
	std::optional<EDirectTouchSpectateMode> ParseDirectTouchSpectateMode(const json_value *pModeValue);
	std::optional<CTouchButton> ParseButton(const json_value *pButtonObject);
	std::unique_ptr<CTouchButtonBehavior> ParseBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CPredefinedTouchButtonBehavior> ParsePredefinedBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CExtraMenuTouchButtonBehavior> ParseExtraMenuBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CBindTouchButtonBehavior> ParseBindBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CBindToggleTouchButtonBehavior> ParseBindToggleBehavior(const json_value *pBehaviorObject);
	void WriteConfiguration(CJsonWriter *pWriter);
};

#endif
