/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>

class CInput : public IEngineInput
{
	IEngineGraphics *m_pGraphics;

	int m_InputGrabbed;
	char *m_pClipboardText;

	bool m_MouseFocus;
	bool m_MouseDoubleClick;

	int m_VideoRestartNeeded;

	void AddEvent(char *pText, int Key, int Flags);
	void Clear() override;
	bool IsEventValid(CEvent *pEvent) const override { return pEvent->m_InputCount == m_InputCounter; }

	// quick access to input
	unsigned short m_aInputCount[g_MaxKeys]; // tw-KEY
	unsigned char m_aInputState[g_MaxKeys]; // SDL_SCANCODE
	int m_InputCounter;

	void UpdateMouseState();

	// IME support
	int m_NumTextInputInstances;
	char m_aEditingText[INPUT_TEXT_SIZE];
	int m_EditingTextLen;
	int m_EditingCursor;

	bool KeyState(int Key) const;

	IEngineGraphics *Graphics() { return m_pGraphics; }

public:
	CInput();

	void Init() override;

	bool ModifierIsPressed() const override { return KeyState(KEY_LCTRL) || KeyState(KEY_RCTRL) || KeyState(KEY_LGUI) || KeyState(KEY_RGUI); }
	bool KeyIsPressed(int Key) const override { return KeyState(Key); }
	bool KeyPress(int Key, bool CheckCounter) const override { return CheckCounter ? (m_aInputCount[Key] == m_InputCounter) : m_aInputCount[Key]; }

	bool MouseRelative(float *pX, float *pY) override;
	void MouseModeAbsolute() override;
	void MouseModeRelative() override;
	void NativeMousePos(int *pX, int *pY) const override;
	bool NativeMousePressed(int Index) override;
	bool MouseDoubleClick() override;

	const char *GetClipboardText() override;
	void SetClipboardText(const char *pText) override;

	int Update() override;

	int VideoRestartNeeded() override;

	bool GetIMEState() override;
	void SetIMEState(bool Activate) override;
	int GetIMEEditingTextLength() const override { return m_EditingTextLen; }
	const char *GetIMEEditingText() override;
	int GetEditingCursor() override;
	void SetEditingPosition(float X, float Y) override;
};

#endif
