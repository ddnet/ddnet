#include "touch_controls.h"

#include <engine/client.h>
#include <engine/shared/config.h>

#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/console.h>
#include <game/client/components/controls.h>
#include <game/client/components/emoticon.h>
#include <game/client/components/menus.h>
#include <game/client/components/spectator.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

static constexpr const char *ACTION_NAMES[] = {"Fire", "Hook"};
static constexpr const char *ACTION_SWAP_NAMES[] = {"Active: Fire", "Active: Hook"};
static constexpr const char *ACTION_COMMANDS[] = {"+fire", "+hook"};

CTouchControls::CTouchButton::CTouchButton(CTouchControls *pTouchControls) :
	m_pTouchControls(pTouchControls),
	m_BackgroundCorners(IGraphics::CORNER_ALL),
	m_BackgroundRounding(10.0f),
	m_Active(false)
{
}

void CTouchControls::CTouchButton::SetActive(const IInput::CTouchFingerState &FingerState)
{
	const vec2 Position = (FingerState.m_Position - m_UnitRect.TopLeft()) / m_UnitRect.Size();
	const vec2 Delta = FingerState.m_Delta / m_UnitRect.Size();
	if(!m_Active)
	{
		m_Active = true;
		m_ActivePosition = Position;
		m_AccumulatedDelta = Delta;
		m_ActivationStartTime = m_pTouchControls->Client()->GlobalTime();
		m_Finger = FingerState.m_Finger;
		m_ActivateFunction(*this);
	}
	else if(m_Finger == FingerState.m_Finger)
	{
		m_ActivePosition = Position;
		m_AccumulatedDelta += Delta;
		if(m_MovementFunction)
		{
			m_MovementFunction(*this);
		}
	}
}

void CTouchControls::CTouchButton::SetInactive()
{
	if(m_Active)
	{
		m_Active = false;
		m_DeactivateFunction(*this);
	}
}

// TODO: Optimization: Use text and quad containers for rendering
void CTouchControls::CTouchButton::Render()
{
	const ColorRGBA ButtonColor = m_Active ? ColorRGBA(0.2f, 0.2f, 0.2f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
	m_ScreenRect.Draw(ButtonColor, m_BackgroundCorners, m_BackgroundRounding);

	CUIRect Label;
	m_ScreenRect.Margin(10.0f, &Label);
	SLabelProperties LabelProps;
	LabelProps.m_MaxWidth = Label.w;
	m_pTouchControls->Ui()->DoLabel(&Label, m_LabelFunction(), 20.0f, TEXTALIGN_MC, LabelProps);
}

static CUIRect ScaleRect(CUIRect Rect, vec2 Factor)
{
	Rect.x *= Factor.x;
	Rect.y *= Factor.y;
	Rect.w *= Factor.x;
	Rect.h *= Factor.y;
	return Rect;
}

// TODO: Adjustable button layout
void CTouchControls::InitButtons(vec2 ScreenSize)
{
	const float LongTouchThreshold = 0.5f;

	const auto &&AlwaysVisibleFunction = []() {
		return true;
	};
	const auto &&IngameVisibleFunction = [&]() {
		return !GameClient()->m_Snap.m_SpecInfo.m_Active;
	};
	const auto &&ExtraButtonVisibleFunction = [&]() {
		return m_ExtraButtonsActive;
	};
	const auto &&ExtraButtonIngameVisibleFunction = [&]() {
		return m_ExtraButtonsActive && !GameClient()->m_Snap.m_SpecInfo.m_Active;
	};
	const auto &&ExtraButtonSpectateVisibleFunction = [&]() {
		return m_ExtraButtonsActive && GameClient()->m_Snap.m_SpecInfo.m_Active;
	};
	const auto &&ZoomVisibleFunction = [&]() {
		return m_ExtraButtonsActive && GameClient()->m_Camera.ZoomAllowed();
	};
	const auto &&VoteVisibleFunction = [&]() {
		return m_ExtraButtonsActive && GameClient()->m_Voting.IsActive();
	};

	// TODO: Use icons/images instead of text labels where possible, make remaining texts localizable

	const vec2 ButtonGridSize = vec2(1.0f, 1.0f) / 120.0f;

	CTouchButton LeftButton(this);
	LeftButton.m_UnitRect = ScaleRect(CUIRect{0, 100, 24, 20}, ButtonGridSize);
	LeftButton.m_ScreenRect = ScaleRect(LeftButton.m_UnitRect, ScreenSize);
	LeftButton.m_LabelFunction = []() { return "Left"; };
	LeftButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+left"); };
	LeftButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+left"); };
	LeftButton.m_IsVisibleFunction = IngameVisibleFunction;
	LeftButton.m_BackgroundCorners = IGraphics::CORNER_NONE;
	m_vTouchButtons.emplace_back(std::move(LeftButton));

	CTouchButton RightButton(this);
	RightButton.m_UnitRect = ScaleRect(CUIRect{24, 100, 24, 20}, ButtonGridSize);
	RightButton.m_ScreenRect = ScaleRect(RightButton.m_UnitRect, ScreenSize);
	RightButton.m_LabelFunction = []() { return "Right"; };
	RightButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+right"); };
	RightButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+right"); };
	RightButton.m_IsVisibleFunction = IngameVisibleFunction;
	RightButton.m_BackgroundCorners = IGraphics::CORNER_TR;
	m_vTouchButtons.emplace_back(std::move(RightButton));

	CTouchButton JumpButton(this);
	JumpButton.m_UnitRect = ScaleRect(CUIRect{12, 80, 24, 20}, ButtonGridSize);
	JumpButton.m_ScreenRect = ScaleRect(JumpButton.m_UnitRect, ScreenSize);
	JumpButton.m_LabelFunction = []() { return "Jump"; };
	JumpButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+jump"); };
	JumpButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+jump"); };
	JumpButton.m_IsVisibleFunction = IngameVisibleFunction;
	JumpButton.m_BackgroundCorners = IGraphics::CORNER_T;
	m_vTouchButtons.emplace_back(std::move(JumpButton));

	CTouchButton PrevWeaponButton(this);
	PrevWeaponButton.m_UnitRect = ScaleRect(CUIRect{14, 2, 10, 10}, ButtonGridSize);
	PrevWeaponButton.m_ScreenRect = ScaleRect(PrevWeaponButton.m_UnitRect, ScreenSize);
	PrevWeaponButton.m_LabelFunction = []() { return "← Weapon"; };
	PrevWeaponButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+prevweapon"); };
	PrevWeaponButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+prevweapon"); };
	PrevWeaponButton.m_IsVisibleFunction = IngameVisibleFunction;
	PrevWeaponButton.m_BackgroundCorners = IGraphics::CORNER_L;
	m_vTouchButtons.emplace_back(std::move(PrevWeaponButton));

	CTouchButton NextWeaponButton(this);
	NextWeaponButton.m_UnitRect = ScaleRect(CUIRect{24, 2, 10, 10}, ButtonGridSize);
	NextWeaponButton.m_ScreenRect = ScaleRect(NextWeaponButton.m_UnitRect, ScreenSize);
	NextWeaponButton.m_LabelFunction = []() { return "Weapon →"; };
	NextWeaponButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+nextweapon"); };
	NextWeaponButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+nextweapon"); };
	NextWeaponButton.m_IsVisibleFunction = IngameVisibleFunction;
	NextWeaponButton.m_BackgroundCorners = IGraphics::CORNER_R;
	m_vTouchButtons.emplace_back(std::move(NextWeaponButton));

	// Menu button:
	// - Short press: show/hide additional buttons
	// - Long press: open ingame menu
	CTouchButton MenuButton(this);
	MenuButton.m_UnitRect = ScaleRect(CUIRect{2, 2, 10, 10}, ButtonGridSize);
	MenuButton.m_ScreenRect = ScaleRect(MenuButton.m_UnitRect, ScreenSize);
	MenuButton.m_LabelFunction = []() { return "☰"; };
	MenuButton.m_ActivateFunction = [](CTouchButton &TouchButton) {};
	MenuButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) {
		if(Client()->GlobalTime() - TouchButton.m_ActivationStartTime >= LongTouchThreshold)
		{
			GameClient()->m_Menus.SetActive(true);
		}
		else
		{
			m_ExtraButtonsActive = !m_ExtraButtonsActive;
		}
	};
	MenuButton.m_IsVisibleFunction = AlwaysVisibleFunction;
	m_vTouchButtons.emplace_back(std::move(MenuButton));

	// TODO: Eventually support standard gestures to zoom (only active in spectator mode to avoid interference while playing)
	CTouchButton ZoomOutButton(this);
	ZoomOutButton.m_UnitRect = ScaleRect(CUIRect{36, 2, 10, 10}, ButtonGridSize);
	ZoomOutButton.m_ScreenRect = ScaleRect(ZoomOutButton.m_UnitRect, ScreenSize);
	ZoomOutButton.m_LabelFunction = []() { return "Zoom out"; };
	ZoomOutButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "zoom-"); };
	ZoomOutButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "zoom-"); };
	ZoomOutButton.m_IsVisibleFunction = ZoomVisibleFunction;
	ZoomOutButton.m_BackgroundCorners = IGraphics::CORNER_L;
	m_vTouchButtons.emplace_back(std::move(ZoomOutButton));

	CTouchButton ZoomDefaultButton(this);
	ZoomDefaultButton.m_UnitRect = ScaleRect(CUIRect{46, 2, 10, 10}, ButtonGridSize);
	ZoomDefaultButton.m_ScreenRect = ScaleRect(ZoomDefaultButton.m_UnitRect, ScreenSize);
	ZoomDefaultButton.m_LabelFunction = []() { return "Default zoom"; };
	ZoomDefaultButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "zoom"); };
	ZoomDefaultButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "zoom"); };
	ZoomDefaultButton.m_IsVisibleFunction = ZoomVisibleFunction;
	ZoomDefaultButton.m_BackgroundCorners = IGraphics::CORNER_NONE;
	m_vTouchButtons.emplace_back(std::move(ZoomDefaultButton));

	CTouchButton ZoomInButton(this);
	ZoomInButton.m_UnitRect = ScaleRect(CUIRect{56, 2, 10, 10}, ButtonGridSize);
	ZoomInButton.m_ScreenRect = ScaleRect(ZoomInButton.m_UnitRect, ScreenSize);
	ZoomInButton.m_LabelFunction = []() { return "Zoom in"; };
	ZoomInButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "zoom+"); };
	ZoomInButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "zoom+"); };
	ZoomInButton.m_IsVisibleFunction = ZoomVisibleFunction;
	ZoomInButton.m_BackgroundCorners = IGraphics::CORNER_R;
	m_vTouchButtons.emplace_back(std::move(ZoomInButton));

	CTouchButton ScoreboardButton(this);
	ScoreboardButton.m_UnitRect = ScaleRect(CUIRect{2, 16, 10, 8}, ButtonGridSize);
	ScoreboardButton.m_ScreenRect = ScaleRect(ScoreboardButton.m_UnitRect, ScreenSize);
	ScoreboardButton.m_LabelFunction = []() { return "Scoreboard"; };
	ScoreboardButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+scoreboard"); };
	ScoreboardButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+scoreboard"); };
	ScoreboardButton.m_IsVisibleFunction = ExtraButtonVisibleFunction;
	m_vTouchButtons.emplace_back(std::move(ScoreboardButton));

	CTouchButton EmoticonButton(this);
	EmoticonButton.m_UnitRect = ScaleRect(CUIRect{14, 16, 10, 8}, ButtonGridSize); // Emoticon and spectate buttons have same position
	EmoticonButton.m_ScreenRect = ScaleRect(EmoticonButton.m_UnitRect, ScreenSize);
	EmoticonButton.m_LabelFunction = []() { return "Emoticon"; };
	EmoticonButton.m_ActivateFunction = [](CTouchButton &TouchButton) {};
	EmoticonButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+emote"); };
	EmoticonButton.m_IsVisibleFunction = ExtraButtonIngameVisibleFunction;
	m_vTouchButtons.emplace_back(std::move(EmoticonButton));

	CTouchButton SpectateButton(this);
	SpectateButton.m_UnitRect = ScaleRect(CUIRect{14, 16, 10, 8}, ButtonGridSize);
	SpectateButton.m_ScreenRect = ScaleRect(SpectateButton.m_UnitRect, ScreenSize);
	SpectateButton.m_LabelFunction = []() { return "Spectate"; }; // Emoticon and spectate buttons have same position
	SpectateButton.m_ActivateFunction = [](CTouchButton &TouchButton) {};
	SpectateButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+spectate"); };
	SpectateButton.m_IsVisibleFunction = ExtraButtonSpectateVisibleFunction;
	m_vTouchButtons.emplace_back(std::move(SpectateButton));

	CTouchButton AllChatButton(this);
	AllChatButton.m_UnitRect = ScaleRect(CUIRect{26, 16, 10, 8}, ButtonGridSize);
	AllChatButton.m_ScreenRect = ScaleRect(AllChatButton.m_UnitRect, ScreenSize);
	AllChatButton.m_LabelFunction = []() { return "Chat"; };
	AllChatButton.m_ActivateFunction = [](CTouchButton &TouchButton) {};
	AllChatButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+show_chat; chat all"); };
	AllChatButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+show_chat; chat all"); };
	AllChatButton.m_IsVisibleFunction = ExtraButtonVisibleFunction;
	m_vTouchButtons.emplace_back(std::move(AllChatButton));

	CTouchButton TeamChatButton(this);
	TeamChatButton.m_UnitRect = ScaleRect(CUIRect{38, 16, 10, 8}, ButtonGridSize);
	TeamChatButton.m_ScreenRect = ScaleRect(TeamChatButton.m_UnitRect, ScreenSize);
	TeamChatButton.m_LabelFunction = []() { return "Team chat"; };
	TeamChatButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "+show_chat; chat team"); };
	TeamChatButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "+show_chat; chat team"); };
	TeamChatButton.m_IsVisibleFunction = ExtraButtonVisibleFunction;
	m_vTouchButtons.emplace_back(std::move(TeamChatButton));

	CTouchButton VoteYesButton(this);
	VoteYesButton.m_UnitRect = ScaleRect(CUIRect{2, 40, 10, 8}, ButtonGridSize);
	VoteYesButton.m_ScreenRect = ScaleRect(VoteYesButton.m_UnitRect, ScreenSize);
	VoteYesButton.m_LabelFunction = []() { return "Vote yes"; };
	VoteYesButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "vote yes"); };
	VoteYesButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "vote yes"); };
	VoteYesButton.m_IsVisibleFunction = VoteVisibleFunction;
	VoteYesButton.m_BackgroundCorners = IGraphics::CORNER_ALL;
	m_vTouchButtons.emplace_back(std::move(VoteYesButton));

	CTouchButton VoteNoButton(this);
	VoteNoButton.m_UnitRect = ScaleRect(CUIRect{14, 40, 10, 8}, ButtonGridSize);
	VoteNoButton.m_ScreenRect = ScaleRect(VoteNoButton.m_UnitRect, ScreenSize);
	VoteNoButton.m_LabelFunction = []() { return "Vote no"; };
	VoteNoButton.m_ActivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(1, "vote no"); };
	VoteNoButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) { Console()->ExecuteLineStroked(0, "vote no"); };
	VoteNoButton.m_IsVisibleFunction = VoteVisibleFunction;
	VoteNoButton.m_BackgroundCorners = IGraphics::CORNER_ALL;
	m_vTouchButtons.emplace_back(std::move(VoteNoButton));

	CTouchButton ToggleDummyButton(this);
	ToggleDummyButton.m_UnitRect = ScaleRect(CUIRect{92, 2, 12, 12}, ButtonGridSize);
	ToggleDummyButton.m_ScreenRect = ScaleRect(ToggleDummyButton.m_UnitRect, ScreenSize);
	ToggleDummyButton.m_LabelFunction = [&]() {
		if(Client()->DummyConnecting())
		{
			return "Dummy connecting…";
		}
		else if(!Client()->DummyConnected())
		{
			return "Connect dummy";
		}
		else
		{
			return "Toggle dummy";
		}
	};
	ToggleDummyButton.m_ActivateFunction = [](CTouchButton &TouchButton) {};
	ToggleDummyButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) {
		if(Client()->DummyConnecting() || Client()->DummyConnectingDelayed())
		{
			return;
		}
		else if(Client()->DummyConnected())
		{
			g_Config.m_ClDummy = !g_Config.m_ClDummy;
		}
		else
		{
			Client()->DummyConnect();
		}
	};
	ToggleDummyButton.m_IsVisibleFunction = [&]() {
		return Client()->DummyAllowed() && (Client()->DummyConnected() || m_ExtraButtonsActive);
	};
	m_vTouchButtons.emplace_back(std::move(ToggleDummyButton));

	// Action button:
	// - If joystick is currently active with one action: activate the other action
	// - Else: swap active action
	CTouchButton ActionButton(this);
	ActionButton.m_UnitRect = ScaleRect(CUIRect{106, 2, 12, 12}, ButtonGridSize);
	ActionButton.m_ScreenRect = ScaleRect(ActionButton.m_UnitRect, ScreenSize);
	ActionButton.m_LabelFunction = [&]() {
		if(m_JoystickActionSecondary != NUM_ACTIONS)
		{
			return ACTION_NAMES[m_JoystickActionSecondary];
		}
		else if(m_JoystickActionPrimary != NUM_ACTIONS)
		{
			return ACTION_NAMES[(m_JoystickActionPrimary + 1) % NUM_ACTIONS];
		}
		return ACTION_SWAP_NAMES[m_ActionSelected];
	};
	ActionButton.m_ActivateFunction = [&](CTouchButton &TouchButton) {
		if(m_JoystickActionPrimary != NUM_ACTIONS)
		{
			m_JoystickActionSecondary = (m_JoystickActionPrimary + 1) % NUM_ACTIONS;
			Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_JoystickActionSecondary]);
		}
		else
		{
			m_ActionSelected = (m_ActionSelected + 1) % NUM_ACTIONS;
		}
	};
	ActionButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) {
		if(m_JoystickActionSecondary != NUM_ACTIONS)
		{
			Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_JoystickActionSecondary]);
			m_JoystickActionSecondary = NUM_ACTIONS;
		}
	};
	ActionButton.m_IsVisibleFunction = IngameVisibleFunction;
	m_vTouchButtons.emplace_back(std::move(ActionButton));

	// TODO: The joystick button is only rendered round but currently it also accepts touches in the whole rectangle covering the circle.
	// TODO: Should use DrawCircle function to render circle and remove m_BackgroundRounding again so it looks smoother.
	const float JoystickButtonHeight = 48.0f;
	const float JoystickButtonWidth = std::ceil(JoystickButtonHeight / (ScreenSize.x / ScreenSize.y));
	CTouchButton JoystickButton(this);
	JoystickButton.m_UnitRect = ScaleRect(CUIRect{120.0f - JoystickButtonWidth - 2.0f, 120.0f - JoystickButtonHeight - 2.0f, JoystickButtonWidth, JoystickButtonHeight}, ButtonGridSize);
	JoystickButton.m_ScreenRect = ScaleRect(JoystickButton.m_UnitRect, ScreenSize);
	JoystickButton.m_LabelFunction = [&]() { return ACTION_NAMES[m_ActionSelected]; };
	JoystickButton.m_ActivateFunction = [&](CTouchButton &TouchButton) {
		m_JoystickActionPrimary = m_ActionSelected;
		GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy] = (TouchButton.m_ActivePosition - vec2(0.5f, 0.5f)) * GameClient()->m_Controls.GetMaxMouseDistance();
		Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_JoystickActionPrimary]);
	};
	JoystickButton.m_DeactivateFunction = [&](CTouchButton &TouchButton) {
		Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_JoystickActionPrimary]);
		m_JoystickActionPrimary = NUM_ACTIONS;
	};
	JoystickButton.m_MovementFunction = [&](CTouchButton &TouchButton) {
		GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy] = (TouchButton.m_ActivePosition - vec2(0.5f, 0.5f)) * GameClient()->m_Controls.GetMaxMouseDistance();
	};
	JoystickButton.m_IsVisibleFunction = [&]() { return g_Config.m_ClTouchControlsJoystick && !m_pClient->m_Snap.m_SpecInfo.m_Active; };
	JoystickButton.m_BackgroundRounding = minimum(JoystickButton.m_ScreenRect.w, JoystickButton.m_ScreenRect.h) / 2.0f;
	m_vTouchButtons.emplace_back(std::move(JoystickButton));
}

void CTouchControls::UpdateButtons(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates)
{
	const bool DirectTouchEnabled = !g_Config.m_ClTouchControlsJoystick || m_pClient->m_Snap.m_SpecInfo.m_Active;

	std::vector<IInput::CTouchFingerState> vRemainingTouchFingerStates = vTouchFingerStates;

	// Remove touch fingers which have been released from the set of used touch fingers.
	for(auto It = m_vUsedTouchFingers.begin(); It != m_vUsedTouchFingers.end();)
	{
		const auto ActiveFinger = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
			return TouchFingerState.m_Finger == *It;
		});
		if(ActiveFinger == vRemainingTouchFingerStates.end())
		{
			It = m_vUsedTouchFingers.erase(It);
		}
		else
		{
			++It;
		}
	}

	// Remove remaining finger states for fingers which are responsible for active actions
	// and release action when the finger responsible for it is not pressed down anymore.
	bool GotDirectFingerState = false; // Whether DirectFingerState is valid
	IInput::CTouchFingerState DirectFingerState{}; // The finger that will be used to update the mouse position
	for(int Action = ACTION_FIRE; Action < NUM_ACTIONS; ++Action)
	{
		if(!m_aActionStates[Action].m_Active)
			continue;

		const auto ActiveFinger = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
			return TouchFingerState.m_Finger == m_aActionStates[Action].m_Finger;
		});
		if(ActiveFinger == vRemainingTouchFingerStates.end() || !DirectTouchEnabled)
		{
			m_aActionStates[Action].m_Active = false;
			Console()->ExecuteLineStroked(0, ACTION_COMMANDS[Action]);
		}
		else
		{
			if(Action == m_ActionLastActivated)
			{
				GotDirectFingerState = true;
				DirectFingerState = *ActiveFinger;
			}
			vRemainingTouchFingerStates.erase(ActiveFinger);
		}
	}

	// Update touch button states after the active action fingers were removed from the vector
	// so that current cursor movement can cross over touch buttons without activating them.
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		bool Active = false;
		if(TouchButton.m_IsVisibleFunction())
		{
			while(!vRemainingTouchFingerStates.empty())
			{
				const auto FingerInsideButton = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
					return TouchButton.m_UnitRect.Inside(TouchFingerState.m_Position);
				});
				if(FingerInsideButton == vRemainingTouchFingerStates.end())
				{
					break;
				}
				TouchButton.SetActive(*FingerInsideButton);
				if(std::find(m_vUsedTouchFingers.begin(), m_vUsedTouchFingers.end(), FingerInsideButton->m_Finger) == m_vUsedTouchFingers.end())
				{
					m_vUsedTouchFingers.push_back(FingerInsideButton->m_Finger);
				}
				vRemainingTouchFingerStates.erase(FingerInsideButton);
				Active = true;
			}
		}
		if(!Active)
		{
			TouchButton.SetInactive();
		}
	}

	// Fingers which are still pressed after having been used for a touch button will
	// not activate the actions, to prevent accidental usage when leaving the buttons.
	vRemainingTouchFingerStates.erase(
		std::remove_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
			return std::find(m_vUsedTouchFingers.begin(), m_vUsedTouchFingers.end(), TouchFingerState.m_Finger) != m_vUsedTouchFingers.end();
		}),
		vRemainingTouchFingerStates.end());

	// Activate action if there is an unhandled pressed down finger.
	int ActivateAction = NUM_ACTIONS;
	if(DirectTouchEnabled && !vRemainingTouchFingerStates.empty() && !m_aActionStates[m_ActionSelected].m_Active)
	{
		GotDirectFingerState = true;
		DirectFingerState = vRemainingTouchFingerStates[0];
		vRemainingTouchFingerStates.erase(vRemainingTouchFingerStates.begin());
		m_aActionStates[m_ActionSelected].m_Active = true;
		m_aActionStates[m_ActionSelected].m_Finger = DirectFingerState.m_Finger;
		m_ActionLastActivated = m_ActionSelected;
		ActivateAction = m_ActionSelected;
	}

	// Update mouse position based on the finger responsible for the last active action.
	if(GotDirectFingerState)
	{
		vec2 WorldScreenSize;
		RenderTools()->CalcScreenParams(Graphics()->ScreenAspect(), m_pClient->m_Snap.m_SpecInfo.m_Active ? m_pClient->m_Camera.m_Zoom : 1.0f, &WorldScreenSize.x, &WorldScreenSize.y);
		if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		{
			GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy] += -DirectFingerState.m_Delta * WorldScreenSize;
			GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].x = clamp(GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].x, -201.0f * 32, (Collision()->GetWidth() + 201.0f) * 32.0f);
			GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].y = clamp(GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].y, -201.0f * 32, (Collision()->GetHeight() + 201.0f) * 32.0f);
		}
		else
		{
			GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy] = (DirectFingerState.m_Position - vec2(0.5f, 0.5f)) * WorldScreenSize;
		}
	}

	// Activate action after the mouse position is set.
	if(ActivateAction != NUM_ACTIONS)
	{
		Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_ActionSelected]);
	}
}

void CTouchControls::RenderButtons()
{
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		if(!TouchButton.m_IsVisibleFunction())
			continue;

		TouchButton.Render();
	}
}

void CTouchControls::OnReset()
{
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.m_Active = false;
	}
	for(CActionState &ActionState : m_aActionStates)
	{
		ActionState.m_Active = false;
	}
	m_vUsedTouchFingers.clear();
}

void CTouchControls::OnWindowResize()
{
	OnReset();
	m_vTouchButtons.clear(); // Recreate buttons to update positions and sizes
}

bool CTouchControls::OnTouchState(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates)
{
	if(!g_Config.m_ClTouchControls)
		return false;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return false;
	if(GameClient()->m_Chat.IsActive() ||
		!GameClient()->m_GameConsole.IsClosed() ||
		GameClient()->m_Menus.IsActive() ||
		GameClient()->m_Emoticon.IsActive() ||
		GameClient()->m_Spectator.IsActive())
	{
		OnReset();
		return false;
	}
	UpdateButtons(vTouchFingerStates);
	return true;
}

void CTouchControls::OnRender()
{
	if(!g_Config.m_ClTouchControls)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(GameClient()->m_Chat.IsActive() || GameClient()->m_Emoticon.IsActive() || GameClient()->m_Spectator.IsActive())
		return;

	const float ScreenHeight = 400.0f * 3.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	const vec2 ScreenSize = vec2(ScreenWidth, ScreenHeight);
	Graphics()->MapScreen(0.0f, 0.0f, ScreenWidth, ScreenHeight);

	if(m_vTouchButtons.empty())
	{
		InitButtons(ScreenSize);
	}
	RenderButtons();
}
