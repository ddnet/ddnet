/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "binds.h"
#include <engine/config.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

static const ColorRGBA gs_BindPrintColor{1.0f, 1.0f, 0.8f, 1.0f};

bool CBinds::CBindsSpecial::OnInput(IInput::CEvent Event)
{
	// only handle F and composed F binds
	if(((Event.m_Key >= KEY_F1 && Event.m_Key <= KEY_F12) || (Event.m_Key >= KEY_F13 && Event.m_Key <= KEY_F24)) && (Event.m_Key != KEY_F5 || !m_pClient->m_Menus.IsActive()))
	{
		int Mask = CBinds::GetModifierMask(Input());

		// Look for a composed bind
		bool ret = false;
		if(m_pBinds->m_aapKeyBindings[Mask][Event.m_Key])
		{
			m_pBinds->GetConsole()->ExecuteLineStroked(Event.m_Flags & IInput::FLAG_PRESS, m_pBinds->m_aapKeyBindings[Mask][Event.m_Key]);
			ret = true;
		}
		// Look for a non composed bind
		if(!ret && m_pBinds->m_aapKeyBindings[0][Event.m_Key])
		{
			m_pBinds->GetConsole()->ExecuteLineStroked(Event.m_Flags & IInput::FLAG_PRESS, m_pBinds->m_aapKeyBindings[0][Event.m_Key]);
			ret = true;
		}

		return ret;
	}

	return false;
}

CBinds::CBinds()
{
	mem_zero(m_aapKeyBindings, sizeof(m_aapKeyBindings));
	m_SpecialBinds.m_pBinds = this;
}

CBinds::~CBinds()
{
	for(int i = 0; i < KEY_LAST; i++)
		for(auto &apKeyBinding : m_aapKeyBindings)
			free(apKeyBinding[i]);
}

void CBinds::Bind(int KeyID, const char *pStr, bool FreeOnly, int ModifierCombination)
{
	if(KeyID < 0 || KeyID >= KEY_LAST)
		return;

	if(FreeOnly && Get(KeyID, ModifierCombination)[0])
		return;

	free(m_aapKeyBindings[ModifierCombination][KeyID]);
	m_aapKeyBindings[ModifierCombination][KeyID] = 0;

	// skip modifiers for +xxx binds
	if(pStr[0] == '+')
		ModifierCombination = 0;

	char aBuf[256];
	if(!pStr[0])
	{
		str_format(aBuf, sizeof(aBuf), "unbound %s%s (%d)", GetKeyBindModifiersName(ModifierCombination), Input()->KeyName(KeyID), KeyID);
	}
	else
	{
		int Size = str_length(pStr) + 1;
		m_aapKeyBindings[ModifierCombination][KeyID] = (char *)malloc(Size);
		str_copy(m_aapKeyBindings[ModifierCombination][KeyID], pStr, Size);
		str_format(aBuf, sizeof(aBuf), "bound %s%s (%d) = %s", GetKeyBindModifiersName(ModifierCombination), Input()->KeyName(KeyID), KeyID, m_aapKeyBindings[ModifierCombination][KeyID]);
	}
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "binds", aBuf, gs_BindPrintColor);
}

int CBinds::GetModifierMask(IInput *pInput)
{
	int Mask = 0;
	static const auto s_aModifierKeys = {
		KEY_LSHIFT,
		KEY_RSHIFT,
		KEY_LCTRL,
		KEY_RCTRL,
		KEY_LALT,
		KEY_RALT,
		KEY_LGUI,
		KEY_RGUI,
	};
	for(const auto Key : s_aModifierKeys)
	{
		if(pInput->KeyIsPressed(Key))
		{
			Mask |= GetModifierMaskOfKey(Key);
		}
	}

	return Mask;
}

int CBinds::GetModifierMaskOfKey(int Key)
{
	switch(Key)
	{
	case KEY_LSHIFT:
	case KEY_RSHIFT:
		return 1 << CBinds::MODIFIER_SHIFT;
	case KEY_LCTRL:
	case KEY_RCTRL:
		return 1 << CBinds::MODIFIER_CTRL;
	case KEY_LALT:
	case KEY_RALT:
		return 1 << CBinds::MODIFIER_ALT;
	case KEY_LGUI:
	case KEY_RGUI:
		return 1 << CBinds::MODIFIER_GUI;
	default:
		return 0;
	}
}

bool CBinds::OnInput(IInput::CEvent e)
{
	// don't handle invalid events
	if(e.m_Key <= 0 || e.m_Key >= KEY_LAST)
		return false;

	int Mask = GetModifierMask(Input());
	int KeyModifierMask = GetModifierMaskOfKey(e.m_Key);
	Mask &= ~KeyModifierMask;

	bool ret = false;
	if(m_aapKeyBindings[Mask][e.m_Key])
	{
		if(e.m_Flags & IInput::FLAG_PRESS)
			Console()->ExecuteLineStroked(1, m_aapKeyBindings[Mask][e.m_Key]);
		if(e.m_Flags & IInput::FLAG_RELEASE)
			Console()->ExecuteLineStroked(0, m_aapKeyBindings[Mask][e.m_Key]);
		ret = true;
	}

	if(m_aapKeyBindings[0][e.m_Key] && !ret)
	{
		// When ctrl+shift are pressed (ctrl+shift binds and also the hard-coded ctrl+shift+d, ctrl+shift+g, ctrl+shift+e), ignore other +xxx binds
		if(e.m_Flags & IInput::FLAG_PRESS && Mask != ((1 << MODIFIER_CTRL) | (1 << MODIFIER_SHIFT)) && Mask != ((1 << MODIFIER_GUI) | (1 << MODIFIER_SHIFT)))
			Console()->ExecuteLineStroked(1, m_aapKeyBindings[0][e.m_Key]);
		if(e.m_Flags & IInput::FLAG_RELEASE)
			Console()->ExecuteLineStroked(0, m_aapKeyBindings[0][e.m_Key]);
		ret = true;
	}

	return ret;
}

void CBinds::UnbindAll()
{
	for(auto &apKeyBinding : m_aapKeyBindings)
	{
		for(auto &pKeyBinding : apKeyBinding)
		{
			free(pKeyBinding);
			pKeyBinding = 0;
		}
	}
}

const char *CBinds::Get(int KeyID, int ModifierCombination)
{
	if(KeyID > 0 && KeyID < KEY_LAST && m_aapKeyBindings[ModifierCombination][KeyID])
		return m_aapKeyBindings[ModifierCombination][KeyID];
	return "";
}

void CBinds::GetKey(const char *pBindStr, char *aBuf, unsigned BufSize)
{
	aBuf[0] = 0;
	for(int Mod = 0; Mod < MODIFIER_COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = Get(KeyId, Mod);
			if(!pBind[0])
				continue;

			if(str_comp(pBind, pBindStr) == 0)
			{
				if(Mod)
					str_format(aBuf, BufSize, "%s+%s", GetModifierName(Mod), Input()->KeyName(KeyId));
				else
					str_format(aBuf, BufSize, "%s", Input()->KeyName(KeyId));
				return;
			}
		}
	}
}

void CBinds::SetDefaults()
{
	// set default key bindings
	UnbindAll();
	Bind(KEY_F1, "toggle_local_console");
	Bind(KEY_F2, "toggle_remote_console");
	Bind(KEY_TAB, "+scoreboard");
	Bind(KEY_EQUALS, "+statboard");
	Bind(KEY_F10, "screenshot");

	Bind(KEY_A, "+left");
	Bind(KEY_D, "+right");

	Bind(KEY_SPACE, "+jump");
	Bind(KEY_MOUSE_1, "+fire");
	Bind(KEY_MOUSE_2, "+hook");
	Bind(KEY_LSHIFT, "+emote");
	Bind(KEY_RETURN, "+show_chat; chat all");
	Bind(KEY_RIGHT, "spectate_next");
	Bind(KEY_LEFT, "spectate_previous");
	Bind(KEY_RSHIFT, "+spectate");

	Bind(KEY_1, "+weapon1");
	Bind(KEY_2, "+weapon2");
	Bind(KEY_3, "+weapon3");
	Bind(KEY_4, "+weapon4");
	Bind(KEY_5, "+weapon5");

	Bind(KEY_MOUSE_WHEEL_UP, "+prevweapon");
	Bind(KEY_MOUSE_WHEEL_DOWN, "+nextweapon");

	Bind(KEY_T, "+show_chat; chat all");
	Bind(KEY_Y, "+show_chat; chat team");
	Bind(KEY_U, "+show_chat");
	Bind(KEY_I, "+show_chat; chat all /c ");

	Bind(KEY_F3, "vote yes");
	Bind(KEY_F4, "vote no");

	Bind(KEY_K, "kill");
	Bind(KEY_Q, "say /pause");
	Bind(KEY_P, "say /pause");

	// DDRace
	g_Config.m_ClDDRaceBindsSet = 0;
	SetDDRaceBinds(false);
}

void CBinds::OnConsoleInit()
{
	// bindings
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this);

	Console()->Register("bind", "s[key] r[command]", CFGFLAG_CLIENT, ConBind, this, "Bind key to execute the command");
	Console()->Register("binds", "?s[key]", CFGFLAG_CLIENT, ConBinds, this, "Print command executed by this keybinding or all binds");
	Console()->Register("unbind", "s[key]", CFGFLAG_CLIENT, ConUnbind, this, "Unbind key");
	Console()->Register("unbindall", "", CFGFLAG_CLIENT, ConUnbindAll, this, "Unbind all keys");

	// default bindings
	SetDefaults();
}

void CBinds::ConBind(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	const char *pBindStr = pResult->GetString(0);
	int Modifier;
	int KeyID = pBinds->GetBindSlot(pBindStr, &Modifier);

	if(!KeyID)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "key %s not found", pBindStr);
		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		return;
	}

	pBinds->Bind(KeyID, pResult->GetString(1), false, Modifier);
}

void CBinds::ConBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		char aBuf[256];
		const char *pKeyName = pResult->GetString(0);

		int Modifier;
		int KeyID = pBinds->GetBindSlot(pKeyName, &Modifier);
		if(!KeyID)
		{
			str_format(aBuf, sizeof(aBuf), "key '%s' not found", pKeyName);
			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		}
		else
		{
			if(!pBinds->m_aapKeyBindings[Modifier][KeyID])
				str_format(aBuf, sizeof(aBuf), "%s (%d) is not bound", pKeyName, KeyID);
			else
				str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", pKeyName, KeyID, pBinds->m_aapKeyBindings[Modifier][KeyID]);

			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		}
	}
	else if(pResult->NumArguments() == 0)
	{
		char aBuf[1024];
		for(int i = 0; i < MODIFIER_COMBINATION_COUNT; i++)
		{
			for(int j = 0; j < KEY_LAST; j++)
			{
				if(!pBinds->m_aapKeyBindings[i][j])
					continue;

				str_format(aBuf, sizeof(aBuf), "%s%s (%d) = %s", GetKeyBindModifiersName(i), pBinds->Input()->KeyName(j), j, pBinds->m_aapKeyBindings[i][j]);
				pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
			}
		}
	}
}

void CBinds::ConUnbind(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	const char *pKeyName = pResult->GetString(0);
	int Modifier;
	int KeyID = pBinds->GetBindSlot(pKeyName, &Modifier);

	if(!KeyID)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "key %s not found", pKeyName);
		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		return;
	}

	pBinds->Bind(KeyID, "", false, Modifier);
}

void CBinds::ConUnbindAll(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	pBinds->UnbindAll();
}

int CBinds::GetKeyID(const char *pKeyName)
{
	// check for numeric
	if(pKeyName[0] == '&')
	{
		int i = str_toint(pKeyName + 1);
		if(i > 0 && i < KEY_LAST)
			return i; // numeric
	}

	// search for key
	for(int i = 0; i < KEY_LAST; i++)
	{
		if(str_comp(pKeyName, Input()->KeyName(i)) == 0)
			return i;
	}

	return 0;
}

int CBinds::GetBindSlot(const char *pBindString, int *pModifierCombination)
{
	*pModifierCombination = MODIFIER_NONE;
	char aMod[32];
	aMod[0] = '\0';
	const char *pKey = str_next_token(pBindString, "+", aMod, sizeof(aMod));
	while(aMod[0] && *(pKey))
	{
		if(!str_comp(aMod, "shift"))
			*pModifierCombination |= (1 << MODIFIER_SHIFT);
		else if(!str_comp(aMod, "ctrl"))
			*pModifierCombination |= (1 << MODIFIER_CTRL);
		else if(!str_comp(aMod, "alt"))
			*pModifierCombination |= (1 << MODIFIER_ALT);
		else if(!str_comp(aMod, "gui"))
			*pModifierCombination |= (1 << MODIFIER_GUI);
		else
			return 0;

		if(str_find(pKey + 1, "+"))
			pKey = str_next_token(pKey + 1, "+", aMod, sizeof(aMod));
		else
			break;
	}
	return GetKeyID(*pModifierCombination == MODIFIER_NONE ? aMod : pKey + 1);
}

const char *CBinds::GetModifierName(int Modifier)
{
	switch(Modifier)
	{
	case MODIFIER_SHIFT:
		return "shift";
	case MODIFIER_CTRL:
		return "ctrl";
	case MODIFIER_ALT:
		return "alt";
	case MODIFIER_GUI:
		return "gui";
	case MODIFIER_NONE:
	default:
		return "";
	}
}

const char *CBinds::GetKeyBindModifiersName(int ModifierCombination)
{
	static char aModifier[256];
	aModifier[0] = '\0';
	for(int k = 1; k < MODIFIER_COUNT; k++)
	{
		if(ModifierCombination & (1 << k))
		{
			str_append(aModifier, GetModifierName(k), sizeof(aModifier));
			str_append(aModifier, "+", sizeof(aModifier));
		}
	}
	return aModifier;
}

void CBinds::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBinds *pSelf = (CBinds *)pUserData;

	pConfigManager->WriteLine("unbindall");
	for(int i = 0; i < MODIFIER_COMBINATION_COUNT; i++)
	{
		for(int j = 0; j < KEY_LAST; j++)
		{
			if(!pSelf->m_aapKeyBindings[i][j])
				continue;

			// worst case the str_escape can double the string length
			int Size = str_length(pSelf->m_aapKeyBindings[i][j]) * 2 + 30;
			char *pBuffer = (char *)malloc(Size);
			char *pEnd = pBuffer + Size;

			str_format(pBuffer, Size, "bind %s%s \"", GetKeyBindModifiersName(i), pSelf->Input()->KeyName(j));
			// process the string. we need to escape some characters
			char *pDst = pBuffer + str_length(pBuffer);
			str_escape(&pDst, pSelf->m_aapKeyBindings[i][j], pEnd);
			str_append(pBuffer, "\"", Size);

			pConfigManager->WriteLine(pBuffer);
			free(pBuffer);
		}
	}
}

// DDRace

void CBinds::SetDDRaceBinds(bool FreeOnly)
{
	if(g_Config.m_ClDDRaceBindsSet < 1)
	{
		Bind(KEY_KP_PLUS, "zoom+", FreeOnly);
		Bind(KEY_KP_MINUS, "zoom-", FreeOnly);
		Bind(KEY_KP_MULTIPLY, "zoom", FreeOnly);
		Bind(KEY_PAUSE, "say /pause", FreeOnly);
		Bind(KEY_UP, "+jump", FreeOnly);
		Bind(KEY_LEFT, "+left", FreeOnly);
		Bind(KEY_RIGHT, "+right", FreeOnly);
		Bind(KEY_LEFTBRACKET, "+prevweapon", FreeOnly);
		Bind(KEY_RIGHTBRACKET, "+nextweapon", FreeOnly);
		Bind(KEY_C, "say /rank", FreeOnly);
		Bind(KEY_V, "say /info", FreeOnly);
		Bind(KEY_B, "say /top5", FreeOnly);
		Bind(KEY_S, "+showhookcoll", FreeOnly);
		Bind(KEY_X, "toggle cl_dummy 0 1", FreeOnly);
		Bind(KEY_H, "toggle cl_dummy_hammer 0 1", FreeOnly);
		Bind(KEY_SLASH, "+show_chat; chat all /", FreeOnly);
		Bind(KEY_PAGEUP, "toggle cl_overlay_entities 0 100", FreeOnly);
		Bind(KEY_KP_0, "say /emote normal 999999", FreeOnly);
		Bind(KEY_KP_1, "say /emote happy 999999", FreeOnly);
		Bind(KEY_KP_2, "say /emote angry 999999", FreeOnly);
		Bind(KEY_KP_3, "say /emote pain 999999", FreeOnly);
		Bind(KEY_KP_4, "say /emote surprise 999999", FreeOnly);
		Bind(KEY_KP_5, "say /emote blink 999999", FreeOnly);
		Bind(KEY_MOUSE_3, "+spectate", FreeOnly);
		Bind(KEY_MINUS, "spectate_previous", FreeOnly);
		Bind(KEY_EQUALS, "spectate_next", FreeOnly);
	}

	g_Config.m_ClDDRaceBindsSet = 1;
}
