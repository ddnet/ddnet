/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <SDL3/SDL.h>

#include <base/system.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include "input.h"
#include "keynames.h"

// support older SDL version (pre 2.0.6)
#ifndef SDL_JOYSTICK_AXIS_MIN
#define SDL_JOYSTICK_AXIS_MIN (-32768)
#endif
#ifndef SDL_JOYSTICK_AXIS_MAX
#define SDL_JOYSTICK_AXIS_MAX 32767
#endif

void CInput::AddKeyEvent(int Key, int Flags)
{
	dbg_assert(Key >= KEY_FIRST && Key < KEY_LAST, "Key invalid: %d", Key);
	dbg_assert((Flags & (FLAG_PRESS | FLAG_RELEASE)) != 0 && (Flags & ~(FLAG_PRESS | FLAG_RELEASE)) == 0, "Flags invalid");

	CEvent Event;
	Event.m_Key = Key;
	Event.m_Flags = Flags;
	Event.m_aText[0] = '\0';
	Event.m_InputCount = m_InputCounter;
	m_vInputEvents.emplace_back(Event);

	if(Flags & IInput::FLAG_PRESS)
	{
		m_aCurrentKeyStates[Key] = true;
		m_aFrameKeyStates[Key] = true;
	}
	if(Flags & IInput::FLAG_RELEASE)
	{
		m_aCurrentKeyStates[Key] = false;
	}
}

void CInput::AddTextEvent(const char *pText)
{
	CEvent Event;
	Event.m_Key = KEY_UNKNOWN;
	Event.m_Flags = FLAG_TEXT;
	str_copy(Event.m_aText, pText);
	Event.m_InputCount = m_InputCounter;
	m_vInputEvents.emplace_back(Event);
}

CInput::CInput()
{
	std::fill(std::begin(m_aCurrentKeyStates), std::end(m_aCurrentKeyStates), false);
	std::fill(std::begin(m_aFrameKeyStates), std::end(m_aFrameKeyStates), false);

	m_vInputEvents.reserve(32);
	m_LastUpdate = 0;
	m_UpdateTime = 0.0f;

	m_InputCounter = 1;
	m_InputGrabbed = false;

	m_MouseFocus = true;

	m_CompositionCursor = 0;
	m_CandidateSelectedIndex = -1;

	m_aDropFile[0] = '\0';
}

void CInput::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();

	// SDLTODO: you can do better
	m_pWindow = SDL_GetWindows(nullptr)[0];
	dbg_assert(m_pWindow, "SDL Window not found");

	StopTextInput();
	MouseModeRelative();
	InitJoysticks();
}

void CInput::Shutdown()
{
	CloseJoysticks();
}

void CInput::InitJoysticks()
{
	if(!SDL_WasInit(SDL_INIT_JOYSTICK) || !SDL_InitSubSystem(SDL_INIT_JOYSTICK))
	{
		dbg_msg("joystick", "Unable to init SDL joystick system: %s", SDL_GetError());
		return;
	}

	int NumJoysticks;
	SDL_JoystickID *JoystickIds = SDL_GetJoysticks(&NumJoysticks);

	dbg_msg("joystick", "%d joystick(s) found", NumJoysticks);
	for(int i = 0; i < NumJoysticks; i++)
		OpenJoystick(JoystickIds[i]);
	UpdateActiveJoystick();

	Console()->Chain("inp_controller_guid", ConchainJoystickGuidChanged, this);
}

bool CInput::OpenJoystick(int JoystickId)
{
	SDL_Joystick *pJoystick = SDL_OpenJoystick(JoystickId);
	if(!pJoystick)
	{
		dbg_msg("joystick", "Could not open joystick %d: '%s'", JoystickId, SDL_GetError());
		return false;
	}
	if(std::find_if(m_vJoysticks.begin(), m_vJoysticks.end(), [pJoystick](const CJoystick &Joystick) -> bool { return Joystick.m_pDelegate == pJoystick; }) != m_vJoysticks.end())
	{
		// Joystick has already been added
		return false;
	}
	m_vJoysticks.emplace_back(this, m_vJoysticks.size(), pJoystick);
	const CJoystick &Joystick = m_vJoysticks[m_vJoysticks.size() - 1];
	dbg_msg("joystick", "Opened joystick %d '%s' (%d axes, %d buttons, %d balls, %d hats)", JoystickId, Joystick.GetName(),
		Joystick.GetNumAxes(), Joystick.GetNumButtons(), Joystick.GetNumBalls(), Joystick.GetNumHats());
	return true;
}

void CInput::UpdateActiveJoystick()
{
	m_pActiveJoystick = nullptr;
	if(m_vJoysticks.empty())
		return;
	for(auto &Joystick : m_vJoysticks)
	{
		if(str_comp(Joystick.GetGUID(), g_Config.m_InpControllerGUID) == 0)
		{
			m_pActiveJoystick = &Joystick;
			return;
		}
	}
	// Fall back to first joystick if no match was found
	if(!m_pActiveJoystick)
		m_pActiveJoystick = &m_vJoysticks.front();
}

void CInput::ConchainJoystickGuidChanged(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		static_cast<CInput *>(pUserData)->UpdateActiveJoystick();
	}
}

float CInput::GetJoystickDeadzone()
{
	return minimum(g_Config.m_InpControllerTolerance / 50.0f, 0.995f);
}

CInput::CJoystick::CJoystick(CInput *pInput, int Index, SDL_Joystick *pDelegate)
{
	m_pInput = pInput;
	m_Index = Index;
	m_pDelegate = pDelegate;
	m_NumAxes = SDL_GetNumJoystickAxes(pDelegate);
	m_NumButtons = SDL_GetNumJoystickButtons(pDelegate);
	m_NumBalls = SDL_GetNumJoystickBalls(pDelegate);
	m_NumHats = SDL_GetNumJoystickHats(pDelegate);
	str_copy(m_aName, SDL_GetJoystickName(pDelegate));
	SDL_GUIDToString(SDL_GetJoystickGUID(pDelegate), m_aGUID, sizeof(m_aGUID));
	m_InstanceId = SDL_GetJoystickID(pDelegate);
}

void CInput::CloseJoysticks()
{
	if(SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

	m_vJoysticks.clear();
	m_pActiveJoystick = nullptr;
}

void CInput::SetActiveJoystick(size_t Index)
{
	m_pActiveJoystick = &m_vJoysticks[Index];
	str_copy(g_Config.m_InpControllerGUID, m_pActiveJoystick->GetGUID());
}

float CInput::CJoystick::GetAxisValue(int Axis)
{
	return (SDL_GetJoystickAxis(m_pDelegate, Axis) - SDL_JOYSTICK_AXIS_MIN) / (float)(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN) * 2.0f - 1.0f;
}

void CInput::CJoystick::GetJoystickHatKeys(int Hat, int HatValue, int (&aHatKeys)[2])
{
	if(HatValue & SDL_HAT_UP)
		aHatKeys[0] = KEY_JOY_HAT0_UP + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else if(HatValue & SDL_HAT_DOWN)
		aHatKeys[0] = KEY_JOY_HAT0_DOWN + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else
		aHatKeys[0] = KEY_UNKNOWN;

	if(HatValue & SDL_HAT_LEFT)
		aHatKeys[1] = KEY_JOY_HAT0_LEFT + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else if(HatValue & SDL_HAT_RIGHT)
		aHatKeys[1] = KEY_JOY_HAT0_RIGHT + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else
		aHatKeys[1] = KEY_UNKNOWN;
}

void CInput::CJoystick::GetHatValue(int Hat, int (&aHatKeys)[2])
{
	GetJoystickHatKeys(Hat, SDL_GetJoystickHat(m_pDelegate, Hat), aHatKeys);
}

bool CInput::CJoystick::Relative(float *pX, float *pY)
{
	if(!Input()->m_MouseFocus || !Input()->m_InputGrabbed || !g_Config.m_InpControllerEnable)
		return false;

	const vec2 RawJoystickPos = vec2(GetAxisValue(g_Config.m_InpControllerX), GetAxisValue(g_Config.m_InpControllerY));
	const float Len = length(RawJoystickPos);
	const float DeadZone = Input()->GetJoystickDeadzone();
	if(Len > DeadZone)
	{
		const float Factor = 2500.0f * Input()->GetUpdateTime() * maximum((Len - DeadZone) / (1.0f - DeadZone), 0.001f) / Len;
		*pX = RawJoystickPos.x * Factor;
		*pY = RawJoystickPos.y * Factor;
		return true;
	}
	return false;
}

bool CInput::CJoystick::Absolute(float *pX, float *pY)
{
	if(!Input()->m_MouseFocus || !Input()->m_InputGrabbed || !g_Config.m_InpControllerEnable)
		return false;

	const vec2 RawJoystickPos = vec2(GetAxisValue(g_Config.m_InpControllerX), GetAxisValue(g_Config.m_InpControllerY));
	const float DeadZone = Input()->GetJoystickDeadzone();
	if(dot(RawJoystickPos, RawJoystickPos) > DeadZone * DeadZone)
	{
		*pX = RawJoystickPos.x;
		*pY = RawJoystickPos.y;
		return true;
	}
	return false;
}

bool CInput::MouseRelative(float *pX, float *pY)
{
	if(!m_MouseFocus || !m_InputGrabbed)
		return false;
	SDL_GetRelativeMouseState(pX, pY);
	return *pX != 0.0f || *pY != 0.0f;
}

void CInput::MouseModeAbsolute()
{
	m_InputGrabbed = false;
	SDL_SetWindowRelativeMouseMode(m_pWindow, false);
	Graphics()->SetWindowGrab(false);
}

void CInput::MouseModeRelative()
{
	m_InputGrabbed = true;
	SDL_SetWindowRelativeMouseMode(m_pWindow, true);
	Graphics()->SetWindowGrab(true);
	// Clear pending relative mouse motion
	SDL_GetRelativeMouseState(nullptr, nullptr);
}

vec2 CInput::NativeMousePos() const
{
	vec2 Position;
	SDL_GetMouseState(&Position.x, &Position.y);
	return Position;
}

bool CInput::NativeMousePressed(int Index) const
{
	int i = SDL_GetMouseState(nullptr, nullptr);
	return (i & SDL_BUTTON_MASK(Index)) != 0;
}

const std::vector<IInput::CTouchFingerState> &CInput::TouchFingerStates() const
{
	return m_vTouchFingerStates;
}

void CInput::ClearTouchDeltas()
{
	for(CTouchFingerState &TouchFingerState : m_vTouchFingerStates)
	{
		TouchFingerState.m_Delta = vec2(0.0f, 0.0f);
	}
}

std::string CInput::GetClipboardText()
{
	char *pClipboardText = SDL_GetClipboardText();
	std::string ClipboardText = pClipboardText;
	SDL_free(pClipboardText);
	return ClipboardText;
}

void CInput::SetClipboardText(const char *pText)
{
	SDL_SetClipboardText(pText);
}

void CInput::StartTextInput()
{
	SDL_StartTextInput(m_pWindow);
}

void CInput::StopTextInput()
{
	SDL_StopTextInput(m_pWindow);
	m_CompositionString = "";
	m_CompositionCursor = 0;
	m_vCandidates.clear();
}

void CInput::EnsureScreenKeyboardShown()
{
	if(!SDL_HasScreenKeyboardSupport() ||
		Graphics()->IsScreenKeyboardShown())
	{
		return;
	}
	SDL_StopTextInput(m_pWindow);
	SDL_StartTextInput(m_pWindow);
}

void CInput::ClearComposition() const
{
	SDL_ClearComposition(m_pWindow);
}

void CInput::ConsumeEvents(std::function<void(const CEvent &Event)> Consumer) const
{
	for(const CEvent &Event : m_vInputEvents)
	{
		// Only propagate valid input events
		if(Event.m_InputCount == m_InputCounter)
		{
			Consumer(Event);
		}
	}
}

void CInput::Clear()
{
	std::fill(std::begin(m_aFrameKeyStates), std::end(m_aFrameKeyStates), false);
	m_vInputEvents.clear();
	ClearTouchDeltas();
}

float CInput::GetUpdateTime() const
{
	return m_UpdateTime;
}

bool CInput::KeyIsPressed(int Key) const
{
	dbg_assert(Key >= KEY_FIRST && Key < KEY_LAST, "Key invalid: %d", Key);
	return m_aCurrentKeyStates[Key];
}

bool CInput::KeyPress(int Key) const
{
	dbg_assert(Key >= KEY_FIRST && Key < KEY_LAST, "Key invalid: %d", Key);
	return m_aFrameKeyStates[Key];
}

const char *CInput::KeyName(int Key) const
{
	dbg_assert(Key >= KEY_FIRST && Key < KEY_LAST, "Key invalid: %d", Key);
	return g_aaKeyStrings[Key];
}

int CInput::FindKeyByName(const char *pKeyName) const
{
	// check for numeric
	if(pKeyName[0] == '&')
	{
		int Key = str_toint(pKeyName + 1);
		if(Key > KEY_FIRST && Key < KEY_LAST)
			return Key; // numeric
	}

	// search for key
	for(int Key = KEY_FIRST; Key < KEY_LAST; Key++)
	{
		if(str_comp_nocase(pKeyName, KeyName(Key)) == 0)
			return Key;
	}

	return KEY_UNKNOWN;
}

void CInput::HandleJoystickAxisMotionEvent(const SDL_JoyAxisEvent &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceId() != Event.which)
		return;
	if(Event.axis >= NUM_JOYSTICK_AXES)
		return;

	const int LeftKey = KEY_JOY_AXIS_0_LEFT + 2 * Event.axis;
	const int RightKey = LeftKey + 1;
	const float DeadZone = GetJoystickDeadzone();

	if(Event.value <= SDL_JOYSTICK_AXIS_MIN * DeadZone && !m_aCurrentKeyStates[LeftKey])
	{
		AddKeyEvent(LeftKey, IInput::FLAG_PRESS);
	}
	else if(Event.value > SDL_JOYSTICK_AXIS_MIN * DeadZone && m_aCurrentKeyStates[LeftKey])
	{
		AddKeyEvent(LeftKey, IInput::FLAG_RELEASE);
	}

	if(Event.value >= SDL_JOYSTICK_AXIS_MAX * DeadZone && !m_aCurrentKeyStates[RightKey])
	{
		AddKeyEvent(RightKey, IInput::FLAG_PRESS);
	}
	else if(Event.value < SDL_JOYSTICK_AXIS_MAX * DeadZone && m_aCurrentKeyStates[RightKey])
	{
		AddKeyEvent(RightKey, IInput::FLAG_RELEASE);
	}
}

void CInput::HandleJoystickButtonEvent(const SDL_JoyButtonEvent &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceId() != Event.which)
		return;
	if(Event.button >= NUM_JOYSTICK_BUTTONS)
		return;

	const int Key = Event.button + KEY_JOYSTICK_BUTTON_0;

	if(Event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN)
	{
		AddKeyEvent(Key, IInput::FLAG_PRESS);
	}
	else if(Event.type == SDL_EVENT_JOYSTICK_BUTTON_UP)
	{
		AddKeyEvent(Key, IInput::FLAG_RELEASE);
	}
}

void CInput::HandleJoystickHatMotionEvent(const SDL_JoyHatEvent &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceId() != Event.which)
		return;
	if(Event.hat >= NUM_JOYSTICK_HATS)
		return;

	int aHatKeys[2];
	CJoystick::GetJoystickHatKeys(Event.hat, Event.value, aHatKeys);

	for(int Key = KEY_JOY_HAT0_UP + Event.hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key <= KEY_JOY_HAT0_RIGHT + Event.hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key++)
	{
		if(Key != aHatKeys[0] && Key != aHatKeys[1] && m_aCurrentKeyStates[Key])
		{
			AddKeyEvent(Key, IInput::FLAG_RELEASE);
		}
	}

	for(int CurrentKey : aHatKeys)
	{
		if(CurrentKey != KEY_UNKNOWN && !m_aCurrentKeyStates[CurrentKey])
		{
			AddKeyEvent(CurrentKey, IInput::FLAG_PRESS);
		}
	}
}

void CInput::HandleJoystickAddedEvent(const SDL_JoyDeviceEvent &Event)
{
	if(OpenJoystick(Event.which))
	{
		UpdateActiveJoystick();
	}
}

void CInput::HandleJoystickRemovedEvent(const SDL_JoyDeviceEvent &Event)
{
	auto RemovedJoystick = std::find_if(m_vJoysticks.begin(), m_vJoysticks.end(), [Event](const CJoystick &Joystick) -> bool { return Joystick.GetInstanceId() == Event.which; });
	if(RemovedJoystick != m_vJoysticks.end())
	{
		dbg_msg("joystick", "Closed joystick %d '%s'", (*RemovedJoystick).GetIndex(), (*RemovedJoystick).GetName());
		auto NextJoystick = m_vJoysticks.erase(RemovedJoystick);
		// Adjust indices of following joysticks
		while(NextJoystick != m_vJoysticks.end())
		{
			(*NextJoystick).m_Index--;
			++NextJoystick;
		}
		UpdateActiveJoystick();
	}
}

void CInput::HandleTouchDownEvent(const SDL_TouchFingerEvent &Event)
{
	CTouchFingerState TouchFingerState;
	TouchFingerState.m_Finger.m_DeviceId = Event.touchID;
	TouchFingerState.m_Finger.m_FingerId = Event.fingerID;
	TouchFingerState.m_Position = vec2(Event.x, Event.y);
	TouchFingerState.m_Delta = vec2(Event.dx, Event.dy);
	TouchFingerState.m_PressTime = time_get_nanoseconds();
	m_vTouchFingerStates.emplace_back(TouchFingerState);
}

void CInput::HandleTouchUpEvent(const SDL_TouchFingerEvent &Event)
{
	auto FoundState = std::find_if(m_vTouchFingerStates.begin(), m_vTouchFingerStates.end(), [Event](const CTouchFingerState &State) {
		return State.m_Finger.m_DeviceId == Event.touchID && State.m_Finger.m_FingerId == Event.fingerID;
	});
	if(FoundState != m_vTouchFingerStates.end())
	{
		m_vTouchFingerStates.erase(FoundState);
	}
}

void CInput::HandleTouchMotionEvent(const SDL_TouchFingerEvent &Event)
{
	auto FoundState = std::find_if(m_vTouchFingerStates.begin(), m_vTouchFingerStates.end(), [Event](const CTouchFingerState &State) {
		return State.m_Finger.m_DeviceId == Event.touchID && State.m_Finger.m_FingerId == Event.fingerID;
	});
	if(FoundState != m_vTouchFingerStates.end())
	{
		FoundState->m_Position = vec2(Event.x, Event.y);
		FoundState->m_Delta += vec2(Event.dx, Event.dy);
	}
}

void CInput::HandleTextEditingEvent(const char *pText, int Start, int Length)
{
	if(pText[0] != '\0')
	{
		m_CompositionString = pText;
		m_CompositionCursor = 0;
		for(int i = 0; i < Start; i++)
		{
			m_CompositionCursor = str_utf8_forward(m_CompositionString.c_str(), m_CompositionCursor);
		}
		// Length is currently unused on Windows and will always be 0, so we don't support selecting composition text
		AddTextEvent("");
	}
	else
	{
		m_CompositionString = "";
		m_CompositionCursor = 0;
	}
}

void CInput::SetCompositionWindowPosition(float X, float Y, float H)
{
	SDL_Rect Rect;
	Rect.x = X / m_pGraphics->ScreenHiDPIScale();
	Rect.y = Y / m_pGraphics->ScreenHiDPIScale();
	Rect.h = H / m_pGraphics->ScreenHiDPIScale();
	Rect.w = 0;
	// TODOSDL: we should use the cursor param, this is jank
	SDL_SetTextInputArea(m_pWindow, &Rect, 0);
}

static int TranslateKeyEventKey(const SDL_KeyboardEvent &KeyEvent)
{
	// See SDL_Keymod for possible modifiers:
	// NONE   =     0
	// LSHIFT =     1
	// RSHIFT =     2
	// LCTRL  =    64
	// RCTRL  =   128
	// LALT   =   256
	// RALT   =   512
	// LGUI   =  1024
	// RGUI   =  2048
	// NUM    =  4096
	// CAPS   =  8192
	// MODE   = 16384
	// Sum if you want to ignore multiple modifiers.
	if(KeyEvent.mod & g_Config.m_InpIgnoredModifiers)
	{
		return KEY_UNKNOWN;
	}

	int Key = g_Config.m_InpTranslatedKeys ? SDL_GetScancodeFromKey(KeyEvent.key, nullptr) : KeyEvent.scancode;

#if defined(CONF_PLATFORM_ANDROID)
	// Translate the Android back-button to the escape-key so it can be used to open/close the menu, close popups etc.
	if(Key == KEY_AC_BACK)
	{
		Key = KEY_ESCAPE;
	}
#endif

	return Key;
}

static int TranslateMouseButtonEventKey(const SDL_MouseButtonEvent &MouseButtonEvent)
{
	switch(MouseButtonEvent.button)
	{
	case SDL_BUTTON_LEFT:
		return KEY_MOUSE_1;
	case SDL_BUTTON_RIGHT:
		return KEY_MOUSE_2;
	case SDL_BUTTON_MIDDLE:
		return KEY_MOUSE_3;
	case SDL_BUTTON_X1:
		return KEY_MOUSE_4;
	case SDL_BUTTON_X2:
		return KEY_MOUSE_5;
	case 6:
		return KEY_MOUSE_6;
	case 7:
		return KEY_MOUSE_7;
	case 8:
		return KEY_MOUSE_8;
	case 9:
		return KEY_MOUSE_9;
	default:
		return KEY_UNKNOWN;
	}
}

static int TranslateMouseWheelEventKey(const SDL_MouseWheelEvent &MouseWheelEvent)
{
	if(MouseWheelEvent.y > 0)
	{
		return KEY_MOUSE_WHEEL_UP;
	}
	else if(MouseWheelEvent.y < 0)
	{
		return KEY_MOUSE_WHEEL_DOWN;
	}
	else if(MouseWheelEvent.x > 0)
	{
		return KEY_MOUSE_WHEEL_RIGHT;
	}
	else if(MouseWheelEvent.x < 0)
	{
		return KEY_MOUSE_WHEEL_LEFT;
	}
	else
	{
		return KEY_UNKNOWN;
	}
}

int CInput::Update()
{
	const int64_t Now = time_get();
	if(m_LastUpdate != 0)
	{
		const float Diff = (Now - m_LastUpdate) / (float)time_freq();
		m_UpdateTime = m_UpdateTime == 0.0f ? Diff : (m_UpdateTime * 0.8f + Diff * 0.2f);
	}
	m_LastUpdate = Now;

	// keep the counter between 1..0xFFFFFFFF, 0 means not pressed
	m_InputCounter = (m_InputCounter % std::numeric_limits<decltype(m_InputCounter)>::max()) + 1;

	SDL_Event Event;
	bool IgnoreKeys = false;

	const auto &&AddKeyEventChecked = [&](int Key, int Flags) {
		if(Key != KEY_UNKNOWN && !IgnoreKeys && (!(Flags & IInput::FLAG_PRESS) || !HasComposition()))
		{
			AddKeyEvent(Key, Flags);
		}
	};

	while(SDL_PollEvent(&Event))
	{
		switch(Event.type)
		{
		case SDL_EVENT_TEXT_EDITING:
			HandleTextEditingEvent(Event.edit.text, Event.edit.start, Event.edit.length);
			break;

		case SDL_EVENT_TEXT_INPUT:
			m_CompositionString = "";
			m_CompositionCursor = 0;
			AddTextEvent(Event.text.text);
			break;

		case SDL_EVENT_TEXT_EDITING_CANDIDATES:
			// TODO: handle Event.edit_candidates.horizontal
			m_vCandidates.clear();
			m_vCandidates.reserve(Event.edit_candidates.num_candidates);
			for(auto i = 0; i < Event.edit_candidates.num_candidates; ++i)
				m_vCandidates.emplace_back(Event.edit_candidates.candidates[i]);
			m_CandidateSelectedIndex = Event.edit_candidates.selected_candidate;
			break;

		// handle keys
		case SDL_EVENT_KEY_DOWN:
			AddKeyEventChecked(TranslateKeyEventKey(Event.key), IInput::FLAG_PRESS);
			break;

		case SDL_EVENT_KEY_UP:
			AddKeyEventChecked(TranslateKeyEventKey(Event.key), IInput::FLAG_RELEASE);
			break;

		// handle the joystick events
		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			HandleJoystickAxisMotionEvent(Event.jaxis);
			break;

		case SDL_EVENT_JOYSTICK_BUTTON_UP:
		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			HandleJoystickButtonEvent(Event.jbutton);
			break;

		case SDL_EVENT_JOYSTICK_HAT_MOTION:
			HandleJoystickHatMotionEvent(Event.jhat);
			break;

		case SDL_EVENT_JOYSTICK_ADDED:
			HandleJoystickAddedEvent(Event.jdevice);
			break;

		case SDL_EVENT_JOYSTICK_REMOVED:
			HandleJoystickRemovedEvent(Event.jdevice);
			break;

		// handle mouse buttons
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			AddKeyEventChecked(TranslateMouseButtonEventKey(Event.button), IInput::FLAG_PRESS);
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			AddKeyEventChecked(TranslateMouseButtonEventKey(Event.button), IInput::FLAG_RELEASE);
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			AddKeyEventChecked(TranslateMouseWheelEventKey(Event.wheel), IInput::FLAG_PRESS | IInput::FLAG_RELEASE);
			break;

		case SDL_EVENT_FINGER_DOWN:
			HandleTouchDownEvent(Event.tfinger);
			break;

		case SDL_EVENT_FINGER_UP:
			HandleTouchUpEvent(Event.tfinger);
			break;

		case SDL_EVENT_FINGER_MOTION:
			HandleTouchMotionEvent(Event.tfinger);
			break;

		case SDL_EVENT_WINDOW_MOVED:
			Graphics()->Move(Event.window.data1, Event.window.data2);
			break;

		// listen to size changes, this includes our manual changes and the ones by the window manager
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			Graphics()->GotResized(Event.window.data1, Event.window.data2, -1);
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			if(m_InputGrabbed)
			{
				MouseModeRelative();
				// Clear pending relative mouse motion
				SDL_GetRelativeMouseState(nullptr, nullptr);
			}
			m_MouseFocus = true;
			IgnoreKeys = true;
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			std::fill(std::begin(m_aCurrentKeyStates), std::end(m_aCurrentKeyStates), false);
			m_MouseFocus = false;
			IgnoreKeys = true;
			if(m_InputGrabbed)
			{
				MouseModeAbsolute();
				// Remember that we had relative mouse
				m_InputGrabbed = true;
			}
			break;

		case SDL_EVENT_WINDOW_MINIMIZED:
#if defined(CONF_PLATFORM_ANDROID) // Save the config when minimized on Android.
			m_pConfigManager->Save();
#endif
			Graphics()->WindowDestroyNtf(Event.window.windowID);
			break;

		case SDL_EVENT_WINDOW_MAXIMIZED:
#if defined(CONF_PLATFORM_MACOS) // TODOSDL: remove this when fixed in SDL
			MouseModeAbsolute();
			MouseModeRelative();
#endif
			[[fallthrough]];
		case SDL_EVENT_WINDOW_RESTORED:
			Graphics()->WindowCreateNtf(Event.window.windowID);
			break;

		// other messages
		case SDL_EVENT_QUIT:
			return 1;

		case SDL_EVENT_DROP_FILE:
			if(Event.drop.source)
				str_copy(m_aDropFile, Event.drop.source);
			break;
		}
	}

	return 0;
}

bool CInput::GetDropFile(char *aBuf, int Len)
{
	if(m_aDropFile[0] != '\0')
	{
		str_copy(aBuf, m_aDropFile, Len);
		m_aDropFile[0] = '\0';
		return true;
	}
	return false;
}

IEngineInput *CreateEngineInput()
{
	return new CInput;
}
