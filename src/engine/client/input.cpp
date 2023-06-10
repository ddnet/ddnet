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

// this header is protected so you don't include it from anywhere
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

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// windows.h must be included before imm.h, but clang-format requires includes to be sorted alphabetically, hence this comment.
#include <imm.h>
#endif

// for platform specific features that aren't available or are broken in SDL
#include <SDL_syswm.h>

void CInput::AddEvent(char *pText, int Key, int Flags)
{
	if(m_NumEvents != INPUT_BUFFER_SIZE)
	{
		m_aInputEvents[m_NumEvents].m_Key = Key;
		m_aInputEvents[m_NumEvents].m_Flags = Flags;
		if(pText == nullptr)
			m_aInputEvents[m_NumEvents].m_aText[0] = '\0';
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

	m_LastUpdate = 0;
	m_UpdateTime = 0.0f;

	m_InputCounter = 1;
	m_InputGrabbed = false;

	m_MouseDoubleClick = false;

	m_NumEvents = 0;
	m_MouseFocus = true;

	m_pClipboardText = nullptr;

	m_CompositionLength = COMP_LENGTH_INACTIVE;
	m_CompositionCursor = 0;
	m_CandidateSelectedIndex = -1;

	m_aDropFile[0] = '\0';
}

void CInput::Init()
{
	StopTextInput();

	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

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

	const int NumJoysticks = SDL_NumJoysticks();
	dbg_msg("joystick", "%d joystick(s) found", NumJoysticks);
	for(int i = 0; i < NumJoysticks; i++)
		OpenJoystick(i);
	UpdateActiveJoystick();

	Console()->Chain("inp_controller_guid", ConchainJoystickGuidChanged, this);
}

bool CInput::OpenJoystick(int JoystickIndex)
{
	SDL_Joystick *pJoystick = SDL_JoystickOpen(JoystickIndex);
	if(!pJoystick)
	{
		dbg_msg("joystick", "Could not open joystick %d: '%s'", JoystickIndex, SDL_GetError());
		return false;
	}
	if(std::find_if(m_vJoysticks.begin(), m_vJoysticks.end(), [pJoystick](const CJoystick &Joystick) -> bool { return Joystick.m_pDelegate == pJoystick; }) != m_vJoysticks.end())
	{
		// Joystick has already been added
		return false;
	}
	m_vJoysticks.emplace_back(this, m_vJoysticks.size(), pJoystick);
	const CJoystick &Joystick = m_vJoysticks[m_vJoysticks.size() - 1];
	dbg_msg("joystick", "Opened joystick %d '%s' (%d axes, %d buttons, %d balls, %d hats)", JoystickIndex, Joystick.GetName(),
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
		m_pActiveJoystick = &m_vJoysticks[0]; // NOLINT(readability-container-data-pointer)
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

void CInput::SetActiveJoystick(size_t Index)
{
	m_pActiveJoystick = &m_vJoysticks[Index];
	str_copy(g_Config.m_InpControllerGUID, m_pActiveJoystick->GetGUID());
}

float CInput::CJoystick::GetAxisValue(int Axis)
{
	return (SDL_JoystickGetAxis(m_pDelegate, Axis) - SDL_JOYSTICK_AXIS_MIN) / (float)(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN) * 2.0f - 1.0f;
}

void CInput::CJoystick::GetJoystickHatKeys(int Hat, int HatValue, int (&HatKeys)[2])
{
	if(HatValue & SDL_HAT_UP)
		HatKeys[0] = KEY_JOY_HAT0_UP + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else if(HatValue & SDL_HAT_DOWN)
		HatKeys[0] = KEY_JOY_HAT0_DOWN + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else
		HatKeys[0] = KEY_UNKNOWN;

	if(HatValue & SDL_HAT_LEFT)
		HatKeys[1] = KEY_JOY_HAT0_LEFT + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else if(HatValue & SDL_HAT_RIGHT)
		HatKeys[1] = KEY_JOY_HAT0_RIGHT + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT;
	else
		HatKeys[1] = KEY_UNKNOWN;
}

void CInput::CJoystick::GetHatValue(int Hat, int (&HatKeys)[2])
{
	return GetJoystickHatKeys(Hat, SDL_JoystickGetHat(m_pDelegate, Hat), HatKeys);
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
	m_InputGrabbed = false;
	SDL_SetRelativeMouseMode(SDL_FALSE);
	Graphics()->SetWindowGrab(false);
}

void CInput::MouseModeRelative()
{
	m_InputGrabbed = true;
#if !defined(CONF_PLATFORM_ANDROID) // No relative mouse on Android
	SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
	Graphics()->SetWindowGrab(true);
	// Clear pending relative mouse motion
	SDL_GetRelativeMouseState(nullptr, nullptr);
}

void CInput::NativeMousePos(int *pX, int *pY) const
{
	SDL_GetMouseState(pX, pY);
}

bool CInput::NativeMousePressed(int Index)
{
	int i = SDL_GetMouseState(nullptr, nullptr);
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

void CInput::StartTextInput()
{
	// enable system messages for IME
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	SDL_StartTextInput();
}

void CInput::StopTextInput()
{
	SDL_StopTextInput();
	// disable system messages for performance
	SDL_EventState(SDL_SYSWMEVENT, SDL_DISABLE);
	m_CompositionLength = COMP_LENGTH_INACTIVE;
	m_CompositionCursor = 0;
	m_aComposition[0] = '\0';
	m_vCandidates.clear();
}

void CInput::Clear()
{
	mem_zero(m_aInputState, sizeof(m_aInputState));
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	m_NumEvents = 0;
}

bool CInput::KeyState(int Key) const
{
	if(Key < KEY_FIRST || Key >= KEY_LAST)
		return false;
	return m_aInputState[Key];
}

void CInput::UpdateMouseState()
{
	const int MouseState = SDL_GetMouseState(nullptr, nullptr);
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
		int HatKeys[2];
		pJoystick->GetHatValue(Hat, HatKeys);
		for(int Key = KEY_JOY_HAT0_UP + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key <= KEY_JOY_HAT0_DOWN + Hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key++)
			m_aInputState[Key] = HatKeys[0] == Key || HatKeys[1] == Key;
	}
}

void CInput::HandleJoystickAxisMotionEvent(const SDL_JoyAxisEvent &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceID() != Event.which)
		return;
	if(Event.axis >= NUM_JOYSTICK_AXES)
		return;

	const int LeftKey = KEY_JOY_AXIS_0_LEFT + 2 * Event.axis;
	const int RightKey = LeftKey + 1;
	const float DeadZone = GetJoystickDeadzone();

	if(Event.value <= SDL_JOYSTICK_AXIS_MIN * DeadZone && !m_aInputState[LeftKey])
	{
		m_aInputState[LeftKey] = true;
		m_aInputCount[LeftKey] = m_InputCounter;
		AddEvent(nullptr, LeftKey, IInput::FLAG_PRESS);
	}
	else if(Event.value > SDL_JOYSTICK_AXIS_MIN * DeadZone && m_aInputState[LeftKey])
	{
		m_aInputState[LeftKey] = false;
		AddEvent(nullptr, LeftKey, IInput::FLAG_RELEASE);
	}

	if(Event.value >= SDL_JOYSTICK_AXIS_MAX * DeadZone && !m_aInputState[RightKey])
	{
		m_aInputState[RightKey] = true;
		m_aInputCount[RightKey] = m_InputCounter;
		AddEvent(nullptr, RightKey, IInput::FLAG_PRESS);
	}
	else if(Event.value < SDL_JOYSTICK_AXIS_MAX * DeadZone && m_aInputState[RightKey])
	{
		m_aInputState[RightKey] = false;
		AddEvent(nullptr, RightKey, IInput::FLAG_RELEASE);
	}
}

void CInput::HandleJoystickButtonEvent(const SDL_JoyButtonEvent &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceID() != Event.which)
		return;
	if(Event.button >= NUM_JOYSTICK_BUTTONS)
		return;

	const int Key = Event.button + KEY_JOYSTICK_BUTTON_0;

	if(Event.type == SDL_JOYBUTTONDOWN)
	{
		m_aInputState[Key] = true;
		m_aInputCount[Key] = m_InputCounter;
		AddEvent(nullptr, Key, IInput::FLAG_PRESS);
	}
	else if(Event.type == SDL_JOYBUTTONUP)
	{
		m_aInputState[Key] = false;
		AddEvent(nullptr, Key, IInput::FLAG_RELEASE);
	}
}

void CInput::HandleJoystickHatMotionEvent(const SDL_JoyHatEvent &Event)
{
	if(!g_Config.m_InpControllerEnable)
		return;
	CJoystick *pJoystick = GetActiveJoystick();
	if(!pJoystick || pJoystick->GetInstanceID() != Event.which)
		return;
	if(Event.hat >= NUM_JOYSTICK_HATS)
		return;

	int HatKeys[2];
	CJoystick::GetJoystickHatKeys(Event.hat, Event.value, HatKeys);

	for(int Key = KEY_JOY_HAT0_UP + Event.hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key <= KEY_JOY_HAT0_DOWN + Event.hat * NUM_JOYSTICK_BUTTONS_PER_HAT; Key++)
	{
		if(Key != HatKeys[0] && Key != HatKeys[1] && m_aInputState[Key])
		{
			m_aInputState[Key] = false;
			AddEvent(nullptr, Key, IInput::FLAG_RELEASE);
		}
	}

	for(int CurrentKey : HatKeys)
	{
		if(CurrentKey != KEY_UNKNOWN && !m_aInputState[CurrentKey])
		{
			m_aInputState[CurrentKey] = true;
			m_aInputCount[CurrentKey] = m_InputCounter;
			AddEvent(nullptr, CurrentKey, IInput::FLAG_PRESS);
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
	auto RemovedJoystick = std::find_if(m_vJoysticks.begin(), m_vJoysticks.end(), [Event](const CJoystick &Joystick) -> bool { return Joystick.GetInstanceID() == Event.which; });
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

void CInput::SetCompositionWindowPosition(float X, float Y, float H)
{
	SDL_Rect Rect;
	Rect.x = X / m_pGraphics->ScreenHiDPIScale();
	Rect.y = Y / m_pGraphics->ScreenHiDPIScale();
	Rect.h = H / m_pGraphics->ScreenHiDPIScale();
	Rect.w = 0;
	SDL_SetTextInputRect(&Rect);
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

	// keep the counter between 1..0xFFFF, 0 means not pressed
	m_InputCounter = (m_InputCounter % 0xFFFF) + 1;

	int NumKeyStates;
	const Uint8 *pState = SDL_GetKeyboardState(&NumKeyStates);
	if(NumKeyStates >= KEY_MOUSE_1)
		NumKeyStates = KEY_MOUSE_1;
	mem_copy(m_aInputState, pState, NumKeyStates);
	mem_zero(m_aInputState + NumKeyStates, KEY_LAST - NumKeyStates);

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
		case SDL_SYSWMEVENT:
			ProcessSystemMessage(Event.syswm.msg);
			break;

		case SDL_TEXTEDITING:
		{
			m_CompositionLength = str_length(Event.edit.text);
			if(m_CompositionLength)
			{
				str_copy(m_aComposition, Event.edit.text);
				m_CompositionCursor = 0;
				for(int i = 0; i < Event.edit.start; i++)
					m_CompositionCursor = str_utf8_forward(m_aComposition, m_CompositionCursor);
				// Event.edit.length is currently unused on Windows and will always be 0, so we don't support selecting composition text
				AddEvent(nullptr, KEY_UNKNOWN, IInput::FLAG_TEXT);
			}
			else
			{
				m_aComposition[0] = '\0';
				m_CompositionLength = 0;
				m_CompositionCursor = 0;
			}
			break;
		}

		case SDL_TEXTINPUT:
			m_aComposition[0] = '\0';
			m_CompositionLength = COMP_LENGTH_INACTIVE;
			m_CompositionCursor = 0;
			AddEvent(Event.text.text, KEY_UNKNOWN, IInput::FLAG_TEXT);
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
			HandleJoystickAxisMotionEvent(Event.jaxis);
			break;

		case SDL_JOYBUTTONUP:
		case SDL_JOYBUTTONDOWN:
			HandleJoystickButtonEvent(Event.jbutton);
			break;

		case SDL_JOYHATMOTION:
			HandleJoystickHatMotionEvent(Event.jhat);
			break;

		case SDL_JOYDEVICEADDED:
			HandleJoystickAddedEvent(Event.jdevice);
			break;

		case SDL_JOYDEVICEREMOVED:
			HandleJoystickRemovedEvent(Event.jdevice);
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
					MouseModeRelative();
					// Clear pending relative mouse motion
					SDL_GetRelativeMouseState(nullptr, nullptr);
				}
				m_MouseFocus = true;
				IgnoreKeys = true;
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				m_MouseFocus = false;
				IgnoreKeys = true;
				if(m_InputGrabbed)
				{
					MouseModeAbsolute();
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

		case SDL_DROPFILE:
			str_copy(m_aDropFile, Event.drop.file);
			SDL_free(Event.drop.file);
			break;
		}

		if(Scancode > KEY_FIRST && Scancode < g_MaxKeys && !IgnoreKeys && !HasComposition())
		{
			if(Action & IInput::FLAG_PRESS)
			{
				m_aInputState[Scancode] = 1;
				m_aInputCount[Scancode] = m_InputCounter;
			}
			AddEvent(nullptr, Scancode, Action);
		}
	}

	if(m_CompositionLength == 0)
		m_CompositionLength = COMP_LENGTH_INACTIVE;

	return 0;
}

void CInput::ProcessSystemMessage(SDL_SysWMmsg *pMsg)
{
#if defined(CONF_FAMILY_WINDOWS)
	// Todo SDL: remove this after SDL2 supports IME candidates
	if(pMsg->subsystem == SDL_SYSWM_WINDOWS && pMsg->msg.win.msg == WM_IME_NOTIFY)
	{
		switch(pMsg->msg.win.wParam)
		{
		case IMN_OPENCANDIDATE:
		case IMN_CHANGECANDIDATE:
		{
			HWND WindowHandle = pMsg->msg.win.hwnd;
			HIMC ImeContext = ImmGetContext(WindowHandle);
			DWORD Size = ImmGetCandidateListW(ImeContext, 0, nullptr, 0);
			LPCANDIDATELIST pCandidateList = nullptr;
			if(Size > 0)
			{
				pCandidateList = (LPCANDIDATELIST)malloc(Size);
				Size = ImmGetCandidateListW(ImeContext, 0, pCandidateList, Size);
			}
			m_vCandidates.clear();
			if(pCandidateList && Size > 0)
			{
				for(DWORD i = pCandidateList->dwPageStart; i < pCandidateList->dwCount && (int)m_vCandidates.size() < (int)pCandidateList->dwPageSize; i++)
				{
					LPCWSTR pCandidate = (LPCWSTR)((DWORD_PTR)pCandidateList + pCandidateList->dwOffset[i]);
					m_vCandidates.push_back(std::move(windows_wide_to_utf8(pCandidate)));
				}
				m_CandidateSelectedIndex = pCandidateList->dwSelection - pCandidateList->dwPageStart;
			}
			else
			{
				m_CandidateSelectedIndex = -1;
			}
			free(pCandidateList);
			ImmReleaseContext(WindowHandle, ImeContext);
			break;
		}
		case IMN_CLOSECANDIDATE:
			m_vCandidates.clear();
			m_CandidateSelectedIndex = -1;
			break;
		}
	}
#endif
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
