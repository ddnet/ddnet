#include <gtest/gtest.h>

#include <base/system.h>

bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

bool IsValidEditorTooltip(const char *pTooltip, char *pErrorMsg, int ErrorMsgSize)
{
	pErrorMsg[0] = '\0';
	char aHotkey[512];
	aHotkey[0] = '\0';

	if(pTooltip[0] == '[')
	{
		str_copy(aHotkey, pTooltip + 1);
		const char *pHotkeyEnd = str_find(aHotkey, "]");
		if(!pHotkeyEnd)
		{
			str_copy(pErrorMsg, "tooltip missing closing square bracket", ErrorMsgSize);
			return false;
		}
		aHotkey[pHotkeyEnd - aHotkey] = '\0';

		for(int i = 0; aHotkey[i]; i++)
		{
			bool ExpectLowerCase = true;
			if(i == 0)
			{
				ExpectLowerCase = false;
			}
			else if(!is_letter(aHotkey[i - 1]))
			{
				// the first character of a word should be uppercase
				ExpectLowerCase = false;
			}

			bool IsLower = aHotkey[i] >= 'a' && aHotkey[i] <= 'z';
			bool IsUpper = aHotkey[i] >= 'A' && aHotkey[i] <= 'Z';

			if(ExpectLowerCase && IsUpper)
			{
				str_format(pErrorMsg, ErrorMsgSize, "expected character '%c' at index %d to be lower case", aHotkey[i], i + 1);
				return false;
			}
			if(!ExpectLowerCase && IsLower)
			{
				str_format(pErrorMsg, ErrorMsgSize, "expected character '%c' at index %d to be upper case", aHotkey[i], i + 1);
				return false;
			}
		}
	}

	const char *pParenthesis = str_find(pTooltip, "(");
	if(pParenthesis)
	{
		const char *pHotkey = str_find_nocase(pParenthesis, "ctrl");
		if(!pHotkey)
			pHotkey = str_find_nocase(pParenthesis, "shift");
		if(!pHotkey)
			pHotkey = str_find_nocase(pParenthesis, "home");

		if(pHotkey)
		{
			int Offset = pHotkey - pTooltip;
			str_format(pErrorMsg, ErrorMsgSize, "found hotkey at offset %d. Hotkeys must be defined at the start.", Offset);
			return false;
		}
	}

	if(!str_endswith(pTooltip, "."))
	{
		str_copy(pErrorMsg, "tooltip has to end with a dot", ErrorMsgSize);
		return false;
	}
	return true;
}

void AssertTooltip(const char *pTooltip)
{
	char aError[512];
	bool IsValid = IsValidEditorTooltip(pTooltip, aError, sizeof(aError));
	if(!IsValid)
	{
		dbg_msg("test", "Invalid tooltip: %s", pTooltip);
		dbg_msg("test", "ERROR: %s", aError);
	}
	EXPECT_TRUE(IsValid);
}

TEST(Editor, QuickActionNames)
{
	char aError[512];
	EXPECT_TRUE(IsValidEditorTooltip("hello world.", aError, sizeof(aError)));
	EXPECT_TRUE(IsValidEditorTooltip("[Ctrl+H] hello world.", aError, sizeof(aError)));
	EXPECT_FALSE(IsValidEditorTooltip("[Ctrl+H hello world.", aError, sizeof(aError)));
	EXPECT_FALSE(IsValidEditorTooltip("[ctrl+h] hello world.", aError, sizeof(aError)));
	EXPECT_FALSE(IsValidEditorTooltip("hello world", aError, sizeof(aError)));
	EXPECT_FALSE(IsValidEditorTooltip("hello world (Ctrl+H).", aError, sizeof(aError)));

#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, button_color, description) AssertTooltip(description);
#include <game/editor/quick_actions.h>
#undef REGISTER_QUICK_ACTION
}
