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

CBinds::CBindSlot::CBindSlot(int KeyId, int ModifierMask) :
	m_Key(KeyId),
	m_ModifierMask(ModifierMask)
{
	dbg_assert(KeyId >= KEY_FIRST && KeyId < KEY_LAST, "KeyId invalid");
	dbg_assert(ModifierMask >= MODIFIER_NONE && ModifierMask < MODIFIER_COMBINATION_COUNT, "ModifierMask invalid");
}

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
	m_SpecialBinds.m_pBinds = this;
}

CBinds::~CBinds()
{
	UnbindAll();
}

void CBinds::Bind(int KeyId, const char *pStr, bool FreeOnly, int ModifierCombination)
{
	CBindSlot BindSlot(KeyId, ModifierCombination);
	char *pBind = Get(BindSlot);

	// FreeOnly means only bind if the slot is free
	if(FreeOnly && pBind)
		return;

	char aBuf[256];
	char aName[128];
	GetName(KeyId, ModifierCombination, aName, sizeof(aName));
	if(!pStr[0])
	{
		free(pBind); // Free the slot
		str_format(aBuf, sizeof(aBuf), "unbound %s (%d)", aName, Input()->KeyName(KeyId), KeyId);
		m_Binds.erase(BindSlot);
	}
	else
	{
		int Size = str_length(pStr) + 1;
		pBind = (char *)realloc(pBind, Size); // Resize the slot
		str_copy(pBind, pStr, Size);
		str_format(aBuf, sizeof(aBuf), "bound %s (%d) = %s", aName, KeyId, pBind);
		m_Binds[BindSlot] = pBind;
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
	const CBindSlot BindSlot(Event.m_Key, GetModifierMask(Input()) & ~KeyModifierMask);

	bool Handled = false;

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		auto ActiveBind = std::find_if(m_vActiveBinds.begin(), m_vActiveBinds.end(), [&](const CBindSlot &Bind) {
			return Event.m_Key == Bind.m_Key;
		});
		if(ActiveBind == m_vActiveBinds.end())
		{
			const auto &&OnKeyPress = [&](int Mask) {
				const char *pBind = Get(BindSlot.m_Key, Mask);
				if(pBind)
				{
					if(g_Config.m_ClSubTickAiming)
					{
						if(str_comp("+fire", pBind) == 0 || str_comp("+hook", pBind) == 0)
						{
							m_MouseOnAction = true;
						}
					}
					Console()->ExecuteLineStroked(1, pBind);
					m_vActiveBinds.emplace_back(BindSlot.m_Key, Mask);
				}
			};

			if(Get(BindSlot))
			{
				OnKeyPress(BindSlot.m_ModifierMask);
				Handled = true;
			}
			else if(Get(BindSlot.m_Key, MODIFIER_NONE) &&
				BindSlot.m_ModifierMask != ((1 << MODIFIER_CTRL) | (1 << MODIFIER_SHIFT)) &&
				BindSlot.m_ModifierMask != ((1 << MODIFIER_GUI) | (1 << MODIFIER_SHIFT)))
			{
				OnKeyPress(MODIFIER_NONE);
				Handled = true;
			}
		}
		else
		{
			// Repeat active bind while key is held down
			// Have to check for nullptr again because the previous execute can unbind itself
			const char *pBind = Get(ActiveBind->m_Key, ActiveBind->m_ModifierMask);
			if(pBind)
				Console()->ExecuteLineStroked(1, pBind);
			Handled = true;
		}
	}

	if(Event.m_Flags & IInput::FLAG_RELEASE)
	{
		const auto &&OnKeyRelease = [&](const CBindSlot &Bind) {
			// Prevent binds from being deactivated while chat, console and menus are open, as these components will
			// still allow key release events to be forwarded to this component, so the active binds can be cleared.
			if(GameClient()->m_Chat.IsActive() ||
				!GameClient()->m_GameConsole.IsClosed() ||
				GameClient()->m_Menus.IsActive())
			{
				return;
			}
			// Have to check for nullptr again because the previous execute can unbind itself
			const char *pBind = Get(Bind);
			if(pBind)
				Console()->ExecuteLineStroked(0, pBind);
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
		if(BindSlot.m_ModifierMask != MODIFIER_NONE)
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
	for(auto &it : m_Binds)
		free(it.second);
	m_Binds.clear();
}

char *CBinds::Get(CBindSlot BindSlot)
{
	if(auto it = m_Binds.find(BindSlot); it != m_Binds.end())
		return it->second;
	return nullptr;
}

void CBinds::GetKey(const char *pBindStr, char *pBuf, size_t BufSize)
{
	pBuf[0] = '\0';
	for(int Modifier = MODIFIER_NONE; Modifier < MODIFIER_COMBINATION_COUNT; Modifier++)
	{
		for(int KeyId = KEY_FIRST; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = Get(KeyId, Modifier);
			if(!pBind)
				continue;

			if(str_comp(pBind, pBindStr) == 0)
			{
				GetName(KeyId, Modifier, pBuf, BufSize);
				return;
			}
		}
	}
}

size_t CBinds::GetName(int KeyId, int ModifierMask, char *pBuf, size_t BufSize)
{
	dbg_assert(KeyId >= KEY_FIRST && KeyId < KEY_LAST, "KeyId invalid");
	dbg_assert(ModifierMask >= MODIFIER_NONE && ModifierMask < MODIFIER_COMBINATION_COUNT, "ModifierMask invalid");
	const size_t ModifiersNameLength = GetKeyBindModifiersName(ModifierMask, pBuf, BufSize);
	if(ModifiersNameLength >= BufSize)
		return BufSize;
	str_append(pBuf + ModifiersNameLength, Input()->KeyName(KeyId), BufSize - ModifiersNameLength);
	return str_length(pBuf);
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
	if(pResult->NumArguments() == 1)
	{
		ConBinds(pResult, pUserData);
		return;
	}

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
			const char *pBind = pBinds->Get(BindSlot);
			if(pBind)
				str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", pKeyName, BindSlot.m_Key, pBind);
			else
				str_format(aBuf, sizeof(aBuf), "%s (%d) is not bound", pKeyName, BindSlot.m_Key);
			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
		}
	}
	else
	{
		char aBuf[1024];
		char aName[128];
		for(auto &it : pBinds->m_Binds)
		{
			const CBindSlot BindSlot = it.first;
			const char *pBind = it.second;
			pBinds->GetName(BindSlot.m_Key, BindSlot.m_ModifierMask, aName, sizeof(aName));
			str_format(aBuf, sizeof(aBuf), "%s (%d) = %s", aName, BindSlot.m_Key, pBind);
			pBinds->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", aBuf, gs_BindPrintColor);
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

size_t CBinds::GetKeyBindModifiersName(int ModifierCombination, char *pBuf, size_t BufSize)
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
	return str_length(pBuf);
}

void CBinds::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBinds *pSelf = (CBinds *)pUserData;

	pConfigManager->WriteLine("unbindall");
	for(auto &it : pSelf->m_Binds)
	{
		const CBindSlot BindSlot = it.first;
		const char *pBind = it.second;
		char pBuf[1024] = "bind ";
		const char *pBufEnd = pBuf + sizeof(pBuf);
		pSelf->GetName(BindSlot.m_Key, BindSlot.m_ModifierMask, pBuf + str_length(pBuf), sizeof(pBuf) - str_length(pBuf));
		str_append(pBuf, " \"", sizeof(pBuf));
		// process the string. we need to escape some characters
		char *pDst = pBuf + str_length(pBuf);
		str_escape(&pDst, pBind, pBufEnd);
		str_append(pBuf, "\"", sizeof(pBuf));
		pConfigManager->WriteLine(pBuf);
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
