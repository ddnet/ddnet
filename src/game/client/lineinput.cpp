/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include "lineinput.h"
#include <engine/keys.h>

CLineInput::CLineInput()
{
	Clear();
}

void CLineInput::Clear()
{
	Set("");
}

void CLineInput::Set(const char *pString)
{
	str_copy(m_aStr, pString);
	str_utf8_stats(m_aStr, MAX_SIZE, MAX_CHARS, &m_Len, &m_NumChars);
	m_CursorPos = m_Len;
}

void CLineInput::SetRange(const char *pString, int Begin, int End)
{
	if(Begin > End)
		std::swap(Begin, End);
	Begin = clamp(Begin, 0, m_Len);
	End = clamp(End, 0, m_Len);

	int RemovedCharSize, RemovedCharCount;
	str_utf8_stats(m_aStr + Begin, End - Begin + 1, MAX_CHARS, &RemovedCharSize, &RemovedCharCount);

	int AddedCharSize, AddedCharCount;
	str_utf8_stats(pString, MAX_SIZE - m_Len + RemovedCharSize, MAX_CHARS - m_NumChars + RemovedCharCount, &AddedCharSize, &AddedCharCount);

	if(RemovedCharSize || AddedCharSize)
	{
		if(AddedCharSize < RemovedCharSize)
		{
			if(AddedCharSize)
				mem_copy(m_aStr + Begin, pString, AddedCharSize);
			mem_move(m_aStr + Begin + AddedCharSize, m_aStr + Begin + RemovedCharSize, m_Len - Begin - AddedCharSize);
		}
		else if(AddedCharSize > RemovedCharSize)
			mem_move(m_aStr + End + AddedCharSize - RemovedCharSize, m_aStr + End, m_Len - End);

		if(AddedCharSize >= RemovedCharSize)
			mem_copy(m_aStr + Begin, pString, AddedCharSize);

		m_CursorPos = End - RemovedCharSize + AddedCharSize;
		m_Len += AddedCharSize - RemovedCharSize;
		m_NumChars += AddedCharCount - RemovedCharCount;
		m_aStr[m_Len] = '\0';
	}
}

void CLineInput::Editing(const char *pString, int Cursor)
{
	str_copy(m_aDisplayStr, m_aStr);
	char aEditingText[IInput::INPUT_TEXT_SIZE + 2];
	str_format(aEditingText, sizeof(aEditingText), "[%s]", pString);
	int NewTextLen = str_length(aEditingText);
	int CharsLeft = (int)sizeof(m_aDisplayStr) - str_length(m_aDisplayStr) - 1;
	int FillCharLen = NewTextLen < CharsLeft ? NewTextLen : CharsLeft;
	for(int i = str_length(m_aDisplayStr) - 1; i >= m_CursorPos; i--)
		m_aDisplayStr[i + FillCharLen] = m_aDisplayStr[i];
	for(int i = 0; i < FillCharLen; i++)
		m_aDisplayStr[m_CursorPos + i] = aEditingText[i];
	m_aDisplayStr[m_CursorPos + FillCharLen] = '\0';
	m_FakeLen = str_length(m_aDisplayStr);
	m_FakeCursorPos = m_CursorPos + Cursor + 1;
}

void CLineInput::Insert(const char *pString, int Begin)
{
	SetRange(pString, Begin, Begin);
}

void CLineInput::Append(const char *pString)
{
	Insert(pString, m_Len);
}

static bool IsNotAWordChar(signed char c)
{
	return (c > 0 && c < '0') || (c > '9' && c < 'A') || (c > 'Z' && c < 'a') || (c > 'z'); // all non chars in ascii -- random
}

int32_t CLineInput::Manipulate(IInput::CEvent Event, char *pStr, int StrMaxSize, int StrMaxChars, int *pStrLenPtr, int *pCursorPosPtr, int *pNumCharsPtr, int32_t ModifyFlags, int ModifierKey)
{
	int NumChars = *pNumCharsPtr;
	int CursorPos = *pCursorPosPtr;
	int Len = *pStrLenPtr;
	int32_t Changes = 0;

	if(CursorPos > Len)
		CursorPos = Len;

	if(Event.m_Flags & IInput::FLAG_TEXT)
	{
		// gather string stats
		int CharCount = 0;
		int CharSize = 0;
		str_utf8_stats(Event.m_aText, MAX_SIZE, MAX_CHARS, &CharSize, &CharCount);

		// add new string
		if(CharCount)
		{
			if(Len + CharSize < StrMaxSize && CursorPos + CharSize < StrMaxSize && NumChars + CharCount < StrMaxChars)
			{
				mem_move(pStr + CursorPos + CharSize, pStr + CursorPos, Len - CursorPos + 1); // +1 == null term
				for(int i = 0; i < CharSize; i++)
					pStr[CursorPos + i] = Event.m_aText[i];
				CursorPos += CharSize;
				Len += CharSize;
				NumChars += CharCount;
				Changes |= ELineInputChanges::LINE_INPUT_CHANGE_STRING;
			}
		}
	}

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		int Key = Event.m_Key;
		if(Key == KEY_BACKSPACE)
		{
			if((ModifyFlags & LINE_INPUT_MODIFY_DONT_DELETE) == 0 && CursorPos > 0)
			{
				int NewCursorPos = str_utf8_rewind(pStr, CursorPos);
				int CharSize = CursorPos - NewCursorPos;
				mem_move(pStr + NewCursorPos, pStr + CursorPos, Len - NewCursorPos - CharSize + 1); // +1 == null term
				CursorPos = NewCursorPos;
				Len -= CharSize;
				if(CharSize > 0)
					--NumChars;
			}
			Changes |= ELineInputChanges::LINE_INPUT_CHANGE_CHARACTERS_DELETE;
		}
		else if(Key == KEY_DELETE)
		{
			if((ModifyFlags & LINE_INPUT_MODIFY_DONT_DELETE) == 0 && CursorPos < Len)
			{
				int p = str_utf8_forward(pStr, CursorPos);
				int CharSize = p - CursorPos;
				mem_move(pStr + CursorPos, pStr + CursorPos + CharSize, Len - CursorPos - CharSize + 1); // +1 == null term
				Len -= CharSize;
				if(CharSize > 0)
					--NumChars;
			}
			Changes |= ELineInputChanges::LINE_INPUT_CHANGE_CHARACTERS_DELETE;
		}
		else if(Key == KEY_LEFT)
		{
			if(ModifierKey == KEY_LCTRL || ModifierKey == KEY_RCTRL || ModifierKey == KEY_LGUI || ModifierKey == KEY_RGUI)
			{
				bool MovedCursor = false;
				int OldCursorPos = CursorPos;
				CursorPos = str_utf8_rewind(pStr, CursorPos);
				if(OldCursorPos != CursorPos)
					MovedCursor = true;
				bool WasNonWordChar = IsNotAWordChar(pStr[CursorPos]);
				while((!WasNonWordChar && !IsNotAWordChar(pStr[CursorPos])) || (WasNonWordChar && IsNotAWordChar(pStr[CursorPos])))
				{
					CursorPos = str_utf8_rewind(pStr, CursorPos);
					if(CursorPos == 0)
						break;
				}
				if(MovedCursor && ((!WasNonWordChar && IsNotAWordChar(pStr[CursorPos])) || (WasNonWordChar && !IsNotAWordChar(pStr[CursorPos]))))
					CursorPos = str_utf8_forward(pStr, CursorPos);
				Changes |= ELineInputChanges::LINE_INPUT_CHANGE_WARP_CURSOR;
			}
			else
			{
				if(CursorPos > 0)
					CursorPos = str_utf8_rewind(pStr, CursorPos);
				Changes |= ELineInputChanges::LINE_INPUT_CHANGE_CURSOR;
			}
		}
		else if(Key == KEY_RIGHT)
		{
			if(ModifierKey == KEY_LCTRL || ModifierKey == KEY_RCTRL || ModifierKey == KEY_LGUI || ModifierKey == KEY_RGUI)
			{
				bool WasNonWordChar = IsNotAWordChar(pStr[CursorPos]);
				while((!WasNonWordChar && !IsNotAWordChar(pStr[CursorPos])) || (WasNonWordChar && IsNotAWordChar(pStr[CursorPos])))
				{
					CursorPos = str_utf8_forward(pStr, CursorPos);
					if(CursorPos >= Len)
						break;
				}
				Changes |= ELineInputChanges::LINE_INPUT_CHANGE_WARP_CURSOR;
			}
			else
			{
				if(CursorPos < Len)
					CursorPos = str_utf8_forward(pStr, CursorPos);
				Changes |= ELineInputChanges::LINE_INPUT_CHANGE_CURSOR;
			}
		}
		else if(Key == KEY_HOME)
		{
			CursorPos = 0;
			Changes |= ELineInputChanges::LINE_INPUT_CHANGE_WARP_CURSOR;
		}
		else if(Key == KEY_END)
		{
			CursorPos = Len;
			Changes |= ELineInputChanges::LINE_INPUT_CHANGE_WARP_CURSOR;
		}
	}

	*pNumCharsPtr = NumChars;
	*pCursorPosPtr = CursorPos;
	*pStrLenPtr = Len;

	return Changes;
}

void CLineInput::ProcessInput(IInput::CEvent e)
{
	Manipulate(e, m_aStr, MAX_SIZE, MAX_CHARS, &m_Len, &m_CursorPos, &m_NumChars, 0, 0);
}
