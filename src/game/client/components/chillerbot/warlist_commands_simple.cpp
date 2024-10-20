// ChillerDragon 2021 - chillerbot ux

#include <engine/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>

#include <base/system.h>

#include "warlist.h"
#include <engine/client.h>

void CWarList::AddSimpleMute(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/mutes", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create mutes folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/mutes/mutes", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create mutes/mutes folder");
		return;
	}

	AddMute("mute", pName);
}

void CWarList::AddSimpleHelper(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/helper", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create helper folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/helper/helper", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create helper/helper folder");
		return;
	}

	AddHelper("helper", pName);
	RemoveTeamNoMsg(pName);
	RemoveWarNoMsg(pName);
}

void CWarList::AddSimpleWar(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/war", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create war folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/war/war", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create war/war folder");
		return;
	}

	AddWar("war", pName);
	RemoveTeamNoMsg(pName);
	RemoveHelperNoMsg(pName);
}

void CWarList::AddSimpleTeam(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/team", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create team folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/team/team", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to create team/team folder");
		return;
	}

	AddTeam("team", pName);
	RemoveWarNoMsg(pName);
	RemoveHelperNoMsg(pName);
}

void CWarList::RemoveSimpleHelper(const char *pName)
{
	char aBuf[512];
	if(!RemoveHelperNameFromVector("chillerbot/warlist/helper/helper", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the helper list", pName);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return;
	}
	if(!WriteHelperNames("chillerbot/warlist/helper/helper"))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to write helper names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the helper list", pName);
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	ReloadList();
}

void CWarList::RemoveSimpleWar(const char *pName)
{
	char aBuf[512];
	if(!RemoveWarNameFromVector("chillerbot/warlist/war/war", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the war list", pName);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return;
	}
	if(!WriteWarNames("chillerbot/warlist/war/war"))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to write war names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the war list", pName);
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	ReloadList();
}


void CWarList::RemoveSimpleMute(const char *pName)
{
	char aBuf[512];
	if(!RemoveMuteNameFromVector("chillerbot/warlist/mutes/mutes", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the war list", pName);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return;
	}
	if(!WriteMuteNames("chillerbot/warlist/mutes/mutes"))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to write war names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the mutes list", pName);
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	ReloadList();
}

void CWarList::RemoveSimpleTeam(const char *pName)
{
	char aBuf[512];
	if(!RemoveTeamNameFromVector("chillerbot/warlist/team/team", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the war list", pName);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return;
	}
	if(!WriteTeamNames("chillerbot/warlist/team/team"))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: failed to write war names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the team list", pName);
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	ReloadList();
}


// Send no message if removing an name from the list

void CWarList::RemoveTeamNoMsg(const char *pName)
{
	if(!RemoveTeamNameFromVector("chillerbot/warlist/team/team", pName))
	{
		return;
	}
	if(!WriteTeamNames("chillerbot/warlist/team/team"))
	{}
	ReloadList();
}

void CWarList::RemoveWarNoMsg(const char *pName)
{
	if(!RemoveWarNameFromVector("chillerbot/warlist/war/war", pName))
	{
		return;
	}
	if(!WriteWarNames("chillerbot/warlist/war/war"))
	{}
	ReloadList();
}

void CWarList::RemoveHelperNoMsg(const char *pName)
{
	if(!RemoveHelperNameFromVector("chillerbot/warlist/helper/helper", pName))
	{
		return;
	}
	if(!WriteHelperNames("chillerbot/warlist/helper/helper"))
	{}
	ReloadList();
}

bool CWarList::OnChatCmdSimple(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine)
{
	if(!str_comp(pCmd, "search")) // "search <name can contain spaces>"
	{
		m_pClient->m_Chat.AddLine(-2, 0, "Error: search only works in advanced warlist mode");
		return true;
	}
	else if(!str_comp(pCmd, "help"))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "=== Chillerbot warlist ===");
		m_pClient->m_Chat.AddLine(-2, 0, "!war <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!delwar <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!team <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!delteam <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!helper <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!delhelper <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!mute <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!delmute <name>");
		// m_pClient->m_Chat.AddLine(-2, 0, "!search <name>");
	}
	else if(!str_comp(pCmd, "helper") || !str_comp(pCmd, "addhelper") || !str_comp(pCmd, g_Config.m_ClAddHelperString)) // "team <name>"
	{
		AddSimpleHelper(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "mute") || !str_comp(pCmd, "addmute") || !str_comp(pCmd, g_Config.m_ClAddMuteString)) // "team <name>"
	{
		AddSimpleMute(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "war") || !str_comp(pCmd, "addwar") || !str_comp(pCmd, g_Config.m_ClAddWarString)) // "war <name>"
	{
		AddSimpleWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "team") || !str_comp(pCmd, "addteam") || !str_comp(pCmd, g_Config.m_ClAddTeamString)) // "team <name>"
	{
		AddSimpleTeam(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delhelper") || !str_comp(pCmd, "unhelper") || !str_comp(pCmd, g_Config.m_ClRemoveHelperString)) // "delwar <name>"
	{
		RemoveSimpleHelper(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delmute") || !str_comp(pCmd, "unmute") || !str_comp(pCmd, g_Config.m_ClRemoveMuteString)) // "delwar <name>"
	{
		RemoveSimpleMute(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delwar") || !str_comp(pCmd, "unwar") || !str_comp(pCmd, "peace") || !str_comp(pCmd, g_Config.m_ClRemoveWarString)) // "delwar <name>"
	{
		RemoveSimpleWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delteam") || !str_comp(pCmd, "unteam") || !str_comp(pCmd, "unfriend") || !str_comp(pCmd, g_Config.m_ClRemoveTeamString)) // "delteam <name>"
	{
		RemoveSimpleTeam(pRawArgLine);
		return true;
	}
	else if(
		!str_comp(pCmd, "addreason") ||
		!str_comp(pCmd, "create") ||
		!str_comp(pCmd, "addtraitor"))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Error: %s only works in advanced warlist mode", pCmd);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return true;
	}
	else
	{
		return false;
	}
	return true;
}
