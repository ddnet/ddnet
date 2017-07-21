/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/config.h>
#include <engine/shared/config.h>
#include "binds.h"

bool CBinds::CBindsSpecial::OnInput(IInput::CEvent Event)
{
	// don't handle invalid events and keys that arn't set to anything
	if(((Event.m_Key >= KEY_F1 && Event.m_Key <= KEY_F12) || (Event.m_Key >= KEY_F13 && Event.m_Key <= KEY_F24)) && m_pBinds->m_apKeyBindings[Event.m_Key])
	{
		int Stroke = 0;
		if(Event.m_Flags&IInput::FLAG_PRESS)
			Stroke = 1;

		m_pBinds->GetConsole()->ExecuteLineStroked(Stroke, m_pBinds->m_apKeyBindings[Event.m_Key]);
		return true;
	}

	return false;
}

CBinds::CBinds()
{
	mem_zero(m_apKeyBindings, sizeof(m_apKeyBindings));
	m_SpecialBinds.m_pBinds = this;
}

CBinds::~CBinds()
{
	for(int i = 0; i < KEY_LAST; i++)
		if(m_apKeyBindings[i])
			mem_free(m_apKeyBindings[i]);
}

void CBinds::Bind(int KeyID, const char *pStr, bool FreeOnly)
{
	if(KeyID < 0 || KeyID >= KEY_LAST)
		return;

	if(FreeOnly && Get(KeyID)[0])
		return;

	if(m_apKeyBindings[KeyID])
	{
		mem_free(m_apKeyBindings[KeyID]);
		m_apKeyBindings[KeyID] = 0;
	}

	char aBuf[256];
	if(!pStr[0])
	{
		str_format(aBuf, sizeof(aBuf), "unbound %s (%d)", Input()->KeyName(KeyID), KeyID);
	}
	else
	{
		int size = str_length(pStr) + 1;
		m_apKeyBindings[KeyID] = (char *)mem_alloc(size, 1);
		if(!m_apKeyBindings[KeyID])
		{
			str_format(aBuf, sizeof(aBuf), "couldn't bind %s (%d) (bind might be too long)", Input()->KeyName(KeyID), KeyID);
		}
		else
		{
			str_copy(m_apKeyBindings[KeyID], pStr, size);
			str_format(aBuf, sizeof(aBuf), "bound %s (%d) = %s", Input()->KeyName(KeyID), KeyID, m_apKeyBindings[KeyID]);
		}
	}
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf);
}

bool CBinds::OnInput(IInput::CEvent e)
{
	// don't handle invalid events and keys that arn't set to anything
	if(e.m_Key <= 0 || e.m_Key >= KEY_LAST || !m_apKeyBindings[e.m_Key])
		return false;

	if(e.m_Flags&IInput::FLAG_PRESS)
		Console()->ExecuteLineStroked(1, m_apKeyBindings[e.m_Key]);
	if(e.m_Flags&IInput::FLAG_RELEASE)
		Console()->ExecuteLineStroked(0, m_apKeyBindings[e.m_Key]);
	return true;
}

void CBinds::UnbindAll()
{
	for(int i = 0; i < KEY_LAST; i++)
	{
		if(m_apKeyBindings[i])
			mem_free(m_apKeyBindings[i]);
		m_apKeyBindings[i] = 0;
	}
}

const char *CBinds::Get(int KeyID)
{
	if(KeyID > 0 && KeyID < KEY_LAST && m_apKeyBindings[KeyID])
		return m_apKeyBindings[KeyID];
	return "";
}

const char *CBinds::GetKey(const char *pBindStr)
{
	for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
	{
		const char *pBind = Get(KeyId);
		if(!pBind[0])
			continue;

		if(str_comp(pBind, pBindStr) == 0)
			return Input()->KeyName(KeyId);
	}

	return "";
}

void CBinds::SetDefaults()
{
	// set default key bindings
	UnbindAll();
	Bind(KEY_F1, "toggle_local_console");
	Bind(KEY_F2, "toggle_remote_console");
	Bind(KEY_TAB, "+scoreboard");
	Bind(KEY_BACKQUOTE, "+statboard");
	Bind(KEY_F10, "screenshot");

	Bind(KEY_A, "+left");
	Bind(KEY_D, "+right");

	Bind(KEY_SPACE, "+jump");
	Bind(KEY_MOUSE_1, "+fire");
	Bind(KEY_MOUSE_2, "+hook");
	Bind(KEY_LSHIFT, "+emote");
#if defined(__ANDROID__)
	Bind(KEY_RCTRL, "+fire");
	Bind(KEY_RETURN, "+hook");
	Bind(KEY_RIGHT, "+right");
	Bind(KEY_LEFT, "+left");
	Bind(KEY_UP, "+jump");
	Bind(KEY_DOWN, "+hook");
	Bind(KEY_PAGEUP, "+prevweapon");
	Bind(KEY_PAGEDOWN, "+nextweapon");
	Bind(KEY_F5, "spectate_previous");
	Bind(KEY_F6, "spectate_next");
#else
	Bind(KEY_RETURN, "+show_chat; chat all");
	Bind(KEY_RIGHT, "spectate_next");
	Bind(KEY_LEFT, "spectate_previous");
	Bind(KEY_RSHIFT, "+spectate");
#endif


	Bind(KEY_1, "+weapon1");
	Bind(KEY_2, "+weapon2");
	Bind(KEY_3, "+weapon3");
	Bind(KEY_4, "+weapon4");
	Bind(KEY_5, "+weapon5");

	Bind(KEY_MOUSE_WHEEL_UP, "+prevweapon");
	Bind(KEY_MOUSE_WHEEL_DOWN, "+nextweapon");

	Bind(KEY_T, "+show_chat; chat all");
	Bind(KEY_Y, "+show_chat; chat team");
	Bind(KEY_Z, "+show_chat; chat team"); // For German keyboards
	Bind(KEY_U, "+show_chat");
	Bind(KEY_I, "+show_chat; chat all /c ");

	Bind(KEY_F3, "vote yes");
	Bind(KEY_F4, "vote no");

	Bind(KEY_K, "kill");
	Bind(KEY_Q, "say /pause");
	Bind(KEY_P, "say /pause");

	// DDRace

	if(g_Config.m_ClDDRaceBinds)
		SetDDRaceBinds(false);
}

void CBinds::OnConsoleInit()
{
	// bindings
	IConfig *pConfig = Kernel()->RequestInterface<IConfig>();
	if(pConfig)
		pConfig->RegisterCallback(ConfigSaveCallback, this);

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
	const char *pKeyName = pResult->GetString(0);
	int id = pBinds->GetKeyID(pKeyName);

	if(!id)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "key %s not found", pKeyName);
		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf);
		return;
	}

	pBinds->Bind(id, pResult->GetString(1));
}

void CBinds::ConDumpBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		char aBuf[256];
		const char *pKeyName = pResult->GetString(0);

		int id = pBinds->GetKeyID(pKeyName);
		if (!id)
		{
			str_format(aBuf, sizeof(aBuf), "key '%s' not found", pKeyName);
			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf);
		}
		else
		{
			if (!pBinds->m_apKeyBindings[id])
				str_format(aBuf, sizeof(aBuf), "%s (%d) is not bound", pKeyName, id);
			else
				str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", pKeyName, id, pBinds->m_apKeyBindings[id]);

			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf);
		}
	}
	else if(pResult->NumArguments() == 0)
	{
		char aBuf[1024];
		for(int i = 0; i < KEY_LAST; i++)
		{
			if(!pBinds->m_apKeyBindings[i])
				continue;
			str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", pBinds->Input()->KeyName(i), i, pBinds->m_apKeyBindings[i]);
			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf);
		}
	}
}

void CBinds::ConUnbind(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	const char *pKeyName = pResult->GetString(0);
	int id = pBinds->GetKeyID(pKeyName);

	if(!id)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "key %s not found", pKeyName);
		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf);
		return;
	}

	pBinds->Bind(id, "");
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
		int i = str_toint(pKeyName+1);
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

void CBinds::ConfigSaveCallback(IConfig *pConfig, void *pUserData)
{
	CBinds *pSelf = (CBinds *)pUserData;

	char aBuffer[256];
	char *pEnd = aBuffer+sizeof(aBuffer);
	pConfig->WriteLine("unbindall");
	for(int i = 0; i < KEY_LAST; i++)
	{
		if(!pSelf->m_apKeyBindings[i])
			continue;
		str_format(aBuffer, sizeof(aBuffer), "bind %s \"", pSelf->Input()->KeyName(i));

		// process the string. we need to escape some characters
		char *pDst = aBuffer + str_length(aBuffer);
		str_escape(&pDst, pSelf->m_apKeyBindings[i], pEnd);
		str_append(aBuffer, "\"", sizeof(aBuffer));

		pConfig->WriteLine(aBuffer);
	}
}

// DDRace

void CBinds::SetDDRaceBinds(bool FreeOnly)
{
	Bind(KEY_KP_PLUS, "zoom+", FreeOnly);
	Bind(KEY_KP_MINUS, "zoom-", FreeOnly);
	Bind(KEY_KP_MULTIPLY, "zoom", FreeOnly);
	Bind(KEY_HOME, "kill", FreeOnly);
	Bind(KEY_PAUSE, "say /pause", FreeOnly);
	Bind(KEY_UP, "+jump", FreeOnly);
	Bind(KEY_LEFT, "+left", FreeOnly);
	Bind(KEY_RIGHT, "+right", FreeOnly);
	Bind(KEY_LEFTBRACKET, "+prevweapon", FreeOnly);
	Bind(KEY_RIGHTBRACKET, "+nextweapon", FreeOnly);
	Bind(KEY_C, "say /rank", FreeOnly);
	Bind(KEY_V, "say /info", FreeOnly);
	Bind(KEY_B, "say /top5", FreeOnly);
	Bind(KEY_X, "emote 14", FreeOnly);
	Bind(KEY_H, "emote 2", FreeOnly);
	Bind(KEY_M, "emote 5", FreeOnly);
	Bind(KEY_S, "+showhookcoll", FreeOnly);
	Bind(KEY_X, "toggle cl_dummy 0 1", FreeOnly);
#if !defined(__ANDROID__)
	Bind(KEY_PAGEDOWN, "toggle cl_show_quads 0 1", FreeOnly);
	Bind(KEY_PAGEUP, "toggle cl_overlay_entities 0 100", FreeOnly);
#endif
	Bind(KEY_KP_0, "say /emote normal 999999", FreeOnly);
	Bind(KEY_KP_1, "say /emote happy 999999", FreeOnly);
	Bind(KEY_KP_2, "say /emote angry 999999", FreeOnly);
	Bind(KEY_KP_3, "say /emote pain 999999", FreeOnly);
	Bind(KEY_KP_4, "say /emote surprise 999999", FreeOnly);
	Bind(KEY_KP_5, "say /emote blink 999999", FreeOnly);
	Bind(KEY_MOUSE_3, "+spectate", FreeOnly);
	Bind(KEY_MINUS, "spectate_previous", FreeOnly);
	Bind(KEY_EQUALS, "spectate_next", FreeOnly);

	g_Config.m_ClDDRaceBindsSet = 1;
}
