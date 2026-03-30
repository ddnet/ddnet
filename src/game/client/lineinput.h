/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_LINEINPUT_H
#define GAME_CLIENT_LINEINPUT_H

#include <engine/input.h>

// line input helper
class CLineInput
{
	enum
	{
		MAX_SIZE = 512,
		MAX_CHARS = MAX_SIZE / 2,
	};
	char m_aStr[MAX_SIZE];
	int m_Len;
	int m_CursorPos;
	int m_NumChars;

	char m_aDisplayStr[MAX_SIZE + IInput::INPUT_TEXT_SIZE + 2];
	int m_FakeLen;
	int m_FakeCursorPos;

public:
	enum ELineInputChanges
	{
		// string was changed
		LINE_INPUT_CHANGE_STRING = 1 << 0,
		// characters were removed from the string
		LINE_INPUT_CHANGE_CHARACTERS_DELETE = 1 << 1,
		// cursor was changed or tried to change(e.g. pressing right at the last char in the string)
		LINE_INPUT_CHANGE_CURSOR = 1 << 2,
		LINE_INPUT_CHANGE_WARP_CURSOR = 1 << 3,
	};
	enum ELineInputModifyFlags
	{
		// don't delete characters
		LINE_INPUT_MODIFY_DONT_DELETE = 1 << 0,
	};
	static int32_t Manipulate(IInput::CEvent e, char *pStr, int StrMaxSize, int StrMaxChars, int *pStrLenPtr, int *pCursorPosPtr, int *pNumCharsPtr, int32_t ModifyFlags, int ModifierKey);

	CLineInput();
	void Clear();
	void ProcessInput(IInput::CEvent e);
	void Editing(const char *pString, int Cursor);
	void Set(const char *pString);
	void SetRange(const char *pString, int Begin, int End);
	void Insert(const char *pString, int Begin);
	void Append(const char *pString);
	const char *GetString(bool Editing = false) const { return Editing ? m_aDisplayStr : m_aStr; }
	int GetLength(bool Editing = false) const { return Editing ? m_FakeLen : m_Len; }
	int GetCursorOffset(bool Editing = false) const { return Editing ? m_FakeCursorPos : m_CursorPos; }
	void SetCursorOffset(int Offset) { m_CursorPos = Offset > m_Len ? m_Len : Offset < 0 ? 0 : Offset; }
};

#endif
