/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include "kernel.h"

const int g_MaxKeys = 512;
extern const char g_aaKeyStrings[g_MaxKeys][20];

enum EInputMouseMode
{
	// Use absolute mouse mode, doesn't grab the mouse inside the window and uses desktop cursor coordinates
	INPUT_MOUSE_MODE_ABSOLUTE = 0,
	// Use relative mouse mode, does grab the mouse inside the window and uses mouse driver coordinates(except if mouse old)
	INPUT_MOUSE_MODE_RELATIVE,
	// Use ingame mouse mode, does grab the mouse inside the window, but uses uses desktop cursor coordinates
	INPUT_MOUSE_MODE_INGAME,
	// Use ingame mouse mode, does grab the mouse inside the window, but uses uses desktop cursor coordinates, but is relative
	INPUT_MOUSE_MODE_INGAME_RELATIVE,
};

class IInput : public IInterface
{
	MACRO_INTERFACE("input", 0)
public:
	enum
	{
		INPUT_TEXT_SIZE = 128
	};

	class CEvent
	{
	public:
		int m_Flags;
		int m_Key;
		char m_aText[INPUT_TEXT_SIZE];
		int m_InputCount;
	};

protected:
	enum
	{
		INPUT_BUFFER_SIZE = 32
	};

	// quick access to events
	int m_NumEvents;
	IInput::CEvent m_aInputEvents[INPUT_BUFFER_SIZE];

	EInputMouseMode m_MouseMode = INPUT_MOUSE_MODE_ABSOLUTE;

public:
	enum
	{
		FLAG_PRESS = 1,
		FLAG_RELEASE = 2,
		FLAG_REPEAT = 4,
		FLAG_TEXT = 8,
	};

	EInputMouseMode GetMouseMode() { return m_MouseMode; };
	void SetMouseMode(EInputMouseMode NewMode) { m_MouseMode = NewMode; };

	// events
	int NumEvents() const { return m_NumEvents; }
	virtual bool IsEventValid(CEvent *pEvent) const = 0;
	CEvent GetEvent(int Index) const
	{
		if(Index < 0 || Index >= m_NumEvents)
		{
			IInput::CEvent e = {0, 0};
			return e;
		}
		return m_aInputEvents[Index];
	}

	CEvent *GetEventsRaw() { return m_aInputEvents; }
	int *GetEventCountRaw() { return &m_NumEvents; }

	// keys
	virtual bool KeyIsPressed(int Key) const = 0;
	virtual bool KeyPress(int Key, bool CheckCounter = false) const = 0;
	const char *KeyName(int Key) const { return (Key >= 0 && Key < g_MaxKeys) ? g_aaKeyStrings[Key] : g_aaKeyStrings[0]; }
	virtual void Clear() = 0;

	//
	virtual void NativeMousePos(int *x, int *y) const = 0;
	virtual bool NativeMousePressed(int index) = 0;
	// return true if the mode was changed
	virtual bool MouseModeRelative() = 0;
	virtual bool MouseModeAbsolute() = 0;
	virtual bool MouseModeInGame(int *pDesiredX = NULL, int *pDesiredY = NULL) = 0;
	virtual bool MouseModeInGameRelative() = 0;

	virtual int MouseDoubleClick() = 0;
	virtual const char *GetClipboardText() = 0;
	virtual void SetClipboardText(const char *Text) = 0;

	// return true if there was a mouse input
	virtual bool MouseAbsolute(int *x, int *y) = 0;
	virtual bool MouseDesktopRelative(int *x, int *y) = 0;
	virtual bool MouseRelative(float *x, float *y) = 0;

	virtual bool GetIMEState() = 0;
	virtual void SetIMEState(bool Activate) = 0;
	virtual int GetIMEEditingTextLength() const = 0;
	virtual const char *GetIMEEditingText() = 0;
	virtual int GetEditingCursor() = 0;
	virtual void SetEditingPosition(float X, float Y) = 0;
};

class IEngineInput : public IInput
{
	MACRO_INTERFACE("engineinput", 0)
public:
	virtual void Init() = 0;
	virtual int Update() = 0;
	virtual void NextFrame() = 0;
	virtual int VideoRestartNeeded() = 0;
};

extern IEngineInput *CreateEngineInput();

#endif
