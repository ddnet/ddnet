/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

class CInput : public IEngineInput
{
	IEngineGraphics *m_pGraphics;

	int m_InputGrabbed;
	char *m_pClipboardText;

	int64 m_LastRelease;
	int64 m_ReleaseDelta;

	int m_VideoRestartNeeded;

	void AddEvent(int Unicode, int Key, int Flags);

	IEngineGraphics *Graphics() { return m_pGraphics; }

public:
	CInput();

	virtual void Init();

	int KeyState(int Key) const;
	int KeyStateOld(int Key) const;
	int KeyWasPressed(int Key) const { return KeyStateOld(Key); }

	int KeyPressed(int Key) const { return KeyState(Key); }
	int KeyReleases(int Key) const { return m_aInputCount[m_InputCurrent][Key].m_Releases; }
	int KeyPresses(int Key) const { return m_aInputCount[m_InputCurrent][Key].m_Presses; }
	int KeyDown(int Key) const { return KeyPressed(Key)&&!KeyWasPressed(Key); }

	virtual void MouseRelative(float *x, float *y);
	virtual void MouseModeAbsolute();
	virtual void MouseModeRelative();
	virtual int MouseDoubleClick();
	virtual const char* GetClipboardText();
	virtual void SetClipboardText(const char *Text);

	void ClearKeyStates();

	int ButtonPressed(int Button) { return m_aInputState[m_InputCurrent][Button]; }

	virtual int Update();

	virtual int VideoRestartNeeded();
};

#endif
