/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

#include <engine/console.h>
#include <engine/input.h>
#include <engine/keys.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_joystick.h>

#include <string>
#include <vector>

class IEngineGraphics;
class IConfigManager;

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
		SDL_JoystickID m_InstanceId;
		int m_NumAxes;
		int m_NumButtons;
		int m_NumBalls;
		int m_NumHats;
		SDL_Joystick *m_pDelegate;

		CInput *Input() { return m_pInput; }

	public:
		CJoystick(CInput *pInput, int Index, SDL_Joystick *pDelegate);
		~CJoystick() override = default;

		int GetIndex() const override { return m_Index; }
		const char *GetName() const override { return m_aName; }
		const char *GetGUID() const { return m_aGUID; }
		SDL_JoystickID GetInstanceId() const { return m_InstanceId; }
		int GetNumAxes() const override { return m_NumAxes; }
		int GetNumButtons() const override { return m_NumButtons; }
		int GetNumBalls() const override { return m_NumBalls; }
		int GetNumHats() const override { return m_NumHats; }
		float GetAxisValue(int Axis) override;
		void GetHatValue(int Hat, int (&aHatKeys)[2]) override;
		bool Relative(float *pX, float *pY) override;
		bool Absolute(float *pX, float *pY) override;

		static void GetJoystickHatKeys(int Hat, int HatValue, int (&aHatKeys)[2]);
	};

private:
	IEngineGraphics *m_pGraphics;
	IConsole *m_pConsole;
	IConfigManager *m_pConfigManager;

	IEngineGraphics *Graphics() const { return m_pGraphics; }
	IConsole *Console() const { return m_pConsole; }

	// Resolved on demand from the graphics backend, which owns the window: it destroys
	// and recreates it, so a cached pointer goes stale and every SDL call on it fails.
	SDL_Window *Window() const;

	// joystick
	std::vector<CJoystick> m_vJoysticks;
	CJoystick *m_pActiveJoystick = nullptr;
	void InitJoysticks();
	bool OpenJoystick(int JoystickId);
	void CloseJoysticks();
	void UpdateActiveJoystick();
	static void ConchainJoystickGuidChanged(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	float GetJoystickDeadzone();

	bool m_InputGrabbed;

	bool m_MouseFocus;
	vec2 m_MouseMotion = vec2(0.0f, 0.0f);
	vec2 m_SecondaryMouseMotion = vec2(0.0f, 0.0f);
	SDL_MouseID m_SecondaryMouseId = 0;
	SDL_KeyboardID m_SecondaryKeyboardId = 0;
	bool m_aSecondaryKeys[KEY_LAST] = {};
	std::vector<std::string> m_vMouseNames;
	std::vector<std::string> m_vKeyboardNames;
	// the config values the secondary devices were resolved from, the settings menu changes them without the console
	int m_SecondaryMouseConfig = 0;
	int m_SecondaryKeyboardConfig = 0;
	std::string m_SecondaryKeysConfig;
	bool IsSecondaryMouse(SDL_MouseID Id) const { return m_SecondaryMouseId != 0 && Id == m_SecondaryMouseId; }
	bool IsSecondaryKeyboard(SDL_KeyboardID Id) const { return m_SecondaryKeyboardId != 0 && Id == m_SecondaryKeyboardId; }
	bool TakeMouseMotion(vec2 &Motion, float *pX, float *pY);
	void UpdateSecondaryDevices();

	// IME support
	std::string m_CompositionString;
	int m_CompositionCursor;
	std::vector<std::string> m_vCandidates;
	int m_CandidateSelectedIndex;

	// events
	std::vector<CEvent> m_vInputEvents;
	int64_t m_LastUpdate;
	float m_UpdateTime;
	void AddKeyEvent(int Key, int Flags, bool Secondary);
	void AddTextEvent(const char *pText);

	// quick access to input
	bool m_aCurrentKeyStates[KEY_LAST];
	bool m_aFrameKeyStates[KEY_LAST];
	uint32_t m_InputCounter;
	std::vector<CTouchFingerState> m_vTouchFingerStates;

	void HandleJoystickAxisMotionEvent(const SDL_JoyAxisEvent &Event);
	void HandleJoystickButtonEvent(const SDL_JoyButtonEvent &Event);
	void HandleJoystickHatMotionEvent(const SDL_JoyHatEvent &Event);
	void HandleJoystickAddedEvent(const SDL_JoyDeviceEvent &Event);
	void HandleJoystickRemovedEvent(const SDL_JoyDeviceEvent &Event);
	vec2 TouchPositionToViewport(vec2 Position) const;
	vec2 TouchDeltaToViewport(vec2 Delta) const;
	void HandleTouchDownEvent(const SDL_TouchFingerEvent &Event);
	void HandleTouchUpEvent(const SDL_TouchFingerEvent &Event);
	void HandleTouchMotionEvent(const SDL_TouchFingerEvent &Event);
	void HandleTextEditingEvent(const char *pText, int Start, int Length);
	int TranslateMouseWheelEventKey(const SDL_MouseWheelEvent &Event);

	// remainder of the scroll deltas that did not add up to a whole notch yet
	float m_ResidualScrollX = 0.0f;
	float m_ResidualScrollY = 0.0f;

	char m_aDropFile[IO_MAX_PATH_LENGTH];

public:
	CInput();

	void Init() override;
	int Update() override;
	void Shutdown() override;

	void ConsumeEvents(std::function<void(const CEvent &Event)> Consumer) const override;
	void Clear() override;
	float GetUpdateTime() const override;

	bool ModifierIsPressed() const override { return KeyIsPressed(KEY_LCTRL) || KeyIsPressed(KEY_RCTRL) || KeyIsPressed(KEY_LGUI) || KeyIsPressed(KEY_RGUI); }
	bool ShiftIsPressed() const override { return KeyIsPressed(KEY_LSHIFT) || KeyIsPressed(KEY_RSHIFT); }
	bool AltIsPressed() const override { return KeyIsPressed(KEY_LALT) || KeyIsPressed(KEY_RALT); }
	bool KeyIsPressed(int Key) const override;
	bool KeyPress(int Key) const override;
	const char *KeyName(int Key) const override;
	int FindKeyByName(const char *pKeyName) const override;

	size_t NumJoysticks() const override { return m_vJoysticks.size(); }
	CJoystick *GetJoystick(size_t Index) override { return &m_vJoysticks[Index]; }
	CJoystick *GetActiveJoystick() override { return m_pActiveJoystick; }
	void SetActiveJoystick(size_t Index) override;

	bool MouseRelative(float *pX, float *pY) override;
	bool SecondaryMouseRelative(float *pX, float *pY) override;
	bool HasSecondaryMouse() const override { return m_SecondaryMouseId != 0; }
	const std::vector<std::string> &MouseNames() const override { return m_vMouseNames; }
	const std::vector<std::string> &KeyboardNames() const override { return m_vKeyboardNames; }
	void MouseModeAbsolute() override;
	void MouseModeRelative() override;
	vec2 NativeMousePos() const override;
	bool NativeMousePressed(int Index) const override;

	const std::vector<CTouchFingerState> &TouchFingerStates() const override;
	void ClearTouchDeltas() override;

	std::string GetClipboardText() override;
	void SetClipboardText(const char *pText) override;

	void StartTextInput() override;
	void StopTextInput() override;
	void EnsureScreenKeyboardShown() override;
	void ClearComposition() override;
	const char *GetComposition() const override { return m_CompositionString.c_str(); }
	bool HasComposition() const override { return !m_CompositionString.empty(); }
	int GetCompositionCursor() const override { return m_CompositionCursor; }
	int GetCompositionLength() const override { return m_CompositionString.length(); }
	const char *GetCandidate(int Index) const override { return m_vCandidates[Index].c_str(); }
	int GetCandidateCount() const override { return m_vCandidates.size(); }
	int GetCandidateSelectedIndex() const override { return m_CandidateSelectedIndex; }
	void SetCompositionWindowPosition(float X, float Y, float H) override;

	bool GetDropFile(char *aBuf, int Len) override;
};

#endif
