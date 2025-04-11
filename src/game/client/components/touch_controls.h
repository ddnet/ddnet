#ifndef GAME_CLIENT_COMPONENTS_TOUCH_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_TOUCH_CONTROLS_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/input.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class CJsonWriter;
typedef struct _json_value json_value;

class CTouchControls : public CComponent
{
public:
	static constexpr const int BUTTON_SIZE_SCALE = 1000000;
	static constexpr const int BUTTON_SIZE_MINIMUM = 50000;
	static constexpr const int BUTTON_SIZE_MAXIMUM = 500000;
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

	class CUnitRect
	{
	public:
		int m_X;
		int m_Y;
		int m_W;
		int m_H;
		bool operator<(const CUnitRect &Other) const
		{
			if(m_X + m_W / 2 != Other.m_X + Other.m_W / 2)
				return m_X + m_W / 2 < Other.m_X + Other.m_W / 2;
			return m_Y + m_H / 2 < Other.m_Y + Other.m_H / 2;
		}
		// This means distance;
		double operator/(const CUnitRect &Other) const
		{
			double Dx = Other.m_X + Other.m_W / 2.0f - m_X - m_W / 2.0f;
			Dx /= BUTTON_SIZE_SCALE;
			Dx *= Dx;
			double Dy = Other.m_Y + Other.m_H / 2.0f - m_Y - m_H / 2.0f;
			Dy /= BUTTON_SIZE_SCALE;
			Dy *= Dy;
			return std::sqrt(Dx + Dy);
		}
		bool IsOverlap(const CUnitRect &Other) const
		{
			return (m_X < Other.m_X + Other.m_W) && (m_X + m_W > Other.m_X) && (m_Y < Other.m_Y + Other.m_H) && (m_Y + m_H > Other.m_Y);
		}
	};

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
	static const constexpr int MAXNUMBER = (int)EButtonVisibility::EXTRA_MENU_5 - (int)EButtonVisibility::EXTRA_MENU_1 + 1;
	const std::array<const char *, (size_t)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> m_VisibilityStrings = {"Ingame", "Zoom Allowed", "Vote Active", "Dummy Allowed", "Dummy Connected", "Rcon Authed",
		"Demo Player", "Extra Menu 1", "Extra Menu 2", "Extra Menu 3", "Extra Menu 4", "Extra Menu 5"};

	enum class EButtonShape
	{
		RECT,
		CIRCLE,
		NUM_SHAPES
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

private:
	static constexpr const char *const DIRECT_TOUCH_INGAME_MODE_NAMES[(int)EDirectTouchIngameMode::NUM_STATES] = {"disabled", "action", "aim", "fire", "hook"};
	static constexpr const char *const DIRECT_TOUCH_SPECTATE_MODE_NAMES[(int)EDirectTouchSpectateMode::NUM_STATES] = {"disabled", "aim"};

	static constexpr const char *const SHAPE_NAMES[(int)EButtonShape::NUM_SHAPES] = {"rect", "circle"};
	std::array<const char *, (int)EButtonShape::NUM_SHAPES> m_Shapes = {"Rect", "Circle"};

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

	static constexpr const char *const LABEL_TYPE_NAMES[(int)CButtonLabel::EType::NUM_TYPES] = {"plain", "localized", "icon"};

public:
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

		CUnitRect m_UnitRect; // {0,0,BUTTON_SIZE_MINIMUM,BUTTON_SIZE_MINIMUM} = default
		CUIRect m_ScreenRect;

		EButtonShape m_Shape; // Rect = default
		int m_BackgroundCorners; // only used with EButtonShape::RECT

		std::vector<CButtonVisibility> m_vVisibilities;
		std::unique_ptr<CTouchButtonBehavior> m_pBehavior; // nullptr = default. In button editor the default is bind behavior with nothing.

		bool m_VisibilityCached;
		std::chrono::nanoseconds m_VisibilityStartTime;

		void UpdatePointers();
		void UpdateScreenFromUnitRect();
		void UpdateBackgroundCorners();

		vec2 ClampTouchPosition(vec2 TouchPosition) const;
		bool IsInside(vec2 TouchPosition) const;
		void UpdateVisibility();
		bool IsVisible() const;
		// Force using Selected for button colors, Rect for rendering rects.
		void Render(std::optional<bool> Selected = std::nullopt, std::optional<CUnitRect> Rect = std::nullopt) const;
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
		virtual const char *GetBehaviorType() const = 0;
		virtual const char *GetPredefinedType() { return nullptr; }
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
		const char *GetBehaviorType() const override { return BEHAVIOR_TYPE; }
		const char *GetPredefinedType() override { return m_pId; }

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
		int GetNumber() const { return m_Number; }
		void SetNumber(int &&Number) { m_Number = Number; }
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
		void SetLabel(CButtonLabel Label);
		std::string GetCommand() const { return m_Command; }
		void SetCommand(std::string &&Command);
		void OnActivate() override;
		void OnDeactivate() override;
		void OnUpdate() override;
		void WriteToConfiguration(CJsonWriter *pWriter) override;
		const char *GetBehaviorType() const override { return BEHAVIOR_TYPE; }

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
			CCommand() :
				m_LabelType(CButtonLabel::EType::PLAIN) {}
		};

		CBindToggleTouchButtonBehavior(std::vector<CCommand> &&vCommands) :
			m_vCommands(std::move(vCommands)) {}

		CButtonLabel GetLabel() const override;
		void SetLabel(CButtonLabel Label);
		std::vector<CCommand> GetCommand() const { return m_vCommands; }
		void SetCommand(std::vector<CCommand> &&Commands) { m_vCommands = Commands; }
		void OnActivate() override;
		void WriteToConfiguration(CJsonWriter *pWriter) override;
		const char *GetBehaviorType() const override { return BEHAVIOR_TYPE; }

	private:
		std::vector<CCommand> m_vCommands;
		size_t m_ActiveCommandIndex = 0;
	};

private:
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
	 * Background color of inactive touch buttons.
	 *
	 * Saved to the touch controls configuration.
	 */
	ColorRGBA m_BackgroundColorInactive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

	// Default Inactive Color. This shouldn't be changed.
	const ColorRGBA m_DefaultBackgroundColorInactive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

	/**
	 * Background color of active touch buttons.
	 *
	 * Saved to the touch controls configuration.
	 */
	ColorRGBA m_BackgroundColorActive = ColorRGBA(0.2f, 0.2f, 0.2f, 0.25f);

	// Default Active Color. This shouldn't be changed.
	const ColorRGBA m_DefaultBackgroundColorActive = ColorRGBA(0.2f, 0.2f, 0.2f, 0.25f);
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
	 * Counts how many joysticks are pressed.
	 */
	int m_JoystickCount = 0;

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

	class CBehaviorFactoryEditor
	{
	public:
		const char *m_pId;
		std::function<std::unique_ptr<CPredefinedTouchButtonBehavior>()> m_Factory;
	};

	bool ParseConfiguration(const void *pFileData, unsigned FileLength);
	std::optional<EDirectTouchIngameMode> ParseDirectTouchIngameMode(const json_value *pModeValue);
	std::optional<EDirectTouchSpectateMode> ParseDirectTouchSpectateMode(const json_value *pModeValue);
	std::optional<ColorRGBA> ParseColor(const json_value *pColorValue, const char *pAttributeName, std::optional<ColorRGBA> DefaultColor) const;
	std::optional<CTouchButton> ParseButton(const json_value *pButtonObject);
	std::unique_ptr<CTouchButtonBehavior> ParseBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CPredefinedTouchButtonBehavior> ParsePredefinedBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CExtraMenuTouchButtonBehavior> ParseExtraMenuBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CBindTouchButtonBehavior> ParseBindBehavior(const json_value *pBehaviorObject);
	std::unique_ptr<CBindToggleTouchButtonBehavior> ParseBindToggleBehavior(const json_value *pBehaviorObject);
	void WriteConfiguration(CJsonWriter *pWriter);

	class CQuadtreeNode
	{
	public:
		CUnitRect m_Space;
		std::unique_ptr<CQuadtreeNode> m_NW = nullptr, m_NE = nullptr, m_SW = nullptr, m_SE = nullptr;
		std::vector<CUnitRect> m_Rects;
		CQuadtreeNode(int X, int Y, int W, int H) :
			m_Space({X, Y, W, H}) {}
		void Split();
	};
	class CQuadtree
	{
	public:
		CQuadtree(int Width, int Height) :
			m_Root(0, 0, Width, Height), m_MaxObj(3), m_MaxDep(3) {} // After 3 splits, the width of each rects is 125000. Button's minimum width is 50000.

		void Insert(const CUnitRect &Rect) { Insert(m_Root, Rect, 0); }
		bool Find(const CUnitRect &MyRect) { return Find(MyRect, m_Root); }

	private:
		CQuadtreeNode m_Root;
		const size_t m_MaxObj;
		const size_t m_MaxDep;
		void Insert(CQuadtreeNode &Node, const CUnitRect &Rect, size_t Depth);
		bool Find(const CUnitRect &MyRect, CQuadtreeNode &Node);
	};
	CUnitRect FindPositionXY(const std::set<CUnitRect> &vVisibleButtonRects, CUnitRect MyRect);

	// This is how editor render buttons.
	void RenderButtonsWhileInEditor();
	// This is how editor deal with touch inputs.
	void EditButtons(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates);
	void CopySettings(CTouchButton *TargetButton, CTouchButton *SrcButton);

	// For process fingerstates in button editor.
	bool m_LongPress = false;
	std::optional<IInput::CTouchFingerState> m_ActiveFingerState;
	std::optional<IInput::CTouchFingerState> m_ZoomFingerState;
	vec2 m_ZoomStartPos = {0.0f, 0.0f};
	vec2 m_AccumulatedDelta = {0.0f, 0.0f};
	std::vector<IInput::CTouchFingerState> m_vDeletedFingerState;
	std::optional<IInput::CTouchFingerState> m_LongPressFingerState;
	std::array<bool, (size_t)EButtonVisibility::NUM_VISIBILITIES> m_aVirtualVisibilities;

	std::optional<CUnitRect> m_ShownRect;
	CTouchButton *m_pSelectedButton = nullptr;
	// This is for render, when directly slide to move buttons on screen.
	std::unique_ptr<CTouchButton> m_pTmpButton = nullptr;

public:
	bool UnsavedChanges() const { return m_UnsavedChanges; }
	void SetUnsavedChanges(bool UnsavedChanges) { m_UnsavedChanges = UnsavedChanges; }

	CTouchButton *NewButton();
	void DeleteButton();
	std::array<bool, MAXNUMBER> FindExistingExtraMenus();
	bool IfOverlapping(CUnitRect MyRect = {0, 0, BUTTON_SIZE_MINIMUM, BUTTON_SIZE_MINIMUM});
	CUnitRect UpdatePosition(CUnitRect MyRect, bool Ignore = false); // If Ignore == true, then the function will also try to avoid m_pSelectedButton.
	void ResetButtonPointers();
	void ResetVirtualVisibilities();
	CUIRect CalculateScreenFromUnitRect(CUnitRect Unit, EButtonShape Shape = EButtonShape::RECT);

	// Getters and setters.
	std::array<bool, (size_t)EButtonVisibility::NUM_VISIBILITIES> VirtualVisibilities() const { return m_aVirtualVisibilities; }
	void ReverseVirtualVisibilities(int Number) { m_aVirtualVisibilities[Number] = !m_aVirtualVisibilities[Number]; }
	std::optional<CUnitRect> ShownRect() const { return m_ShownRect; }
	void SetShownRect(std::optional<CUnitRect> Rect) { m_ShownRect = Rect; }
	CTouchButton *SelectedButton() const { return m_pSelectedButton; }
	void SetSelectedButton(CTouchButton *TargetButton) { m_pSelectedButton = TargetButton; }
	bool NoRealButtonSelected() const { return m_pSelectedButton == nullptr; }
	void RemakeTmpButton() { m_pTmpButton = std::make_unique<CTouchButton>(this); }
	CTouchButton *TmpButton() const { return m_pTmpButton.get(); }
	bool IsButtonEditing() const { return m_pSelectedButton != nullptr || m_pTmpButton != nullptr; }
	ColorRGBA DefaultBackgroundColorInactive() const { return m_DefaultBackgroundColorInactive; }
	ColorRGBA DefaultBackgroundColorActive() const { return m_DefaultBackgroundColorActive; }
	ColorRGBA BackgroundColorInactive() const { return m_BackgroundColorInactive; }
	ColorRGBA BackgroundColorActive() const { return m_BackgroundColorActive; }
	void SetBackgroundColorInActive(ColorRGBA Color) { m_BackgroundColorInactive = Color; }
	void SetBackgroundColorActive(ColorRGBA Color) { m_BackgroundColorActive = Color; }
	std::array<const char *, (size_t)EButtonVisibility::NUM_VISIBILITIES> VisibilityStrings() const { return m_VisibilityStrings; }
	std::array<const char *, (int)EButtonShape::NUM_SHAPES> Shapes() const { return m_Shapes; }

	// The extra menu behavior will use m_CachedNumber in CMenus.
	const CBehaviorFactoryEditor m_BehaviorFactoriesEditor[10] = {
		{CExtraMenuTouchButtonBehavior::BEHAVIOR_ID, [&]() { return std::make_unique<CExtraMenuTouchButtonBehavior>(0); }},
		{CJoystickHookTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CJoystickHookTouchButtonBehavior>(); }},
		{CJoystickFireTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CJoystickFireTouchButtonBehavior>(); }},
		{CJoystickAimTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CJoystickAimTouchButtonBehavior>(); }},
		{CJoystickActionTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CJoystickActionTouchButtonBehavior>(); }},
		{CUseActionTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CUseActionTouchButtonBehavior>(); }},
		{CSwapActionTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CSwapActionTouchButtonBehavior>(); }},
		{CSpectateTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CSpectateTouchButtonBehavior>(); }},
		{CEmoticonTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CEmoticonTouchButtonBehavior>(); }},
		{CIngameMenuTouchButtonBehavior::BEHAVIOR_ID, []() { return std::make_unique<CIngameMenuTouchButtonBehavior>(); }}};

	// Set the EPopupType and call
	enum class EPopupType
	{
		// Unsaved settings when changing selected button.
		BUTTON_CHANGED = 0,
		// FindPositionXY can't find an empty space for the selected button(Currently it's overlapping).
		NO_SPACE,
		// Selected button is not visible.
		BUTTON_INVISIBLE,
		NUM_POPUPS
	};

	// These things must be set before opening the menu for calling the popup.
	// After setting these, use GameClient()->m_Menus.SetActive(true), then the popup could be called automatically if EPopupType is not NUM_POPUPS.
	class CPopupParam
	{
	public:
		EPopupType m_PopupType = EPopupType::NUM_POPUPS;
		CTouchButton *m_OldSelectedButton = nullptr;
		CTouchButton *m_NewSelectedButton = nullptr;
		bool m_KeepMenuOpen = false;
	};

	CPopupParam RequiredPopup();

	// The issues won't be resolved until the Button Editor is rendered. If you want to solve issues right now don't use this.
	// This is usually for update cached settings in button editor.
	enum class EIssueType
	{
		// Update Cached settings from m_TargetButton.
		CACHE_SETTINGS = 0,
		// Save Cached settings to m_TargetButton.
		SAVE_SETTINGS,
		NUM_ISSUES
	};

	class CIssueParam
	{
	public:
		// Whether the issue is finished.
		bool m_Finished = true;
		CTouchButton *m_TargetButton = nullptr;
	};

	bool IsIssueNotFinished();
	std::array<CTouchControls::CIssueParam, (unsigned)CTouchControls::EIssueType::NUM_ISSUES> Issues();

private:
	CPopupParam m_PopupParam;
	bool m_UnsavedChanges = false;

	std::array<CIssueParam, (int)EIssueType::NUM_ISSUES> m_IssueParam;
};

#endif
