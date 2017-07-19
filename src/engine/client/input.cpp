/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SDL.h"

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>

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

	m_LastRelease = 0;
	m_ReleaseDelta = -1;

	m_NumEvents = 0;
	m_MouseFocus = true;

	m_VideoRestartNeeded = 0;
	m_pClipboardText = NULL;

	m_CountEditingText = 0;
}

void CInput::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();

	MouseModeRelative();
}

void CInput::MouseRelative(float *x, float *y)
{
	if(!m_MouseFocus || !m_InputGrabbed)
		return;

#if defined(__ANDROID__) // No relative mouse on Android
	int nx = 0, ny = 0;
	SDL_GetMouseState(&nx, &ny);
	*x = nx;
	*y = ny;
#else
	int nx = 0, ny = 0;
	float Sens = ((g_Config.m_ClDyncam && g_Config.m_ClDyncamMousesens) ? g_Config.m_ClDyncamMousesens : g_Config.m_InpMousesens) / 100.0f;

	SDL_GetRelativeMouseState(&nx,&ny);

	*x = nx*Sens;
	*y = ny*Sens;
#endif
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
	SDL_SetRelativeMouseMode(SDL_TRUE);
	Graphics()->SetWindowGrab(true);
}

int CInput::MouseDoubleClick()
{
	if(m_ReleaseDelta >= 0 && m_ReleaseDelta < (time_freq() / 3))
	{
		m_LastRelease = 0;
		m_ReleaseDelta = -1;
		return 1;
	}
	return 0;
}

const char* CInput::GetClipboardText()
{
	if(m_pClipboardText)
	{
		SDL_free(m_pClipboardText);
	}
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
	return m_aInputState[Key>=KEY_MOUSE_1 ? Key : SDL_GetScancodeFromKey(KeyToKeycode(Key))];
}

void CInput::NextFrame()
{
	int i;
	const Uint8 *pState = SDL_GetKeyboardState(&i);
	if(i >= KEY_LAST)
		i = KEY_LAST-1;
	mem_copy(m_aInputState, pState, i);
}

bool CInput::GetIMEState()
{
	return m_CountEditingText > 0;
}

void CInput::SetIMEState(bool Activate)
{
	if(Activate)
	{
		if(m_CountEditingText == 0)
			SDL_StartTextInput();
		else
			m_CountEditingText++;
	}
	else
	{
		if(m_CountEditingText == 0)
			return;
		m_CountEditingText--;
		if(m_CountEditingText == 0)
			SDL_StopTextInput();
	}
}

const char* CInput::GetIMECandidate()
{
	if (str_length(m_pEditingText))
		return m_pEditingText;
	else
		return "";
}

int CInput::GetEditingCursor()
{
	return m_EditingCursor;
}

int CInput::Update()
{
	// keep the counter between 1..0xFFFF, 0 means not pressed
	m_InputCounter = (m_InputCounter%0xFFFF)+1;

	// these states must always be updated manually because they are not in the GetKeyState from SDL
	int i = SDL_GetMouseState(NULL, NULL);
	if(i&SDL_BUTTON(1)) m_aInputState[KEY_MOUSE_1] = 1; // 1 is left
	if(i&SDL_BUTTON(3)) m_aInputState[KEY_MOUSE_2] = 1; // 3 is right
	if(i&SDL_BUTTON(2)) m_aInputState[KEY_MOUSE_3] = 1; // 2 is middle
	if(i&SDL_BUTTON(4)) m_aInputState[KEY_MOUSE_4] = 1;
	if(i&SDL_BUTTON(5)) m_aInputState[KEY_MOUSE_5] = 1;
	if(i&SDL_BUTTON(6)) m_aInputState[KEY_MOUSE_6] = 1;
	if(i&SDL_BUTTON(7)) m_aInputState[KEY_MOUSE_7] = 1;
	if(i&SDL_BUTTON(8)) m_aInputState[KEY_MOUSE_8] = 1;
	if(i&SDL_BUTTON(9)) m_aInputState[KEY_MOUSE_9] = 1;

	{
		SDL_Event Event;
		bool IgnoreKeys = false;
		while(SDL_PollEvent(&Event))
		{
			int Key = -1;
			int Scancode = 0;
			int Action = IInput::FLAG_PRESS;
			switch(Event.type)
			{
				case SDL_TEXTEDITING:
				{
					if(str_length(Event.edit.text))
					{
						str_format(m_pEditingText, sizeof(m_pEditingText), Event.edit.text);
						m_EditingCursor = 0;
						for (int i = 0; i < Event.edit.start; i++)
							m_EditingCursor = str_utf8_forward(m_pEditingText, m_EditingCursor);
					}
					break;
				}
				case SDL_TEXTINPUT:
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
						Key = KeycodeToKey(Event.key.keysym.sym);
						Scancode = Event.key.keysym.scancode;
					}
					break;
				case SDL_KEYUP:
					Action = IInput::FLAG_RELEASE;
					Key = KeycodeToKey(Event.key.keysym.sym);
					Scancode = Event.key.keysym.scancode;
					break;

				// handle mouse buttons
				case SDL_MOUSEBUTTONUP:
					Action = IInput::FLAG_RELEASE;

					if(Event.button.button == 1) // ignore_convention
					{
						m_ReleaseDelta = time_get() - m_LastRelease;
						m_LastRelease = time_get();
					}

					// fall through
				case SDL_MOUSEBUTTONDOWN:
					if(Event.button.button == SDL_BUTTON_LEFT) Key = KEY_MOUSE_1; // ignore_convention
					if(Event.button.button == SDL_BUTTON_RIGHT) Key = KEY_MOUSE_2; // ignore_convention
					if(Event.button.button == SDL_BUTTON_MIDDLE) Key = KEY_MOUSE_3; // ignore_convention
					if(Event.button.button == SDL_BUTTON_X1) Key = KEY_MOUSE_4; // ignore_convention
					if(Event.button.button == SDL_BUTTON_X2) Key = KEY_MOUSE_5; // ignore_convention
					if(Event.button.button == 6) Key = KEY_MOUSE_6; // ignore_convention
					if(Event.button.button == 7) Key = KEY_MOUSE_7; // ignore_convention
					if(Event.button.button == 8) Key = KEY_MOUSE_8; // ignore_convention
					if(Event.button.button == 9) Key = KEY_MOUSE_9; // ignore_convention
					Scancode = Key;
					break;

				case SDL_MOUSEWHEEL:
					if(Event.wheel.y > 0) Key = KEY_MOUSE_WHEEL_UP; // ignore_convention
					if(Event.wheel.y < 0) Key = KEY_MOUSE_WHEEL_DOWN; // ignore_convention
					if(Event.wheel.x > 0) Key = KEY_MOUSE_WHEEL_LEFT; // ignore_convention
					if(Event.wheel.x < 0) Key = KEY_MOUSE_WHEEL_RIGHT; // ignore_convention
					Action |= IInput::FLAG_RELEASE;
					Scancode = Key;
					break;

				case SDL_WINDOWEVENT:
					// Ignore keys following a focus gain as they may be part of global
					// shortcuts
					switch (Event.window.event)
					{
						case SDL_WINDOWEVENT_RESIZED:
#if defined(SDL_VIDEO_DRIVER_X11)
							Graphics()->Resize(Event.window.data1, Event.window.data2);
#elif defined(__ANDROID__)
							m_VideoRestartNeeded = 1;
#endif
							break;
						case SDL_WINDOWEVENT_FOCUS_GAINED:
							if(m_InputGrabbed)
								MouseModeRelative();
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
#if defined(CONF_PLATFORM_MACOSX)	// Todo: remove this when fixed in SDL
						case SDL_WINDOWEVENT_MAXIMIZED:
							MouseModeAbsolute();
							MouseModeRelative();
							break;
#endif
					}
					break;

				// other messages
				case SDL_QUIT:
					return 1;
			}

			if(Key >= 0 && Key < g_MaxKeys && !IgnoreKeys && m_CountEditingText == 0)
			{
				if(Action&IInput::FLAG_PRESS)
				{
					m_aInputState[Scancode] = 1;
					m_aInputCount[Key] = m_InputCounter;
				}
				AddEvent(0, Key, Action);
			}

		}
	}

	return 0;
}

int CInput::VideoRestartNeeded()
{
	if( m_VideoRestartNeeded )
	{
		m_VideoRestartNeeded = 0;
		return 1;
	}
	return 0;
}

IEngineInput *CreateEngineInput() { return new CInput; }
