/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "binds.h"
#include <engine/config.h>
#include <engine/shared/config.h>

static const ColorRGBA gs_BindPrintColor{1.0f, 1.0f, 0.8f, 1.0f};

bool CBinds::CBindsSpecial::OnInput(IInput::CEvent Event)
{
	// only handle F and composed F binds
	if((Event.m_Key >= KEY::F1 && Event.m_Key <= KEY::F12) || (Event.m_Key >= KEY::F13 && Event.m_Key <= KEY::F24))
	{
		int Mask = m_pBinds->GetModifierMask(Input());

		// Look for a composed bind
		bool ret = false;
		for(int Mod = 1; Mod < MODIFIER_COMBINATION_COUNT; Mod++)
		{
			if(Mask == Mod && m_pBinds->m_aapKeyBindings[Mod][Event.m_Key])
			{
				m_pBinds->GetConsole()->ExecuteLineStroked(Event.m_Flags & IInput::FLAG_PRESS, m_pBinds->m_aapKeyBindings[Mod][Event.m_Key]);
				ret = true;
			}
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
	for(int i = 0; i < KEY::LAST; i++)
		for(auto &apKeyBinding : m_aapKeyBindings)
			if(apKeyBinding[i])
				free(apKeyBinding[i]);
}

void CBinds::Bind(int KeyID, const char *pStr, bool FreeOnly, int Modifier)
{
	if(KeyID < 0 || KeyID >= KEY::LAST)
		return;

	if(FreeOnly && Get(KeyID, Modifier)[0])
		return;

	if(m_aapKeyBindings[Modifier][KeyID])
	{
		free(m_aapKeyBindings[Modifier][KeyID]);
		m_aapKeyBindings[Modifier][KeyID] = 0;
	}

	// skip modifiers for +xxx binds
	if(pStr[0] == '+')
		Modifier = 0;

	char aBuf[256];
	if(!pStr[0])
	{
		str_format(aBuf, sizeof(aBuf), "unbound %s%s (%d)", GetKeyBindModifiersName(Modifier), Input()->KeyName(KeyID), KeyID);
	}
	else
	{
		int Size = str_length(pStr) + 1;
		m_aapKeyBindings[Modifier][KeyID] = (char *)malloc(Size);
		str_copy(m_aapKeyBindings[Modifier][KeyID], pStr, Size);
		str_format(aBuf, sizeof(aBuf), "bound %s%s (%d) = %s", GetKeyBindModifiersName(Modifier), Input()->KeyName(KeyID), KeyID, m_aapKeyBindings[Modifier][KeyID]);
	}
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
}

int CBinds::GetModifierMask(IInput *i)
{
	int Mask = 0;
	Mask |= i->KeyIsPressed(KEY::LSHIFT) << CBinds::MODIFIER_SHIFT;
	Mask |= i->KeyIsPressed(KEY::RSHIFT) << CBinds::MODIFIER_SHIFT;
	Mask |= i->KeyIsPressed(KEY::LCTRL) << CBinds::MODIFIER_CTRL;
	Mask |= i->KeyIsPressed(KEY::RCTRL) << CBinds::MODIFIER_CTRL;
	Mask |= i->KeyIsPressed(KEY::LALT) << CBinds::MODIFIER_ALT;
	if(!Mask)
		return 1 << CBinds::MODIFIER_NONE;

	return Mask;
}

int CBinds::GetModifierMaskOfKey(int Key)
{
	switch(Key)
	{
	case KEY::LSHIFT:
	case KEY::RSHIFT:
		return 1 << CBinds::MODIFIER_SHIFT;
	case KEY::LCTRL:
	case KEY::RCTRL:
		return 1 << CBinds::MODIFIER_CTRL;
	case KEY::LALT:
		return 1 << CBinds::MODIFIER_ALT;
	default:
		return 0;
	}
}

bool CBinds::ModifierMatchesKey(int Modifier, int Key)
{
	switch(Modifier)
	{
	case MODIFIER_SHIFT:
		return Key == KEY::LSHIFT || Key == KEY::RSHIFT;
	case MODIFIER_CTRL:
		return Key == KEY::LCTRL || Key == KEY::RCTRL;
	case MODIFIER_ALT:
		return Key == KEY::LALT;
	case MODIFIER_NONE:
	default:
		return false;
	}
}

bool CBinds::OnInput(IInput::CEvent e)
{
	// don't handle invalid events
	if(e.m_Key <= 0 || e.m_Key >= KEY::LAST)
		return false;

	int Mask = GetModifierMask(Input());
	int KeyModifierMask = GetModifierMaskOfKey(e.m_Key);
	Mask &= ~KeyModifierMask;
	if(!Mask)
		Mask = 1 << MODIFIER_NONE;

	bool ret = false;
	for(int Mod = 1; Mod < MODIFIER_COMBINATION_COUNT; Mod++)
	{
		if(m_aapKeyBindings[Mod][e.m_Key] && (Mask == Mod))
		{
			if(e.m_Flags & IInput::FLAG_PRESS)
				Console()->ExecuteLineStroked(1, m_aapKeyBindings[Mod][e.m_Key]);
			if(e.m_Flags & IInput::FLAG_RELEASE)
				Console()->ExecuteLineStroked(0, m_aapKeyBindings[Mod][e.m_Key]);
			ret = true;
		}
	}

	if(m_aapKeyBindings[0][e.m_Key] && !ret)
	{
		// When ctrl+shift are pressed (ctrl+shift binds and also the hard-coded ctrl+shift+d, ctrl+shift+g, ctrl+shift+e), ignore other +xxx binds
		if(e.m_Flags & IInput::FLAG_PRESS && Mask != ((1 << MODIFIER_CTRL) | (1 << MODIFIER_SHIFT)))
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
			if(pKeyBinding)
				free(pKeyBinding);
			pKeyBinding = 0;
		}
	}
}

const char *CBinds::Get(int KeyID, int Modifier)
{
	if(KeyID > 0 && KeyID < KEY::LAST && m_aapKeyBindings[Modifier][KeyID])
		return m_aapKeyBindings[Modifier][KeyID];
	return "";
}

void CBinds::GetKey(const char *pBindStr, char *aBuf, unsigned BufSize)
{
	aBuf[0] = 0;
	for(int Mod = 0; Mod < MODIFIER_COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY::LAST; KeyId++)
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
	Bind(KEY::F1, "toggle_local_console");
	Bind(KEY::F2, "toggle_remote_console");
	Bind(KEY::TAB, "+scoreboard");
	Bind(KEY::EQUALS, "+statboard");
	Bind(KEY::F10, "screenshot");

	Bind(KEY::A, "+left");
	Bind(KEY::D, "+right");

	Bind(KEY::SPACE, "+jump");
	Bind(KEY::MOUSE_1, "+fire");
	Bind(KEY::MOUSE_2, "+hook");
	Bind(KEY::LSHIFT, "+emote");
	Bind(KEY::RETURN, "+show_chat; chat all");
	Bind(KEY::RIGHT, "spectate_next");
	Bind(KEY::LEFT, "spectate_previous");
	Bind(KEY::RSHIFT, "+spectate");

	Bind(KEY::ONE, "+weapon1");
	Bind(KEY::TWO, "+weapon2");
	Bind(KEY::THREE, "+weapon3");
	Bind(KEY::FOUR, "+weapon4");
	Bind(KEY::FIVE, "+weapon5");

	Bind(KEY::MOUSE_WHEEL_UP, "+prevweapon");
	Bind(KEY::MOUSE_WHEEL_DOWN, "+nextweapon");

	Bind(KEY::T, "+show_chat; chat all");
	Bind(KEY::Y, "+show_chat; chat team");
	Bind(KEY::U, "+show_chat");
	Bind(KEY::I, "+show_chat; chat all /c ");

	Bind(KEY::F3, "vote yes");
	Bind(KEY::F4, "vote no");

	Bind(KEY::K, "kill");
	Bind(KEY::Q, "say /pause");
	Bind(KEY::P, "say /pause");

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
	Console()->Register("dump_binds", "?s[key]", CFGFLAG_CLIENT, ConDumpBinds, this, "Print command executed by this keybindind or all binds");
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

void CBinds::ConDumpBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		char aBuf[256];
		const char *pKeyName = pResult->GetString(0);

		int Modifier;
		int id = pBinds->GetBindSlot(pKeyName, &Modifier);
		if(!id)
		{
			str_format(aBuf, sizeof(aBuf), "key '%s' not found", pKeyName);
			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		}
		else
		{
			if(!pBinds->m_aapKeyBindings[Modifier][id])
				str_format(aBuf, sizeof(aBuf), "%s (%d) is not bound", pKeyName, id);
			else
				str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", pKeyName, id, pBinds->m_aapKeyBindings[Modifier][id]);

			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		}
	}
	else if(pResult->NumArguments() == 0)
	{
		char aBuf[1024];
		for(int i = 0; i < MODIFIER_COMBINATION_COUNT; i++)
		{
			for(int j = 0; j < KEY::LAST; j++)
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
	int id = pBinds->GetBindSlot(pKeyName, &Modifier);

	if(!id)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "key %s not found", pKeyName);
		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		return;
	}

	pBinds->Bind(id, "", false, Modifier);
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
		if(i > 0 && i < KEY::LAST)
			return i; // numeric
	}

	// search for key
	for(int i = 0; i < KEY::LAST; i++)
	{
		if(str_comp(pKeyName, Input()->KeyName(i)) == 0)
			return i;
	}

	return 0;
}

int CBinds::GetBindSlot(const char *pBindString, int *Mod)
{
	*Mod = MODIFIER_NONE;
	char aMod[32];
	aMod[0] = '\0';
	const char *pKey = str_next_token(pBindString, "+", aMod, sizeof(aMod));
	while(aMod[0] && *(pKey))
	{
		if(!str_comp(aMod, "shift"))
			*Mod |= (1 << MODIFIER_SHIFT);
		else if(!str_comp(aMod, "ctrl"))
			*Mod |= (1 << MODIFIER_CTRL);
		else if(!str_comp(aMod, "alt"))
			*Mod |= (1 << MODIFIER_ALT);
		else
			return 0;

		if(str_find(pKey + 1, "+"))
			pKey = str_next_token(pKey + 1, "+", aMod, sizeof(aMod));
		else
			break;
	}
	return GetKeyID(*Mod == MODIFIER_NONE ? aMod : pKey + 1);
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
	case MODIFIER_NONE:
	default:
		return "";
	}
}

const char *CBinds::GetKeyBindModifiersName(int Modifier)
{
	static char aModifier[256];
	aModifier[0] = '\0';
	for(int k = 1; k < MODIFIER_COUNT; k++)
	{
		if(Modifier & (1 << k))
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

	char aBuffer[256];
	char *pEnd = aBuffer + sizeof(aBuffer);
	pConfigManager->WriteLine("unbindall");
	for(int i = 0; i < MODIFIER_COMBINATION_COUNT; i++)
	{
		for(int j = 0; j < KEY::LAST; j++)
		{
			if(!pSelf->m_aapKeyBindings[i][j])
				continue;

			str_format(aBuffer, sizeof(aBuffer), "bind %s%s \"", GetKeyBindModifiersName(i), pSelf->Input()->KeyName(j));
			// process the string. we need to escape some characters
			char *pDst = aBuffer + str_length(aBuffer);
			str_escape(&pDst, pSelf->m_aapKeyBindings[i][j], pEnd);
			str_append(aBuffer, "\"", sizeof(aBuffer));

			pConfigManager->WriteLine(aBuffer);
		}
	}
}

// DDRace

void CBinds::SetDDRaceBinds(bool FreeOnly)
{
	if(g_Config.m_ClDDRaceBindsSet < 1)
	{
		Bind(KEY::KP_PLUS, "zoom+", FreeOnly);
		Bind(KEY::KP_MINUS, "zoom-", FreeOnly);
		Bind(KEY::KP_MULTIPLY, "zoom", FreeOnly);
		Bind(KEY::PAUSE, "say /pause", FreeOnly);
		Bind(KEY::UP, "+jump", FreeOnly);
		Bind(KEY::LEFT, "+left", FreeOnly);
		Bind(KEY::RIGHT, "+right", FreeOnly);
		Bind(KEY::LEFTBRACKET, "+prevweapon", FreeOnly);
		Bind(KEY::RIGHTBRACKET, "+nextweapon", FreeOnly);
		Bind(KEY::C, "say /rank", FreeOnly);
		Bind(KEY::V, "say /info", FreeOnly);
		Bind(KEY::B, "say /top5", FreeOnly);
		Bind(KEY::S, "+showhookcoll", FreeOnly);
		Bind(KEY::X, "toggle cl_dummy 0 1", FreeOnly);
		Bind(KEY::H, "toggle cl_dummy_hammer 0 1", FreeOnly);
		Bind(KEY::SLASH, "+show_chat; chat all /", FreeOnly);
		Bind(KEY::PAGEDOWN, "toggle cl_show_quads 0 1", FreeOnly);
		Bind(KEY::PAGEUP, "toggle cl_overlay_entities 0 100", FreeOnly);
		Bind(KEY::KP_0, "say /emote normal 999999", FreeOnly);
		Bind(KEY::KP_1, "say /emote happy 999999", FreeOnly);
		Bind(KEY::KP_2, "say /emote angry 999999", FreeOnly);
		Bind(KEY::KP_3, "say /emote pain 999999", FreeOnly);
		Bind(KEY::KP_4, "say /emote surprise 999999", FreeOnly);
		Bind(KEY::KP_5, "say /emote blink 999999", FreeOnly);
		Bind(KEY::MOUSE_3, "+spectate", FreeOnly);
		Bind(KEY::MINUS, "spectate_previous", FreeOnly);
		Bind(KEY::EQUALS, "spectate_next", FreeOnly);
	}

	g_Config.m_ClDDRaceBindsSet = 1;
}
