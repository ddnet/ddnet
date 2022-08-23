/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <SDL.h>

#include <base/system.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include "input.h"

//print >>f, "int inp_key_code(const char *key_name) { int i; if (!strcmp(key_name, \"-?-\")) return -1; else for (i = 0; i < 512; i++) if (!strcmp(key_strings[i], key_name)) return i; return -1; }"

// this header is protected so you don't include it from anywere
#define KEYS_INCLUDE
#include "keynames.h"
#undef KEYS_INCLUDE

// support older SDL version (pre 2.0.6)
#ifndef SDL_JOYSTICK_AXIS_MIN
#define SDL_JOYSTICK_AXIS_MIN (-32768)
#endif
#ifndef SDL_JOYSTICK_AXIS_MAX
#define SDL_JOYSTICK_AXIS_MAX 32767
#endif

void CInput::AddEvent(char *pText, int Key, int Flags)
{
	if(m_NumEvents != INPUT_BUFFER_SIZE)
	{
		m_aInputEvents[m_NumEvents].m_Key = Key;
		m_aInputEvents[m_NumEvents].m_Flags = Flags;
		if(!pText)
			m_aInputEvents[m_NumEvents].m_aText[0] = 0;
		else
			str_copy(m_aInputEvents[m_NumEvents].m_aText, pText);
		m_aInputEvents[m_NumEvents].m_InputCount = m_InputCounter;
		m_NumEvents++;
	}
}

CInput::CInput()
{
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	mem_zero(m_aInputState, sizeof(m_aInputState));

	m_InputCounter = 1;
	m_InputGrabbed = 0;

	m_MouseDoubleClick = false;

	m_NumEvents = 0;
	m_MouseFocus = true;

	m_VideoRestartNeeded = 0;
	m_pClipboardText = NULL;

	m_NumTextInputInstances = 0;
	m_EditingTextLen = -1;
	m_aEditingText[0] = 0;
}

void CInput::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	// increase ime instance counter for menu
	SetIMEState(true);
	MouseModeRelative();

	InitJoysticks();
}

void CInput::Shutdown()
{
	SDL_free(m_pClipboardText);
	CloseJoysticks();
}

void CInput::InitJoysticks()
{
	if(!SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
		{
			dbg_msg("joystick", "Unable to init SDL joystick system: %s", SDL_GetError());
			return;
		}
	}

	int NumJoysticks = SDL_NumJoysticks();
	if(NumJoysticks > 0)
	{
		dbg_msg("joystick", "%d joystick(s) found", NumJoysticks);
		int ActualIndex = 0;
		for(int i = 0; i < NumJoysticks; i++)
		{
			SDL_Joystick *pJoystick = SDL_JoystickOpen(i);
			if(!pJoystick)
			{
				dbg_msg("joystick", "Could not open joystick %d: '%s'", i, SDL_GetError());
				continue;
			}
			m_vJoysticks.emplace_back(this, ActualIndex, pJoystick);
			const CJoystick &Joystick = m_vJoysticks[m_vJoysticks.size() - 1];
			dbg_msg("joystick", "Opened Joystick %d '%s' (%d axes, %d buttons, %d balls, %d hats)", i, Joystick.GetName(),
				Joystick.GetNumAxes(), Joystick.GetNumButtons(), Joystick.GetNumBalls(), Joystick.GetNumHats());
			ActualIndex++;
		}
		if(ActualIndex > 0)
		{
			UpdateActiveJoystick();
			Console()->Chain("joystick_guid", ConchainJoystickGuidChanged, this);
		}
	}
	else
	{
		dbg_msg("joystick", "No joysticks found");
	}
}

void CInput::UpdateActiveJoystick()
{
	if(m_vJoysticks.empty())
		return;
	m_pActiveJoystick = nullptr;
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
		m_pActiveJoystick = &m_vJoysticks[0]; // NOLINT(readability-container-data-pointer)
}

void CInput::ConchainJoystickGuidChanged(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	static_cast<CInput *>(pUserData)->UpdateActiveJoystick();
}

float CInput::GetJoystickDeadzone()
{
	return g_Config.m_InpControllerTolerance / 50.0f;
}

CInput::CJoystick::CJoystick(CInput *pInput, int Index, SDL_Joystick *pDelegate)
{
	m_pInput = pInput;
	m_Index = Index;
	m_pDelegate = pDelegate;
	m_NumAxes = SDL_JoystickNumAxes(pDelegate);
	m_NumButtons = SDL_JoystickNumButtons(pDelegate);
	m_NumBalls = SDL_JoystickNumBalls(pDelegate);
	m_NumHats = SDL_JoystickNumHats(pDelegate);
	str_copy(m_aName, SDL_JoystickName(pDelegate));
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(pDelegate), m_aGUID, sizeof(m_aGUID));
	m_InstanceID = SDL_JoystickInstanceID(pDelegate);
}

void CInput::CloseJoysticks()
{
	if(SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

	m_vJoysticks.clear();
	m_pActiveJoystick = nullptr;
}

void CInput::SelectNextJoystick()
{
	const int Num = m_vJoysticks.size();
	if(Num > 1)
	{
		m_pActiveJoystick = &m_vJoysticks[(m_pActiveJoystick->GetIndex() + 1) % Num];
		str_copy(g_Config.m_InpControllerGUID, m_pActiveJoystick->GetGUID());
	}
}

float CInput::CJoystick::GetAxisValue(int Axis)
{
	return (SDL_JoystickGetAxis(m_pDelegate, Axis) - SDL_JOYSTICK_AXIS_MIN) / float(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN) * 2.0f - 1.0f;
}

int CInput::CJoystick::GetJoystickHatKey(int Hat, int HatValue)
{
	switch(HatValue)
	{
	case SDL_HAT_LEFTUP: return KEY_JOY_HAT0_LEFTUP + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	case SDL_HAT_UP: return KEY_JOY_HAT0_UP + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	case SDL_HAT_RIGHTUP: return KEY_JOY_HAT0_RIGHTUP + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	case SDL_HAT_LEFT: return KEY_JOY_HAT0_LEFT + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	case SDL_HAT_RIGHT: return KEY_JOY_HAT0_RIGHT + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	case SDL_HAT_LEFTDOWN: return KEY_JOY_HAT0_LEFTDOWN + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	case SDL_HAT_DOWN: return KEY_JOY_HAT0_DOWN + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	case SDL_HAT_RIGHTDOWN: return KEY_JOY_HAT0_RIGHTDOWN + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	}
	return -1;
}

int CInput::CJoystick::GetHatValue(int Hat)
{
	return GetJoystickHatKey(Hat, SDL_JoystickGetHat(m_pDelegate, Hat));
}

bool CInput::CJoystick::Relative(float *pX, float *pY)
{
	if(!g_Config.m_InpControllerEnable)
		return false;

	const vec2 RawJoystickPos = vec2(GetAxisValue(g_Config.m_InpControllerX), GetAxisValue(g_Config.m_InpControllerY));
	const float Len = length(RawJoystickPos);
	const float DeadZone = Input()->GetJoystickDeadzone();
	if(Len > DeadZone)
	{
		const float Factor = 0.1f * maximum((Len - DeadZone) / (1 - DeadZone), 0.001f) / Len;
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

	int nx = 0, ny = 0;
#if defined(CONF_PLATFORM_ANDROID) // No relative mouse on Android
	static int s_LastX = 0;
	static int s_LastY = 0;
	SDL_GetMouseState(&nx, &ny);
	int XTmp = nx - s_LastX;
	int YTmp = ny - s_LastY;
	s_LastX = nx;
	s_LastY = ny;
	nx = XTmp;
	ny = YTmp;
#else
	SDL_GetRelativeMouseState(&nx, &ny);
#endif

	*pX = nx;
	*pY = ny;
	return *pX != 0.0f || *pY != 0.0f;
}

void CInput::MouseModeAbsolute()
{
	m_InputGrabbed = 0;
	SDL_SetRelativeMouseMode(SDL_FALSE);
	Graphics()->SetWindowGrab(false);
}

void CInput::MouseModeRelative()
{
	m_InputGrabbed = 1;
#if !defined(CONF_PLATFORM_ANDROID) // No relative mouse on Android
	SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
	Graphics()->SetWindowGrab(true);
	// Clear pending relative mouse motion
	SDL_GetRelativeMouseState(0x0, 0x0);
}

void CInput::NativeMousePos(int *pX, int *pY) const
{
	SDL_GetMouseState(pX, pY);
}

bool CInput::NativeMousePressed(int Index)
{
	int i = SDL_GetMouseState(NULL, NULL);
	return (i & SDL_BUTTON(Index)) != 0;
}

bool CInput::MouseDoubleClick()
{
	if(m_MouseDoubleClick)
	{
		m_MouseDoubleClick = false;
		return true;
	}
	return false;
}

const char *CInput::GetClipboardText()
{
	SDL_free(m_pClipboardText);
	m_pClipboardText = SDL_GetClipboardText();
	return m_pClipboardText;
}

void CInput::SetClipboardText(const char *pText)
{
	SDL_SetClipboardText(pText);
}

void CInput::Clear()
{
	mem_zero(m_aInputState, sizeof(m_aInputState));
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	m_NumEvents = 0;
}

bool CInput::KeyState(int Key) const
{
	if(Key < 0 || Key >= KEY_LAST)
		return false;
	return m_aInputState[Key];
}

void CInput::UpdateMouseState()
{
	const int MouseState = SDL_GetMouseState(NULL, NULL);
	if(MouseState & SDL_BUTTON(SDL_BUTTON_LEFT))
		m_aInputState[KEY_MOUSE_1] = 1;
	if(MouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
		m_aInputState[KEY_MOUSE_2] = 1;
	if(MouseState & SDL_BUTTON(SDL_BUTTON_MIDDLE))
		m_aInputState[KEY_MOUSE_3] = 1;
	if(MouseState & SDL_BUTTON(SDL_BUTTON_X1))
		m_aInputState[KEY_MOUSE_4] = 1;
	if(MouseState & SDL_BUTTON(SDL_BUTTON_X2))
		m_aInputState[KEY_MOUSE_5] = 1;
	if(MouseState & SDL_BUTTON(6))
		m_aInputState[KEY_MOUSE_6] = 1;
	if(MouseState & SDL_BUTTON(7))
		m_aInputState[KEY_MOUSE_7] = 1;
	if(MouseState & SDL_BUTTON(8))
		m_aInputState[KEY_MOUSE_8] = 1;
	if(MouseState & SDL_BUTTON(9))
		m_aInputState[KEY_MOUSE_9] = 1;
}

void CInput::UpdateJoystickState()
{
	if(!g_Config.m_InpControllerEnable)
		return;
	IJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick)
		return;

	const float DeadZone = GetJoystickDeadzone();
	for(int Axis = 0; Axis < pJoystick->GetNumAxes(); Axis++)
	{
		const float Value = pJoystick->GetAxisValue(Axis);
		const int LeftKey = KEY_JOY_AXIS_0_LEFT + 2 * Axis;
		const int RightKey = LeftKey + 1;
		m_aInputState[LeftKey] = Value <= -DeadZone;
		m_aInputState[RightKey] = Value >= DeadZone;
	}

	for(int Hat = 0; Hat < pJoystick->GetNumHats(); Hat++)
	{
		const int HatState = pJoystick->GetHatValue(Hat);
		for(int Key = KEY_JOY_HAT0_LEFTUP + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key <= KEY_JOY_HAT0_RIGHTDOWN + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key++)
			m_aInputState[Key] = Key == HatState;
	}
}

void CInput::HandleJoystickAxisMotionEvent(const SDL_Event &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceID() != Event.jaxis.which)
		return;
	if(Event.jaxis.axis >= NUM_JOYSTICK_AXES)
		return;

	const int LeftKey = KEY_JOY_AXIS_0_LEFT + 2 * Event.jaxis.axis;
	const int RightKey = LeftKey + 1;
	const float DeadZone = GetJoystickDeadzone();

	if(Event.jaxis.value <= SDL_JOYSTICK_AXIS_MIN * DeadZone && !m_aInputState[LeftKey])
	{
		m_aInputState[LeftKey] = true;
		m_aInputCount[LeftKey] = m_InputCounter;
		AddEvent(0, LeftKey, IInput::FLAG_PRESS);
	}
	else if(Event.jaxis.value > SDL_JOYSTICK_AXIS_MIN * DeadZone && m_aInputState[LeftKey])
	{
		m_aInputState[LeftKey] = false;
		AddEvent(0, LeftKey, IInput::FLAG_RELEASE);
	}

	if(Event.jaxis.value >= SDL_JOYSTICK_AXIS_MAX * DeadZone && !m_aInputState[RightKey])
	{
		m_aInputState[RightKey] = true;
		m_aInputCount[RightKey] = m_InputCounter;
		AddEvent(0, RightKey, IInput::FLAG_PRESS);
	}
	else if(Event.jaxis.value < SDL_JOYSTICK_AXIS_MAX * DeadZone && m_aInputState[RightKey])
	{
		m_aInputState[RightKey] = false;
		AddEvent(0, RightKey, IInput::FLAG_RELEASE);
	}
}

void CInput::HandleJoystickButtonEvent(const SDL_Event &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceID() != Event.jbutton.which)
		return;
	if(Event.jbutton.button >= NUM_JOYSTICK_BUTTONS)
		return;

	const int Key = Event.jbutton.button + KEY_JOYSTICK_BUTTON_0;

	if(Event.type == SDL_JOYBUTTONDOWN)
	{
		m_aInputState[Key] = true;
		m_aInputCount[Key] = m_InputCounter;
		AddEvent(0, Key, IInput::FLAG_PRESS);
	}
	else if(Event.type == SDL_JOYBUTTONUP)
	{
		m_aInputState[Key] = false;
		AddEvent(0, Key, IInput::FLAG_RELEASE);
	}
}

void CInput::HandleJoystickHatMotionEvent(const SDL_Event &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceID() != Event.jhat.which)
		return;
	if(Event.jhat.hat >= NUM_JOYSTICK_HATS)
		return;

	const int CurrentKey = CJoystick::GetJoystickHatKey(Event.jhat.hat, Event.jhat.value);

	for(int Key = KEY_JOY_HAT0_LEFTUP + Event.jhat.hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key <= KEY_JOY_HAT0_RIGHTDOWN + Event.jhat.hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key++)
	{
		if(Key != CurrentKey && m_aInputState[Key])
		{
			m_aInputState[Key] = false;
			AddEvent(0, Key, IInput::FLAG_RELEASE);
		}
	}

	if(CurrentKey >= 0)
	{
		m_aInputState[CurrentKey] = true;
		m_aInputCount[CurrentKey] = m_InputCounter;
		AddEvent(0, CurrentKey, IInput::FLAG_PRESS);
	}
}

bool CInput::GetIMEState()
{
	return m_NumTextInputInstances > 0;
}

void CInput::SetIMEState(bool Activate)
{
	if(Activate)
	{
		if(m_NumTextInputInstances == 0)
			SDL_StartTextInput();
		m_NumTextInputInstances++;
	}
	else
	{
		if(m_NumTextInputInstances == 0)
			return;
		m_NumTextInputInstances--;
		if(m_NumTextInputInstances == 0)
			SDL_StopTextInput();
	}
}

const char *CInput::GetIMEEditingText()
{
	if(m_EditingTextLen > 0)
		return m_aEditingText;
	else
		return "";
}

int CInput::GetEditingCursor()
{
	return m_EditingCursor;
}

void CInput::SetEditingPosition(float X, float Y)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	int ScreenWidth = Graphics()->ScreenWidth();
	int ScreenHeight = Graphics()->ScreenHeight();
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	vec2 ScreenScale = vec2(ScreenWidth / (ScreenX1 - ScreenX0), ScreenHeight / (ScreenY1 - ScreenY0));

	SDL_Rect ImeWindowRect;
	ImeWindowRect.x = X * ScreenScale.x;
	ImeWindowRect.y = Y * ScreenScale.y;
	ImeWindowRect.h = 60;
	ImeWindowRect.w = 1000;

	SDL_SetTextInputRect(&ImeWindowRect);
}

int CInput::Update()
{
	// keep the counter between 1..0xFFFF, 0 means not pressed
	m_InputCounter = (m_InputCounter % 0xFFFF) + 1;

	int NumKeyStates;
	const Uint8 *pState = SDL_GetKeyboardState(&NumKeyStates);
	if(NumKeyStates >= KEY_MOUSE_1)
		NumKeyStates = KEY_MOUSE_1;
	mem_copy(m_aInputState, pState, NumKeyStates);
	mem_zero(m_aInputState + NumKeyStates, KEY_LAST - NumKeyStates);
	if(m_EditingTextLen == 0)
		m_EditingTextLen = -1;

	// these states must always be updated manually because they are not in the SDL_GetKeyboardState from SDL
	UpdateMouseState();
	UpdateJoystickState();

	SDL_Event Event;
	bool IgnoreKeys = false;
	while(SDL_PollEvent(&Event))
	{
		int Scancode = 0;
		int Action = IInput::FLAG_PRESS;
		switch(Event.type)
		{
		case SDL_TEXTEDITING:
		{
			m_EditingTextLen = str_length(Event.edit.text);
			if(m_EditingTextLen)
			{
				str_copy(m_aEditingText, Event.edit.text);
				m_EditingCursor = 0;
				for(int i = 0; i < Event.edit.start; i++)
					m_EditingCursor = str_utf8_forward(m_aEditingText, m_EditingCursor);
			}
			else
			{
				m_aEditingText[0] = 0;
			}
			break;
		}
		case SDL_TEXTINPUT:
			m_EditingTextLen = -1;
			AddEvent(Event.text.text, 0, IInput::FLAG_TEXT);
			break;
		// handle keys
		case SDL_KEYDOWN:
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
			if(!(Event.key.keysym.mod & g_Config.m_InpIgnoredModifiers))
			{
				Scancode = g_Config.m_InpTranslatedKeys ? SDL_GetScancodeFromKey(Event.key.keysym.sym) : Event.key.keysym.scancode;
			}
			break;
		case SDL_KEYUP:
			Action = IInput::FLAG_RELEASE;
			Scancode = g_Config.m_InpTranslatedKeys ? SDL_GetScancodeFromKey(Event.key.keysym.sym) : Event.key.keysym.scancode;
			break;

		// handle the joystick events
		case SDL_JOYAXISMOTION:
			HandleJoystickAxisMotionEvent(Event);
			break;

		case SDL_JOYBUTTONUP:
		case SDL_JOYBUTTONDOWN:
			HandleJoystickButtonEvent(Event);
			break;

		case SDL_JOYHATMOTION:
			HandleJoystickHatMotionEvent(Event);
			break;

		// handle mouse buttons
		case SDL_MOUSEBUTTONUP:
			Action = IInput::FLAG_RELEASE;

			[[fallthrough]];
		case SDL_MOUSEBUTTONDOWN:
			if(Event.button.button == SDL_BUTTON_LEFT)
				Scancode = KEY_MOUSE_1;
			if(Event.button.button == SDL_BUTTON_RIGHT)
				Scancode = KEY_MOUSE_2;
			if(Event.button.button == SDL_BUTTON_MIDDLE)
				Scancode = KEY_MOUSE_3;
			if(Event.button.button == SDL_BUTTON_X1)
				Scancode = KEY_MOUSE_4;
			if(Event.button.button == SDL_BUTTON_X2)
				Scancode = KEY_MOUSE_5;
			if(Event.button.button == 6)
				Scancode = KEY_MOUSE_6;
			if(Event.button.button == 7)
				Scancode = KEY_MOUSE_7;
			if(Event.button.button == 8)
				Scancode = KEY_MOUSE_8;
			if(Event.button.button == 9)
				Scancode = KEY_MOUSE_9;
			if(Event.button.button == SDL_BUTTON_LEFT)
			{
				if(Event.button.clicks % 2 == 0)
					m_MouseDoubleClick = true;
				if(Event.button.clicks == 1)
					m_MouseDoubleClick = false;
			}
			break;

		case SDL_MOUSEWHEEL:
			if(Event.wheel.y > 0)
				Scancode = KEY_MOUSE_WHEEL_UP;
			if(Event.wheel.y < 0)
				Scancode = KEY_MOUSE_WHEEL_DOWN;
			if(Event.wheel.x > 0)
				Scancode = KEY_MOUSE_WHEEL_LEFT;
			if(Event.wheel.x < 0)
				Scancode = KEY_MOUSE_WHEEL_RIGHT;
			Action |= IInput::FLAG_RELEASE;
			break;

		case SDL_WINDOWEVENT:
			// Ignore keys following a focus gain as they may be part of global
			// shortcuts
			switch(Event.window.event)
			{
			case SDL_WINDOWEVENT_MOVED:
				Graphics()->Move(Event.window.data1, Event.window.data2);
				break;
			// listen to size changes, this includes our manual changes and the ones by the window manager
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				Graphics()->GotResized(Event.window.data1, Event.window.data2, -1);
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				if(m_InputGrabbed)
				{
					// Enable this in case SDL 2.0.16 has major bugs or 2.0.18 still doesn't fix tabbing out with relative mouse
					// MouseModeRelative();
					// Clear pending relative mouse motion
					SDL_GetRelativeMouseState(0x0, 0x0);
				}
				m_MouseFocus = true;
				IgnoreKeys = true;
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				m_MouseFocus = false;
				IgnoreKeys = true;
				if(m_InputGrabbed)
				{
					// Enable this in case SDL 2.0.16 has major bugs or 2.0.18 still doesn't fix tabbing out with relative mouse
					// MouseModeAbsolute();
					// Remember that we had relative mouse
					m_InputGrabbed = true;
				}
				break;
			case SDL_WINDOWEVENT_MINIMIZED:
				Graphics()->WindowDestroyNtf(Event.window.windowID);
				break;

			case SDL_WINDOWEVENT_MAXIMIZED:
#if defined(CONF_PLATFORM_MACOS) // Todo: remove this when fixed in SDL
				MouseModeAbsolute();
				MouseModeRelative();
#endif
				[[fallthrough]];
			case SDL_WINDOWEVENT_RESTORED:
				Graphics()->WindowCreateNtf(Event.window.windowID);
				break;
			}
			break;

		// other messages
		case SDL_QUIT:
			return 1;
		}

		if(Scancode > KEY_FIRST && Scancode < g_MaxKeys && !IgnoreKeys && (!SDL_IsTextInputActive() || m_EditingTextLen == -1))
		{
			if(Action & IInput::FLAG_PRESS)
			{
				m_aInputState[Scancode] = 1;
				m_aInputCount[Scancode] = m_InputCounter;
			}
			AddEvent(0, Scancode, Action);
		}
	}

	return 0;
}

int CInput::VideoRestartNeeded()
{
	if(m_VideoRestartNeeded)
	{
		m_VideoRestartNeeded = 0;
		return 1;
	}
	return 0;
}

IEngineInput *CreateEngineInput()
{
	return new CInput;
}
