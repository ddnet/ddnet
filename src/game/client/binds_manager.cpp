#include "binds_manager.h"

#include <game/client/gameclient.h>

#include <base/log.h>

static const ColorRGBA gs_BindPrintColor{1.0f, 1.0f, 0.8f, 1.0f};

bool CBindsManager::CBindsSpecial::OnInput(const IInput::CEvent &Event)
{
	if(m_pBindsManager->m_aActiveBindGroup[0] == '\0')
		return false;

	return m_pBindsManager->Binds(m_pBindsManager->m_aActiveBindGroup)->m_SpecialBinds.OnInput(Event);
}

CBindsManager::CBindsManager() :
	m_vpBinds()
{
	m_SpecialBinds.m_pBindsManager = this;
	m_aActiveBindGroup[0] = '\0';
}

void CBindsManager::OnInit()
{
}

void CBindsManager::OnConsoleInit()
{
	ConfigManager()->RegisterCallback(ConfigSaveCallback, this);

	Console()->Register("bindv2", "s[group] s[key] ?r[command]", CFGFLAG_CLIENT, ConBind, this, "Bind key to execute a command or view keybindings in group");
	Console()->Register("bindsv2", "?s[group] ?s[key]", CFGFLAG_CLIENT, ConBinds, this, "Print command executed by this keybinding or all binds in group, or all available groups");
	Console()->Register("unbindv2", "s[group] s[key]", CFGFLAG_CLIENT, ConUnbind, this, "Unbind key from group");
	Console()->Register("unbindallv2", "?s[group]", CFGFLAG_CLIENT, ConUnbindAll, this, "Unbind all keys from group or all keys");

	RegisterBindGroups();
	SetDefaults();
}

bool CBindsManager::OnInput(const IInput::CEvent &Event)
{
	if(m_aActiveBindGroup[0] == '\0')
		return false;

	return Binds(m_aActiveBindGroup)->OnInput(Event);
}

void CBindsManager::SetDefaults()
{
	auto pIngameBinds = Binds(GROUP_INGAME);
	{ // In game binds
		pIngameBinds->UnbindAll();
		pIngameBinds->Bind(KEY_TAB, "+scoreboard");
		pIngameBinds->Bind(KEY_EQUALS, "+statboard");
		pIngameBinds->Bind(KEY_F10, "screenshot");

		pIngameBinds->Bind(KEY_A, "+left");
		pIngameBinds->Bind(KEY_D, "+right");

		pIngameBinds->Bind(KEY_SPACE, "+jump");
		pIngameBinds->Bind(KEY_MOUSE_1, "+fire");
		pIngameBinds->Bind(KEY_MOUSE_2, "+hook");
		pIngameBinds->Bind(KEY_LSHIFT, "+emote");
		pIngameBinds->Bind(KEY_RETURN, "+show_chat; chat all");
		pIngameBinds->Bind(KEY_RIGHT, "spectate_next");
		pIngameBinds->Bind(KEY_LEFT, "spectate_previous");
		pIngameBinds->Bind(KEY_RSHIFT, "+spectate");

		pIngameBinds->Bind(KEY_1, "+weapon1");
		pIngameBinds->Bind(KEY_2, "+weapon2");
		pIngameBinds->Bind(KEY_3, "+weapon3");
		pIngameBinds->Bind(KEY_4, "+weapon4");
		pIngameBinds->Bind(KEY_5, "+weapon5");

		pIngameBinds->Bind(KEY_MOUSE_WHEEL_UP, "+prevweapon");
		pIngameBinds->Bind(KEY_MOUSE_WHEEL_DOWN, "+nextweapon");

		pIngameBinds->Bind(KEY_T, "+show_chat; chat all");
		pIngameBinds->Bind(KEY_Y, "+show_chat; chat team");
		pIngameBinds->Bind(KEY_U, "+show_chat");
		pIngameBinds->Bind(KEY_I, "+show_chat; chat all /c ");

		pIngameBinds->Bind(KEY_F3, "vote yes");
		pIngameBinds->Bind(KEY_F4, "vote no");

		pIngameBinds->Bind(KEY_K, "kill");
		pIngameBinds->Bind(KEY_Q, "say /pause");
		pIngameBinds->Bind(KEY_P, "say /pause");
	}
}

void CBindsManager::RegisterBindGroup(const char *pName)
{
	if(Binds(pName) != nullptr)
	{
		log_error("binds manager", "Group with name '%s' has already been registered!", pName);
		return;
	}

	std::shared_ptr<CBindsV2> pBinds = std::make_shared<CBindsV2>(pName);
	pBinds->m_pClient = m_pClient;
	pBinds->m_SpecialBinds.m_pClient = m_pClient;
	m_vpBinds.push_back(pBinds);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Registered bind group '%s'", pName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds manager", aBuf, gs_BindPrintColor);
}

void CBindsManager::SetActiveBindGroup(const char *pName)
{
	for(auto &pBind : m_vpBinds)
	{
		if(!str_comp_nocase(pBind->GroupName(), pName))
		{
			str_copy(m_aActiveBindGroup, pName);
			return;
		}
	}
	m_aActiveBindGroup[0] = '\0';
	dbg_msg("binds manager", "invalid bind group '%s'", pName);
}

void CBindsManager::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBindsManager *pSelf = (CBindsManager *)pUserData;

	for(auto &BindGroup : pSelf->m_vpBinds)
	{
		BindGroup->SaveBinds();
	}
}

std::shared_ptr<CBindsV2> CBindsManager::Binds(const char *pGroupName)
{
	auto ItRes = std::find_if(m_vpBinds.begin(), m_vpBinds.end(), [&pGroupName](const std::shared_ptr<CBindsV2> &pBind) {
		return !str_comp_nocase(pBind->GroupName(), pGroupName);
	});
	if(ItRes == m_vpBinds.end())
		return nullptr;
	return *ItRes;
}

std::shared_ptr<CBindsV2> CBindsManager::CheckGroup(const char *pGroupName)
{
	auto pBinds = Binds(pGroupName);

	if(!pBinds)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "group %s does not exist. Use bindsv2 to list all available groups.", pGroupName);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds manager", aBuf, gs_BindPrintColor);
		return nullptr;
	}

	return pBinds;
}

void CBindsManager::ConBind(IConsole::IResult *pResult, void *pUserData)
{
	CBindsManager *pSelf = (CBindsManager *)pUserData;
	auto pBinds = pSelf->CheckGroup(pResult->GetString(0));
	if(!pBinds)
		return;

	CBindsV2::ConBind(pResult->GetString(1), pResult->GetString(2), pBinds);
}

void CBindsManager::ConBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBindsManager *pSelf = (CBindsManager *)pUserData;
	if(pResult->NumArguments() == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "available bind groups:");

		for(auto &pBinds : pSelf->m_vpBinds)
		{
			str_append(aBuf, " ");
			str_append(aBuf, pBinds->GroupName());
		}

		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bindsv2", aBuf, gs_BindPrintColor);

		return;
	}

	auto pBinds = pSelf->CheckGroup(pResult->GetString(0));
	if(!pBinds)
		return;

	CBindsV2::ConBinds(pResult->GetString(1), pBinds);
}

void CBindsManager::ConUnbind(IConsole::IResult *pResult, void *pUserData)
{
	CBindsManager *pSelf = (CBindsManager *)pUserData;
	auto pBinds = pSelf->CheckGroup(pResult->GetString(0));
	if(!pBinds)
		return;

	CBindsV2::ConUnbind(pResult->GetString(1), pBinds);
}

void CBindsManager::ConUnbindAll(IConsole::IResult *pResult, void *pUserData)
{
	CBindsManager *pSelf = (CBindsManager *)pUserData;
	if(pResult->NumArguments() == 0)
	{
		for(auto &pBinds : pSelf->m_vpBinds)
			pBinds->UnbindAll();

		return;
	}

	auto pBinds = pSelf->CheckGroup(pResult->GetString(0));
	if(!pBinds)
		return;

	CBindsV2::ConUnbindAll(pBinds);
}

void CBindsManager::RegisterBindGroups()
{
	RegisterBindGroup(GROUP_INGAME);
	RegisterBindGroup(GROUP_MENUS);
	RegisterBindGroup(GROUP_EDITOR);
	RegisterBindGroup(GROUP_DEMO_PLAYER);
}
