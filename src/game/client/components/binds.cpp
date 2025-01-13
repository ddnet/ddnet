/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "binds.h"
#include <base/system.h>
#include <engine/config.h>
#include <engine/shared/config.h>

#include <game/client/components/chat.h>
#include <game/client/components/console.h>
#include <game/client/gameclient.h>

static const ColorRGBA gs_BindPrintColor{1.0f, 1.0f, 0.8f, 1.0f};

bool CBinds::CBindsSpecial::OnInput(const IInput::CEvent &Event)
{
	if((Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_RELEASE)) == 0)
		return false;

	// only handle F and composed F binds
	// do not handle F5 bind while menu is active
	if(((Event.m_Key >= KEY_F1 && Event.m_Key <= KEY_F12) || (Event.m_Key >= KEY_F13 && Event.m_Key <= KEY_F24)) &&
		(Event.m_Key != KEY_F5 || !m_pClient->m_Menus.IsActive()))
	{
		return m_pBinds->OnInput(Event);
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
	UnbindAll();
}

void CBinds::Bind(int KeyId, const char *pStr, bool FreeOnly, int ModifierCombination)
{
	dbg_assert(KeyId >= KEY_FIRST && KeyId < KEY_LAST, "KeyId invalid");
	dbg_assert(ModifierCombination >= MODIFIER_NONE && ModifierCombination < MODIFIER_COMBINATION_COUNT, "ModifierCombination invalid");

	if(FreeOnly && Get(KeyId, ModifierCombination)[0])
		return;

	free(m_aapKeyBindings[ModifierCombination][KeyId]);
	m_aapKeyBindings[ModifierCombination][KeyId] = nullptr;

	char aBuf[256];
	char aModifiers[128];
	GetKeyBindModifiersName(ModifierCombination, aModifiers, sizeof(aModifiers));
	if(!pStr[0])
	{
		str_format(aBuf, sizeof(aBuf), "unbound %s%s (%d)", aModifiers, Input()->KeyName(KeyId), KeyId);
	}
	else
	{
		int Size = str_length(pStr) + 1;
		m_aapKeyBindings[ModifierCombination][KeyId] = (char *)malloc(Size);
		str_copy(m_aapKeyBindings[ModifierCombination][KeyId], pStr, Size);
		str_format(aBuf, sizeof(aBuf), "bound %s%s (%d) = %s", aModifiers, Input()->KeyName(KeyId), KeyId, m_aapKeyBindings[ModifierCombination][KeyId]);
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
		return CBinds::MODIFIER_NONE;
	}
}

bool CBinds::OnInput(const IInput::CEvent &Event)
{
	if((Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_RELEASE)) == 0)
		return false;

	const int KeyModifierMask = GetModifierMaskOfKey(Event.m_Key);
	const int ModifierMask = GetModifierMask(Input()) & ~KeyModifierMask;

	bool Handled = false;

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		auto ActiveBind = std::find_if(m_vActiveBinds.begin(), m_vActiveBinds.end(), [&](const CBindSlot &Bind) {
			return Event.m_Key == Bind.m_Key;
		});
		if(ActiveBind == m_vActiveBinds.end())
		{
			const auto &&OnKeyPress = [&](int Mask) {
				const char *pBind = m_aapKeyBindings[Mask][Event.m_Key];
				if(g_Config.m_ClSubTickAiming)
				{
					if(str_comp("+fire", pBind) == 0 || str_comp("+hook", pBind) == 0)
					{
						m_MouseOnAction = true;
					}
				}
				Console()->ExecuteLineStroked(1, pBind);
				m_vActiveBinds.emplace_back(Event.m_Key, Mask);
			};

			if(m_aapKeyBindings[ModifierMask][Event.m_Key])
			{
				OnKeyPress(ModifierMask);
				Handled = true;
			}
			else if(m_aapKeyBindings[MODIFIER_NONE][Event.m_Key] &&
				ModifierMask != ((1 << MODIFIER_CTRL) | (1 << MODIFIER_SHIFT)) &&
				ModifierMask != ((1 << MODIFIER_GUI) | (1 << MODIFIER_SHIFT)))
			{
				OnKeyPress(MODIFIER_NONE);
				Handled = true;
			}
		}
		else
		{
			// Repeat active bind while key is held down
			// Have to check for nullptr again because the previous execute can unbind itself
			if(m_aapKeyBindings[ActiveBind->m_ModifierMask][ActiveBind->m_Key])
			{
				Console()->ExecuteLineStroked(1, m_aapKeyBindings[ActiveBind->m_ModifierMask][ActiveBind->m_Key]);
			}
			Handled = true;
		}
	}

	if(Event.m_Flags & IInput::FLAG_RELEASE)
	{
		const auto &&OnKeyRelease = [&](const CBindSlot &Bind) {
			// Prevent binds from being deactivated while chat, console and menus are open, as these components will
			// still allow key release events to be forwarded to this component, so the active binds can be cleared.
			if(GameClient()->m_Chat.IsActive() ||
				GameClient()->m_GameConsole.IsActive() ||
				GameClient()->m_Menus.IsActive())
			{
				return;
			}
			// Have to check for nullptr again because the previous execute can unbind itself
			if(!m_aapKeyBindings[Bind.m_ModifierMask][Bind.m_Key])
			{
				return;
			}
			Console()->ExecuteLineStroked(0, m_aapKeyBindings[Bind.m_ModifierMask][Bind.m_Key]);
		};

		// Release active bind that uses this primary key
		auto ActiveBind = std::find_if(m_vActiveBinds.begin(), m_vActiveBinds.end(), [&](const CBindSlot &Bind) {
			return Event.m_Key == Bind.m_Key;
		});
		if(ActiveBind != m_vActiveBinds.end())
		{
			OnKeyRelease(*ActiveBind);
			m_vActiveBinds.erase(ActiveBind);
			Handled = true;
		}

		// Release all active binds that use this modifier key
		if(KeyModifierMask != MODIFIER_NONE)
		{
			while(true)
			{
				auto ActiveModifierBind = std::find_if(m_vActiveBinds.begin(), m_vActiveBinds.end(), [&](const CBindSlot &Bind) {
					return (Bind.m_ModifierMask & KeyModifierMask) != 0;
				});
				if(ActiveModifierBind == m_vActiveBinds.end())
					break;
				OnKeyRelease(*ActiveModifierBind);
				m_vActiveBinds.erase(ActiveModifierBind);
				Handled = true;
			}
		}
	}

	return Handled;
}

void CBinds::UnbindAll()
{
	for(auto &apKeyBinding : m_aapKeyBindings)
	{
		for(auto &pKeyBinding : apKeyBinding)
		{
			free(pKeyBinding);
			pKeyBinding = nullptr;
		}
	}
}

const char *CBinds::Get(int KeyId, int ModifierCombination)
{
	dbg_assert(KeyId >= KEY_FIRST && KeyId < KEY_LAST, "KeyId invalid");
	dbg_assert(ModifierCombination >= MODIFIER_NONE && ModifierCombination < MODIFIER_COMBINATION_COUNT, "ModifierCombination invalid");
	return m_aapKeyBindings[ModifierCombination][KeyId] ? m_aapKeyBindings[ModifierCombination][KeyId] : "";
}

void CBinds::GetKey(const char *pBindStr, char *pBuf, size_t BufSize)
{
	pBuf[0] = '\0';
	for(int Modifier = MODIFIER_NONE; Modifier < MODIFIER_COMBINATION_COUNT; Modifier++)
	{
		char aModifiers[128];
		GetKeyBindModifiersName(Modifier, aModifiers, sizeof(aModifiers));
		for(int KeyId = KEY_FIRST; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = Get(KeyId, Modifier);
			if(!pBind[0])
				continue;

			if(str_comp(pBind, pBindStr) == 0)
			{
				str_format(pBuf, BufSize, "%s%s", aModifiers, Input()->KeyName(KeyId));
				return;
			}
		}
	}
}

void CBinds::SetDefaults()
{
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
	Bind(KEY_Q, "say /spec");
	Bind(KEY_P, "say /pause");

	g_Config.m_ClDDRaceBindsSet = 0;
	SetDDRaceBinds(false);
}

void CBinds::OnConsoleInit()
{
	ConfigManager()->RegisterCallback(ConfigSaveCallback, this);

	Console()->Register("bind", "s[key] ?r[command]", CFGFLAG_CLIENT, ConBind, this, "Bind key to execute a command or view keybindings");
	Console()->Register("binds", "?s[key]", CFGFLAG_CLIENT, ConBinds, this, "Print command executed by this keybinding or all binds");
	Console()->Register("unbind", "s[key]", CFGFLAG_CLIENT, ConUnbind, this, "Unbind key");
	Console()->Register("unbindall", "", CFGFLAG_CLIENT, ConUnbindAll, this, "Unbind all keys");

	SetDefaults();
}

void CBinds::ConBind(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	const char *pBindStr = pResult->GetString(0);
	const CBindSlot BindSlot = pBinds->GetBindSlot(pBindStr);

	if(!BindSlot.m_Key)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "key %s not found", pBindStr);
		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		return;
	}

	if(pResult->NumArguments() == 1)
	{
		char aBuf[256];
		const char *pKeyName = pResult->GetString(0);

		if(!pBinds->m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key])
			str_format(aBuf, sizeof(aBuf), "%s (%d) is not bound", pKeyName, BindSlot.m_Key);
		else
			str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", pKeyName, BindSlot.m_Key, pBinds->m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key]);

		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		return;
	}

	pBinds->Bind(BindSlot.m_Key, pResult->GetString(1), false, BindSlot.m_ModifierMask);
}

void CBinds::ConBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		char aBuf[256];
		const char *pKeyName = pResult->GetString(0);
		const CBindSlot BindSlot = pBinds->GetBindSlot(pKeyName);
		if(!BindSlot.m_Key)
		{
			str_format(aBuf, sizeof(aBuf), "key '%s' not found", pKeyName);
			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		}
		else
		{
			if(!pBinds->m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key])
				str_format(aBuf, sizeof(aBuf), "%s (%d) is not bound", pKeyName, BindSlot.m_Key);
			else
				str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", pKeyName, BindSlot.m_Key, pBinds->m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key]);

			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		}
	}
	else
	{
		char aBuf[1024];
		for(int Modifier = MODIFIER_NONE; Modifier < MODIFIER_COMBINATION_COUNT; Modifier++)
		{
			char aModifiers[128];
			GetKeyBindModifiersName(Modifier, aModifiers, sizeof(aModifiers));
			for(int Key = KEY_FIRST; Key < KEY_LAST; Key++)
			{
				if(!pBinds->m_aapKeyBindings[Modifier][Key])
					continue;

				str_format(aBuf, sizeof(aBuf), "%s%s (%d) = %s", aModifiers, pBinds->Input()->KeyName(Key), Key, pBinds->m_aapKeyBindings[Modifier][Key]);
				pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
			}
		}
	}
}

void CBinds::ConUnbind(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	const char *pKeyName = pResult->GetString(0);
	const CBindSlot BindSlot = pBinds->GetBindSlot(pKeyName);

	if(!BindSlot.m_Key)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "key %s not found", pKeyName);
		pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		return;
	}

	pBinds->Bind(BindSlot.m_Key, "", false, BindSlot.m_ModifierMask);
}

void CBinds::ConUnbindAll(IConsole::IResult *pResult, void *pUserData)
{
	CBinds *pBinds = (CBinds *)pUserData;
	pBinds->UnbindAll();
}

CBinds::CBindSlot CBinds::GetBindSlot(const char *pBindString) const
{
	int ModifierMask = MODIFIER_NONE;
	char aMod[32];
	aMod[0] = '\0';
	const char *pKey = str_next_token(pBindString, "+", aMod, sizeof(aMod));
	while(aMod[0] && *(pKey))
	{
		if(!str_comp_nocase(aMod, "shift"))
			ModifierMask |= (1 << MODIFIER_SHIFT);
		else if(!str_comp_nocase(aMod, "ctrl"))
			ModifierMask |= (1 << MODIFIER_CTRL);
		else if(!str_comp_nocase(aMod, "alt"))
			ModifierMask |= (1 << MODIFIER_ALT);
		else if(!str_comp_nocase(aMod, "gui"))
			ModifierMask |= (1 << MODIFIER_GUI);
		else
			return {KEY_UNKNOWN, MODIFIER_NONE};

		if(str_find(pKey + 1, "+"))
			pKey = str_next_token(pKey + 1, "+", aMod, sizeof(aMod));
		else
			break;
	}
	return {Input()->FindKeyByName(ModifierMask == MODIFIER_NONE ? aMod : pKey + 1), ModifierMask};
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

void CBinds::GetKeyBindModifiersName(int ModifierCombination, char *pBuf, size_t BufSize)
{
	pBuf[0] = '\0';
	for(int k = 1; k < MODIFIER_COUNT; k++)
	{
		if(ModifierCombination & (1 << k))
		{
			str_append(pBuf, GetModifierName(k), BufSize);
			str_append(pBuf, "+", BufSize);
		}
	}
}

void CBinds::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBinds *pSelf = (CBinds *)pUserData;

	pConfigManager->WriteLine("unbindall");
	for(int Modifier = MODIFIER_NONE; Modifier < MODIFIER_COMBINATION_COUNT; Modifier++)
	{
		char aModifiers[128];
		GetKeyBindModifiersName(Modifier, aModifiers, sizeof(aModifiers));
		for(int Key = KEY_FIRST; Key < KEY_LAST; Key++)
		{
			if(!pSelf->m_aapKeyBindings[Modifier][Key])
				continue;

			// worst case the str_escape can double the string length
			int Size = str_length(pSelf->m_aapKeyBindings[Modifier][Key]) * 2 + 30;
			char *pBuffer = (char *)malloc(Size);
			char *pEnd = pBuffer + Size;

			str_format(pBuffer, Size, "bind %s%s \"", aModifiers, pSelf->Input()->KeyName(Key));
			// process the string. we need to escape some characters
			char *pDst = pBuffer + str_length(pBuffer);
			str_escape(&pDst, pSelf->m_aapKeyBindings[Modifier][Key], pEnd);
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
