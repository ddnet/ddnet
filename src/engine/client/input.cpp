/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SDL.h"

#include <base/system.h>
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

void CInput::AddEvent(char *pText, int Key, int Flags)
{
	if(m_NumEvents != INPUT_BUFFER_SIZE)
	{
		m_aInputEvents[m_NumEvents].m_Key = Key;
		m_aInputEvents[m_NumEvents].m_Flags = Flags;
		if(!pText)
			m_aInputEvents[m_NumEvents].m_aText[0] = 0;
		else
			str_copy(m_aInputEvents[m_NumEvents].m_aText, pText, sizeof(m_aInputEvents[m_NumEvents].m_aText));
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

	m_LastX = 0;
	m_LastY = 0;
}

void CInput::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();

	// increase ime instance counter for menu
	SetIMEState(true);
	MouseModeRelative();
}

void CInput::MouseRelative(float *x, float *y)
{
	if(!m_MouseFocus || !m_InputGrabbed)
		return;

	int nx = 0, ny = 0;
	float Sens = g_Config.m_InpMousesens / 100.0f;

#if defined(CONF_PLATFORM_ANDROID) // No relative mouse on Android
	SDL_GetMouseState(&nx, &ny);
	int XTmp = nx - m_LastX;
	int YTmp = ny - m_LastY;
	m_LastX = nx;
	m_LastY = ny;
	nx = XTmp;
	ny = YTmp;
	Sens = 1;
#else
	SDL_GetRelativeMouseState(&nx, &ny);
#endif

	*x = nx * Sens;
	*y = ny * Sens;
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

void CInput::NativeMousePos(int *x, int *y) const
{
	int nx = 0, ny = 0;
	SDL_GetMouseState(&nx, &ny);

	*x = nx;
	*y = ny;
}

bool CInput::NativeMousePressed(int index)
{
	int i = SDL_GetMouseState(NULL, NULL);
	return (i & SDL_BUTTON(index)) != 0;
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

void CInput::SetClipboardText(const char *Text)
{
	SDL_SetClipboardText(Text);
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

	// these states must always be updated manually because they are not in the GetKeyState from SDL
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
				str_copy(m_aEditingText, Event.edit.text, sizeof(m_aEditingText));
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

		// handle mouse buttons
		case SDL_MOUSEBUTTONUP:
			Action = IInput::FLAG_RELEASE;

			// fall through
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
				// fallthrough
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
