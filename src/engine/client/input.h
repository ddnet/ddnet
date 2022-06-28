/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

#include <engine/input.h>
#include <engine/keys.h>

class IEngineGraphics;

class CInput : public IEngineInput
{
public:
	class CJoystick : public IJoystick
	{
		CInput *m_pInput;
		int m_Index;
		char m_aName[64];
		char m_aGUID[34];
		SDL_JoystickID m_InstanceID;
		int m_NumAxes;
		int m_NumButtons;
		int m_NumBalls;
		int m_NumHats;
		SDL_Joystick *m_pDelegate;

		CInput *Input() { return m_pInput; }

	public:
		CJoystick() {}
		CJoystick(CInput *pInput, int Index, SDL_Joystick *pDelegate);
		virtual ~CJoystick() = default;

		int GetIndex() const override { return m_Index; }
		const char *GetName() const override { return m_aName; }
		const char *GetGUID() const { return m_aGUID; }
		SDL_JoystickID GetInstanceID() const { return m_InstanceID; }
		int GetNumAxes() const override { return m_NumAxes; }
		int GetNumButtons() const override { return m_NumButtons; }
		int GetNumBalls() const override { return m_NumBalls; }
		int GetNumHats() const override { return m_NumHats; }
		float GetAxisValue(int Axis) override;
		int GetHatValue(int Hat) override;
		bool Relative(float *pX, float *pY) override;
		bool Absolute(float *pX, float *pY) override;

		static int GetJoystickHatKey(int Hat, int HatValue);
	};

private:
	IEngineGraphics *m_pGraphics;
	IConsole *m_pConsole;

	IEngineGraphics *Graphics() { return m_pGraphics; }
	IConsole *Console() { return m_pConsole; }

	// joystick
	std::vector<CJoystick> m_vJoysticks;
	CJoystick *m_pActiveJoystick = nullptr;
	void InitJoysticks();
	void CloseJoysticks();
	void UpdateActiveJoystick();
	static void ConchainJoystickGuidChanged(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	float GetJoystickDeadzone();

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
	void UpdateJoystickState();
	void HandleJoystickAxisMotionEvent(const SDL_Event &Event);
	void HandleJoystickButtonEvent(const SDL_Event &Event);
	void HandleJoystickHatMotionEvent(const SDL_Event &Event);

	// IME support
	int m_NumTextInputInstances;
	char m_aEditingText[INPUT_TEXT_SIZE];
	int m_EditingTextLen;
	int m_EditingCursor;

	bool KeyState(int Key) const;

public:
	CInput();

	void Init() override;
	void Shutdown() override;

	bool ModifierIsPressed() const override { return KeyState(KEY_LCTRL) || KeyState(KEY_RCTRL) || KeyState(KEY_LGUI) || KeyState(KEY_RGUI); }
	bool KeyIsPressed(int Key) const override { return KeyState(Key); }
	bool KeyPress(int Key, bool CheckCounter) const override { return CheckCounter ? (m_aInputCount[Key] == m_InputCounter) : m_aInputCount[Key]; }

	size_t NumJoysticks() const override { return m_vJoysticks.size(); }
	CJoystick *GetActiveJoystick() override { return m_pActiveJoystick; }
	void SelectNextJoystick() override;

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
