#include "keyboard.h"

#include "keynames.h"
#include "keynames_readable.h"

#include <base/dbg.h>
#include <base/str.h>

#include <engine/keys.h>

#include <game/localization.h>

const char *KeyName(int Key)
{
	dbg_assert(Key >= KEY_FIRST && Key < KEY_LAST, "Key invalid: %d", Key);
	return g_aaKeyStrings[Key];
}

const char *KeyNameHumanReadable(int Key)
{
	dbg_assert(Key >= KEY_FIRST && Key < KEY_LAST, "Key invalid: %d", Key);
	return KeyNamesHumanReadable()[Key];
}

static void CapitalizeFirst(const char *pSrc, char *pDst, int DstSize)
{
	if(pSrc[0] == '\0' || DstSize < 2)
	{
		pDst[0] = '\0';
		return;
	}
	pDst[0] = str_uppercase(pSrc[0]);
	str_copy(pDst + 1, pSrc + 1, DstSize - 1);
}

void GenerateKeyNameHumanReadable(char *pHumanReadableKeyName, int Key, int KeyNameSize)
{
	dbg_assert(KeyNameSize > 0 && pHumanReadableKeyName != nullptr, "Invalid KeyName buffer");

	if(Key == KEY_UNKNOWN)
	{
		pHumanReadableKeyName[0] = '\0';
		return;
	}

	const char *pKeyName = KeyName(Key);
	const char *pNumPad = str_startswith(pKeyName, "kp_");
	if(pNumPad)
	{
		char aSuffix[16];
		CapitalizeFirst(pNumPad, aSuffix, sizeof(aSuffix));
		str_format(pHumanReadableKeyName, KeyNameSize, Localize("Numpad %s", "key"), aSuffix);
	}
	else if(const char *pMouse = str_startswith(pKeyName, "mouse"))
	{
		switch(pMouse[0])
		{
		case '1':
			str_copy(pHumanReadableKeyName, Localize("Left mouse", "key"), KeyNameSize);
			break;
		case '2':
			str_copy(pHumanReadableKeyName, Localize("Right mouse", "key"), KeyNameSize);
			break;
		case '3':
			str_copy(pHumanReadableKeyName, Localize("Middle mouse", "key"), KeyNameSize);
			break;
		default:
		{
			const char *pWheel = str_startswith(pMouse, "wheel");
			if(pWheel)
			{
				if(pWheel[0] == '\0')
				{
					str_copy(pHumanReadableKeyName, Localize("Mouse wheel", "key"), KeyNameSize);
				}
				else
				{
					char aDir[16];
					CapitalizeFirst(pWheel, aDir, sizeof(aDir));
					str_format(pHumanReadableKeyName, KeyNameSize, Localize("Mouse wheel %s", "key"), aDir);
				}
			}
			else
			{
				str_format(pHumanReadableKeyName, KeyNameSize, Localize("Mouse %s", "key"), pMouse);
			}
		}
		break;
		}
	}
	else if(const char *pJoystickButton = str_startswith(pKeyName, "joystick"))
	{
		str_format(pHumanReadableKeyName, KeyNameSize, Localize("Joystick Button %s", "key"), pJoystickButton);
	}
	else if(const char *pHat = str_startswith(pKeyName, "joy_hat"))
	{
		char aNumber[8], aDirection[8];
		const char *pRest = pHat;
		pRest = str_next_token(pRest, "_", aNumber, sizeof(aNumber));
		str_next_token(pRest, "_", aDirection, sizeof(aDirection));
		char aDir[16];
		CapitalizeFirst(aDirection, aDir, sizeof(aDir));
		str_format(pHumanReadableKeyName, KeyNameSize, Localize("D-Pad %s %s", "key"), aNumber, aDir);
	}
	else if(const char *pAxis = str_startswith(pKeyName, "joy_axis"))
	{
		char aNumber[8], aDirection[8];
		const char *pRest = pAxis;
		pRest = str_next_token(pRest, "_", aNumber, sizeof(aNumber));
		str_next_token(pRest, "_", aDirection, sizeof(aDirection));
		char aDir[16];
		CapitalizeFirst(aDirection, aDir, sizeof(aDir));
		str_format(pHumanReadableKeyName, KeyNameSize, Localize("Stick %s %s", "key"), aNumber, aDir);
	}
	else if(pKeyName[0] == '&')
	{
		str_format(pHumanReadableKeyName, KeyNameSize, Localize("Button %s", "key"), pKeyName + 1);
	}
	else if((pKeyName[0] == 'r' || pKeyName[0] == 'l') &&
		(str_comp("alt", pKeyName + 1) == 0 || str_comp("shift", pKeyName + 1) == 0 || str_comp("ctrl", pKeyName + 1) == 0 || str_comp("gui", pKeyName + 1) == 0))
	{
		char aSuffix[16];
		CapitalizeFirst(pKeyName + 1, aSuffix, sizeof(aSuffix));
		if(pKeyName[0] == 'r')
			str_format(pHumanReadableKeyName, KeyNameSize, Localize("Right %s", "key"), aSuffix);
		else
			str_format(pHumanReadableKeyName, KeyNameSize, Localize("Left %s", "key"), aSuffix);
	}
	else
	{
		CapitalizeFirst(pKeyName, pHumanReadableKeyName, KeyNameSize);
	}
}
