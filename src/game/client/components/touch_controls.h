#ifndef GAME_CLIENT_COMPONENTS_TOUCH_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_TOUCH_CONTROLS_H

#include <base/vmath.h>

#include <engine/input.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <functional>
#include <unordered_set>
#include <vector>

class CTouchControls : public CComponent
{
	// TODO: Encapsulate behavior and rendering in subclasses with virtual functions instead of using std::functions?
	class CTouchButton
	{
		CTouchControls *m_pTouchControls;

	public:
		CTouchButton(CTouchControls *pTouchControls);

		CUIRect m_UnitRect;
		CUIRect m_ScreenRect;

		std::function<const char *()> m_LabelFunction;
		std::function<void(CTouchButton &TouchButton)> m_ActivateFunction;
		std::function<void(CTouchButton &TouchButton)> m_DeactivateFunction;
		std::function<void(CTouchButton &TouchButton)> m_MovementFunction; // can be nullptr
		std::function<bool()> m_IsVisibleFunction;
		int m_BackgroundCorners;
		float m_BackgroundRounding;

		bool m_Active; // variables below must only be used when active
		IInput::CTouchFinger m_Finger;
		vec2 m_ActivePosition;
		vec2 m_AccumulatedDelta;
		float m_ActivationStartTime;

		void SetActive(const IInput::CTouchFingerState &FingerState);
		void SetInactive();
		void Render();
	};

	std::vector<CTouchButton> m_vTouchButtons;

	/**
	 * Touch fingers which will be ignored for activating the primary action.
	 * These are fingers which were used for touch buttons previously. They also
	 * include fingers which are still pressed but not activating any button.
	 */
	std::vector<IInput::CTouchFinger> m_vUsedTouchFingers;

	bool m_ExtraButtonsActive = false;

	enum
	{
		ACTION_FIRE = 0,
		ACTION_HOOK,
		NUM_ACTIONS
	};

	class CActionState
	{
	public:
		bool m_Active = false;
		IInput::CTouchFinger m_Finger;
	};

	int m_ActionSelected = ACTION_FIRE;
	int m_ActionLastActivated = ACTION_FIRE;
	CActionState m_aActionStates[NUM_ACTIONS];
	int m_JoystickActionPrimary = NUM_ACTIONS;
	int m_JoystickActionSecondary = NUM_ACTIONS;

	void InitButtons(vec2 ScreenSize);
	void UpdateButtons(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates);
	void RenderButtons();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnWindowResize() override;
	bool OnTouchState(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates) override;
	void OnRender() override;
};
#endif
