/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

#include <engine/graphics.h>
#include <engine/input.h>

#include <stddef.h>

class CInput : public IEngineInput
{
	IEngineGraphics *m_pGraphics;

	char *m_pClipboardText;

	int64_t m_LastRelease;
	int64_t m_ReleaseDelta;

	bool m_MouseFocus;
	int m_VideoRestartNeeded;

	void AddEvent(char *pText, int Key, int Flags);
	void Clear();
	bool IsEventValid(CEvent *pEvent) const { return pEvent->m_InputCount == m_InputCounter; };

	// quick access to input
	unsigned short m_aInputCount[g_MaxKeys]; // tw-KEY
	unsigned char m_aInputState[g_MaxKeys]; // SDL_SCANCODE
	int m_InputCounter;

	// IME support
	int m_NumTextInputInstances;
	char m_aEditingText[INPUT_TEXT_SIZE];
	int m_EditingTextLen;
	int m_EditingCursor;

	int m_DesktopX = 0;
	int m_DesktopY = 0;

	bool KeyState(int Key) const;

	IEngineGraphics *Graphics() { return m_pGraphics; }

	void MouseModeInGameRelativeImpl();

public:
	CInput();

	virtual void Init();

	bool KeyIsPressed(int Key) const { return KeyState(Key); }
	bool KeyPress(int Key, bool CheckCounter) const { return CheckCounter ? (m_aInputCount[Key] == m_InputCounter) : m_aInputCount[Key]; }

	virtual void NativeMousePos(int *x, int *y) const;
	virtual bool NativeMousePressed(int index);
	virtual bool MouseRelative(float *x, float *y);
	virtual bool MouseDesktopRelative(int *x, int *y);
	virtual bool MouseAbsolute(int *x, int *y);

	// return true if the mode was changed
	virtual void MouseModeChange(EInputMouseMode OldState, EInputMouseMode NewState);
	virtual bool MouseModeAbsolute();
	virtual bool MouseModeRelative();
	virtual bool MouseModeInGame(int *pDesiredX = NULL, int *pDesiredY = NULL);
	virtual bool MouseModeInGameRelative();

	virtual int MouseDoubleClick();
	virtual const char *GetClipboardText();
	virtual void SetClipboardText(const char *Text);

	virtual int Update();
	virtual void NextFrame();

	virtual int VideoRestartNeeded();

	virtual bool GetIMEState();
	virtual void SetIMEState(bool Activate);
	int GetIMEEditingTextLength() const { return m_EditingTextLen; }
	virtual const char *GetIMEEditingText();
	virtual int GetEditingCursor();
	virtual void SetEditingPosition(float X, float Y);
};

#endif
