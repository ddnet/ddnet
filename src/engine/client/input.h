/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

#include <SDL_events.h>
#include <SDL_joystick.h>

#include <engine/input.h>
#include <engine/keys.h>

#include <string>
#include <vector>

class IEngineGraphics;

class CInput : public IEngineInput
{
public:
	class CJoystick : public IJoystick
	{
		friend class CInput;

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
		void GetHatValue(int Hat, int (&HatKeys)[2]) override;
		bool Relative(float *pX, float *pY) override;
		bool Absolute(float *pX, float *pY) override;

		static void GetJoystickHatKeys(int Hat, int HatValue, int (&HatKeys)[2]);
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
	bool OpenJoystick(int JoystickIndex);
	void CloseJoysticks();
	void UpdateActiveJoystick();
	static void ConchainJoystickGuidChanged(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	float GetJoystickDeadzone();

	bool m_InputGrabbed;
	char *m_pClipboardText;

	bool m_MouseFocus;
	bool m_MouseDoubleClick;

	// IME support
	char m_aComposition[MAX_COMPOSITION_ARRAY_SIZE];
	int m_CompositionCursor;
	int m_CompositionLength;
	std::vector<std::string> m_vCandidates;
	int m_CandidateSelectedIndex;

	void AddEvent(char *pText, int Key, int Flags);
	void Clear() override;
	bool IsEventValid(const CEvent &Event) const override { return Event.m_InputCount == m_InputCounter; }

	// quick access to input
	unsigned short m_aInputCount[g_MaxKeys]; // tw-KEY
	unsigned char m_aInputState[g_MaxKeys]; // SDL_SCANCODE
	int m_InputCounter;

	void UpdateMouseState();
	void UpdateJoystickState();
	void HandleJoystickAxisMotionEvent(const SDL_JoyAxisEvent &Event);
	void HandleJoystickButtonEvent(const SDL_JoyButtonEvent &Event);
	void HandleJoystickHatMotionEvent(const SDL_JoyHatEvent &Event);
	void HandleJoystickAddedEvent(const SDL_JoyDeviceEvent &Event);
	void HandleJoystickRemovedEvent(const SDL_JoyDeviceEvent &Event);

	char m_aDropFile[IO_MAX_PATH_LENGTH];

	bool KeyState(int Key) const;

	void ProcessSystemMessage(SDL_SysWMmsg *pMsg);

public:
	CInput();

	void Init() override;
	int Update() override;
	void Shutdown() override;

	bool ModifierIsPressed() const override { return KeyState(KEY_LCTRL) || KeyState(KEY_RCTRL) || KeyState(KEY_LGUI) || KeyState(KEY_RGUI); }
	bool ShiftIsPressed() const override { return KeyState(KEY_LSHIFT) || KeyState(KEY_RSHIFT); }
	bool AltIsPressed() const override { return KeyState(KEY_LALT) || KeyState(KEY_RALT); }
	bool KeyIsPressed(int Key) const override { return KeyState(Key); }
	bool KeyPress(int Key, bool CheckCounter) const override { return CheckCounter ? (m_aInputCount[Key] == m_InputCounter) : m_aInputCount[Key]; }

	size_t NumJoysticks() const override { return m_vJoysticks.size(); }
	CJoystick *GetJoystick(size_t Index) override { return &m_vJoysticks[Index]; }
	CJoystick *GetActiveJoystick() override { return m_pActiveJoystick; }
	void SetActiveJoystick(size_t Index) override;

	bool MouseRelative(float *pX, float *pY) override;
	void MouseModeAbsolute() override;
	void MouseModeRelative() override;
	void NativeMousePos(int *pX, int *pY) const override;
	bool NativeMousePressed(int Index) override;
	bool MouseDoubleClick() override;

	const char *GetClipboardText() override;
	void SetClipboardText(const char *pText) override;

	void StartTextInput() override;
	void StopTextInput() override;
	const char *GetComposition() const override { return m_aComposition; }
	bool HasComposition() const override { return m_CompositionLength != COMP_LENGTH_INACTIVE; }
	int GetCompositionCursor() const override { return m_CompositionCursor; }
	int GetCompositionLength() const override { return m_CompositionLength; }
	const char *GetCandidate(int Index) const override { return m_vCandidates[Index].c_str(); }
	int GetCandidateCount() const override { return m_vCandidates.size(); }
	int GetCandidateSelectedIndex() const override { return m_CandidateSelectedIndex; }
	void SetCompositionWindowPosition(float X, float Y, float H) override;

	bool GetDropFile(char *aBuf, int Len) override;
};

#endif
