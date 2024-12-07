/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include "kernel.h"

#include <base/types.h>
#include <base/vmath.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

const int g_MaxKeys = 512;
extern const char g_aaKeyStrings[g_MaxKeys][20];

class IInput : public IInterface
{
	MACRO_INTERFACE("input")
public:
	class CEvent
	{
	public:
		int m_Flags;
		int m_Key;
		uint32_t m_InputCount;
		char m_aText[32]; // SDL_TEXTINPUTEVENT_TEXT_SIZE
	};

	enum
	{
		FLAG_PRESS = 1 << 0,
		FLAG_RELEASE = 1 << 1,
		FLAG_TEXT = 1 << 2,
	};
	enum ECursorType
	{
		CURSOR_NONE,
		CURSOR_MOUSE,
		CURSOR_JOYSTICK,
	};

	// events
	virtual void ConsumeEvents(std::function<void(const CEvent &Event)> Consumer) const = 0;
	virtual void Clear() = 0;

	/**
	 * @return Rolling average of the time in seconds between
	 * calls of the Update function.
	 */
	virtual float GetUpdateTime() const = 0;

	// keys
	virtual bool ModifierIsPressed() const = 0;
	virtual bool ShiftIsPressed() const = 0;
	virtual bool AltIsPressed() const = 0;
	virtual bool KeyIsPressed(int Key) const = 0;
	virtual bool KeyPress(int Key, bool CheckCounter = false) const = 0;
	const char *KeyName(int Key) const { return (Key >= 0 && Key < g_MaxKeys) ? g_aaKeyStrings[Key] : g_aaKeyStrings[0]; }
	virtual int FindKeyByName(const char *pKeyName) const = 0;

	// joystick
	class IJoystick
	{
	public:
		virtual int GetIndex() const = 0;
		virtual const char *GetName() const = 0;
		virtual int GetNumAxes() const = 0;
		virtual int GetNumButtons() const = 0;
		virtual int GetNumBalls() const = 0;
		virtual int GetNumHats() const = 0;
		virtual float GetAxisValue(int Axis) = 0;
		virtual void GetHatValue(int Hat, int (&HatKeys)[2]) = 0;
		virtual bool Relative(float *pX, float *pY) = 0;
		virtual bool Absolute(float *pX, float *pY) = 0;
	};
	virtual size_t NumJoysticks() const = 0;
	virtual IJoystick *GetJoystick(size_t Index) = 0;
	virtual IJoystick *GetActiveJoystick() = 0;
	virtual void SetActiveJoystick(size_t Index) = 0;

	// mouse
	virtual vec2 NativeMousePos() const = 0;
	virtual bool NativeMousePressed(int Index) const = 0;
	virtual void MouseModeRelative() = 0;
	virtual void MouseModeAbsolute() = 0;
	virtual bool MouseRelative(float *pX, float *pY) = 0;

	// touch
	/**
	 * Represents a unique finger for a current touch event. If there are multiple touch input devices, they
	 * are handled transparently like different fingers. The concrete values of the member variables of this
	 * class are arbitrary based on the touch device driver and should only be used to uniquely identify touch
	 * fingers. Note that once a finger has been released, the same finger value may also be reused again.
	 */
	class CTouchFinger
	{
		friend class CInput;

		int64_t m_DeviceId;
		int64_t m_FingerId;

	public:
		bool operator==(const CTouchFinger &Other) const { return m_DeviceId == Other.m_DeviceId && m_FingerId == Other.m_FingerId; }
		bool operator!=(const CTouchFinger &Other) const { return !(*this == Other); }
	};
	/**
	 * Represents the state of a particular touch finger currently being pressed down on a touch device.
	 */
	class CTouchFingerState
	{
	public:
		/**
		 * The unique finger which this state is associated with.
		 */
		CTouchFinger m_Finger;
		/**
		 * The current position of the finger. The x- and y-components of the position are normalized to the
		 * range `0.0f`-`1.0f` representing the absolute position of the finger on the current touch device.
		 */
		vec2 m_Position;
		/**
		 * The current delta of the finger. The x- and y-components of the delta are normalized to the
		 * range `0.0f`-`1.0f` representing the absolute delta of the finger on the current touch device.
		 *
		 * @remark This is reset to zero at the end of each frame.
		 */
		vec2 m_Delta;
		/**
		 * The time when this finger was first pressed down.
		 */
		std::chrono::nanoseconds m_PressTime;
	};
	/**
	 * Returns a vector of the states of all touch fingers currently being pressed down on touch devices.
	 * Note that this only contains fingers which are pressed down, i.e. released fingers are never stored.
	 * The order of the fingers in this vector is based on the order in which the fingers where pressed.
	 *
	 * @return vector of all touch finger states
	 */
	virtual const std::vector<CTouchFingerState> &TouchFingerStates() const = 0;
	/**
	 * Must be called after the touch finger states have been used during the client update to ensure that
	 * touch deltas are only accumulated until the next update. If the touch states are only used during
	 * rendering, i.e. for user interfaces, then this is called automatically by calling @link Clear @endlink.
	 */
	virtual void ClearTouchDeltas() = 0;

	// clipboard
	virtual std::string GetClipboardText() = 0;
	virtual void SetClipboardText(const char *pText) = 0;

	// text editing
	virtual void StartTextInput() = 0;
	virtual void StopTextInput() = 0;
	virtual const char *GetComposition() const = 0;
	virtual bool HasComposition() const = 0;
	virtual int GetCompositionCursor() const = 0;
	virtual int GetCompositionLength() const = 0;
	virtual const char *GetCandidate(int Index) const = 0;
	virtual int GetCandidateCount() const = 0;
	virtual int GetCandidateSelectedIndex() const = 0;
	virtual void SetCompositionWindowPosition(float X, float Y, float H) = 0;

	virtual bool GetDropFile(char *aBuf, int Len) = 0;

	ECursorType CursorRelative(float *pX, float *pY)
	{
		if(MouseRelative(pX, pY))
			return CURSOR_MOUSE;
		IJoystick *pJoystick = GetActiveJoystick();
		if(pJoystick && pJoystick->Relative(pX, pY))
			return CURSOR_JOYSTICK;
		return CURSOR_NONE;
	}
};

class IEngineInput : public IInput
{
	MACRO_INTERFACE("engineinput")
public:
	virtual void Init() = 0;
	virtual void Shutdown() override = 0;
	virtual int Update() = 0;
};

extern IEngineInput *CreateEngineInput();

#endif
