/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>

#include <stddef.h>

class CInput : public IEngineInput
{
	IEngineGraphics *m_pGraphics;

	int m_LastX;
	int m_LastY;

	int m_InputGrabbed;
	char *m_pClipboardText;

	bool m_MouseFocus;
	bool m_MouseDoubleClick;

	int m_VideoRestartNeeded;

	void AddEvent(char *pText, int Key, int Flags);
	void Clear();
	bool IsEventValid(CEvent *pEvent) const { return pEvent->m_InputCount == m_InputCounter; }

	// quick access to input
	unsigned short m_aInputCount[g_MaxKeys]; // tw-KEY
	unsigned char m_aInputState[g_MaxKeys]; // SDL_SCANCODE
	int m_InputCounter;

	// IME support
	int m_NumTextInputInstances;
	char m_aEditingText[INPUT_TEXT_SIZE];
	int m_EditingTextLen;
	int m_EditingCursor;

	bool KeyState(int Key) const;

	IEngineGraphics *Graphics() { return m_pGraphics; }

public:
	CInput();

	virtual void Init();

	bool ModifierIsPressed() const { return KeyState(KEY_LCTRL) || KeyState(KEY_RCTRL) || KeyState(KEY_LGUI) || KeyState(KEY_RGUI); }
	bool KeyIsPressed(int Key) const { return KeyState(Key); }
	bool KeyPress(int Key, bool CheckCounter) const { return CheckCounter ? (m_aInputCount[Key] == m_InputCounter) : m_aInputCount[Key]; }

	virtual void MouseRelative(float *x, float *y);
	virtual void MouseModeAbsolute();
	virtual void MouseModeRelative();
	virtual void NativeMousePos(int *x, int *y) const;
	virtual bool NativeMousePressed(int index);
	virtual bool MouseDoubleClick();
	virtual const char *GetClipboardText();
	virtual void SetClipboardText(const char *Text);

	virtual int Update();

	virtual int VideoRestartNeeded();

	virtual bool GetIMEState();
	virtual void SetIMEState(bool Activate);
	int GetIMEEditingTextLength() const { return m_EditingTextLen; }
	virtual const char *GetIMEEditingText();
	virtual int GetEditingCursor();
	virtual void SetEditingPosition(float X, float Y);
};

#endif
