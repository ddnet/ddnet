#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "warlist.h"

void CWarList::OnNewSnapshot()
{
}

void CWarList::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterTCallback(ConfigSaveCallback, this);

	// TODO: PUT CON COMMANDS HERE

	m_pStorage = Kernel()->RequestInterface<IStorage>();
	IOHANDLE File = m_pStorage->OpenFile(WARLIST_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		Console()->ExecuteFile(WARLIST_FILE);
	}
}

// Preset war Commands
void CWarList::ConNameWar(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConClanWar(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConNameTeam(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConClanTeam(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConRemoveNameWar(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConRemoveNameTeam(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConRemoveClanWar(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConRemoveClanTeam(IConsole::IResult *pResult, void *pUserData) {}

// Generic Commands
void CWarList::ConName(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pName = pResult->GetString(1);
	const char *pReason = pResult->GetString(2);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->AddWarEntry(pName, "", pReason, pType);
}
void CWarList::ConClan(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pClan = pResult->GetString(1);
	const char *pReason = pResult->GetString(2);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->AddWarEntry("", pClan, pReason, pType);
}
void CWarList::ConRemoveName(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pName = pResult->GetString(1);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->RemoveWarEntry(pName, "", pType);
}
void CWarList::ConRemoveClan(IConsole::IResult *pResult, void *pUserData) 
{
	const char *pType = pResult->GetString(0);
	const char *pClan = pResult->GetString(1);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->RemoveWarEntry("", pClan, pType);
}

// Backend Commands for config file
void CWarList::ConAddWarEntry(IConsole::IResult *pResult, void *pUserData) 
{
	const char *pType = pResult->GetString(0);
	const char *pName = pResult->GetString(1);
	const char *pClan = pResult->GetString(2);
	const char *pReason = pResult->GetString(3);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->AddWarEntry(pName, pClan, pReason, pType);
}
void CWarList::ConUpsertWarType(IConsole::IResult *pResult, void *pUserData) 
{
	int Index = pResult->GetInteger(0);
	const char *pType = pResult->GetString(1);
	unsigned int ColorInt = pResult->GetInteger(2);
	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(ColorInt));
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->UpsertWarType(Index, pType, Color);
}

void CWarList::UpdateWarEntry(int Index, const char *pName, const char *pClan, const char *pReason, CWarType *pType)
{
	if(Index >= 0 && Index < static_cast<int>(m_WarEntries.size()))
	{
		str_copy(m_WarEntries[Index].m_aName, pName);
		str_copy(m_WarEntries[Index].m_aClan, pClan);
		str_copy(m_WarEntries[Index].m_aReason, pReason);
		m_WarEntries[Index].m_pWarType = pType;
	}
}

void CWarList::UpsertWarType(int Index, const char *pType, ColorRGBA Color)
{
	if(str_comp(pType, "none"))
		return;

	if(Index >= 0 && Index < static_cast<int>(m_WarTypes.size()))
	{
		str_copy(m_WarTypes[Index].m_aWarType, pType);
		m_WarTypes[Index].m_Color = Color;
	}
	else
	{
		AddWarType(pType, Color);
	}
}

void CWarList::AddWarEntry(const char *pName, const char *pClan, const char *pReason, const char *pType)
{
	CWarType *WarType = FindWarType(pType);
	CWarEntry Entry(WarType);
	str_copy(Entry.m_aReason, pReason);

	if(pClan)
		str_copy(Entry.m_aClan, pClan);
	else if(pName)
		str_copy(Entry.m_aName, pName);
	else
		return;

	m_WarEntries.push_back(Entry);
}

void CWarList::AddWarType(const char *pType, ColorRGBA Color)
{
	if(str_comp(pType, "none"))
		return;

	CWarType *Type = FindWarType(pType);
	if(*Type == m_WarTypeNone)
	{
		CWarType NewType(pType, Color);
		m_WarTypes.push_back(NewType);
	}
	else
	{
		Type->m_Color = Color;
	}
}

void CWarList::RemoveWarEntry(const char *pName, const char *pClan, const char *pType)
{
	CWarType *WarType = FindWarType(pType);
	CWarEntry Entry(WarType, pName, pClan, "");
	auto it = std::find(m_WarEntries.begin(), m_WarEntries.end(), Entry);
	if(it != m_WarEntries.end())
		m_WarEntries.erase(it);
}

void CWarList::RemoveWarType(const char *pType)
{
	CWarType Type(pType);
	auto it = std::find(m_WarTypes.begin(), m_WarTypes.end(), Type);
	if(it != m_WarTypes.end())
	{
		// Don't remove default war types
		if(!it->m_Removable)
			return;

		// Find all war entries and set them to None if they are using this type
		for(CWarEntry &Entry : m_WarEntries)
		{
			if(*Entry.m_pWarType == *it)
			{
				Entry.m_pWarType = &m_WarTypeNone;
			}
		}
		m_WarTypes.erase(it);
	}
}

CWarType *CWarList::FindWarType(const char *pType)
{
	CWarType Type(pType);
	auto it = std::find(m_WarTypes.begin(), m_WarTypes.end(), Type);
	if(it != m_WarTypes.end())
		return &(*it);
	else
		return &m_WarTypeNone;
}

CWarEntry *CWarList::FindWarEntry(const char *pName, const char *pClan, const char *pType)
{
	CWarType *WarType = FindWarType(pType);
	CWarEntry Entry(WarType, pName, pClan, "");
	auto it = std::find(m_WarEntries.begin(), m_WarEntries.end(), Entry);

	if(it != m_WarEntries.end())
		return &(*it);
	else
		return nullptr;
}

ColorRGBA CWarList::GetNameplateColor(int ClientId)
{
	return m_WarPlayers[ClientId].m_NameColor;
}
ColorRGBA CWarList::GetClanColor(int ClientId)
{
	return m_WarPlayers[ClientId].m_ClanColor;
}

void CWarList::GetReason(char *pReason, int ClientId)
{
	str_copy(pReason, m_WarPlayers[ClientId].m_aReason, sizeof(m_WarPlayers[ClientId].m_aReason));
}

CWarDataCache CWarList::GetWarData(int ClientId)
{
	return m_WarPlayers[ClientId];
}

void CWarList::SortWarEntries() {}

void CWarList::UpdateWarPlayers()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		m_WarPlayers[i].IsWarName = false;
		m_WarPlayers[i].IsWarClan = false;

		for(CWarEntry &Entry : m_WarEntries)
		{
			if(str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aName))
			{
				str_copy(m_WarPlayers[i].m_aReason, Entry.m_aReason);
				m_WarPlayers[i].IsWarName = true;
				m_WarPlayers[i].m_NameColor = Entry.m_pWarType->m_Color;
			}
			else if(Entry.m_aClan && str_comp(GameClient()->m_aClients[i].m_aClan, Entry.m_aClan))
			{
				// Name war reason has priority over clan war reason
				if(!m_WarPlayers[i].IsWarName)
					str_copy(m_WarPlayers[i].m_aReason, Entry.m_aReason);

				m_WarPlayers[i].IsWarClan = true;
				m_WarPlayers[i].m_ClanColor = Entry.m_pWarType->m_Color;
			}
		}
	}
}

void CWarList::WriteLine(const char *pLine)
{
	if(!m_WarlistFile || io_write(m_WarlistFile, pLine, str_length(pLine)) != static_cast<unsigned>(str_length(pLine)) || !io_write_newline(m_WarlistFile))
		return;
}

void CWarList::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CWarList *pThis = (CWarList *)pUserData;
	char aBufTmp[512];
	bool Failed = false;
	pThis->m_WarlistFile = pThis->m_pStorage->OpenFile(WARLIST_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);

	if(!pThis->m_WarlistFile)
	{
		dbg_msg("config", "ERROR: opening %s failed", aBufTmp);
		return;
	}

	char aBuf[1024];
	for(int i = 0; i < static_cast<int>(pThis->m_WarTypes.size()); i++)
	{
		CWarType &WarType = pThis->m_WarTypes[i];

		// Imported wartypes don't get saved
		if(WarType.m_Imported)
			continue;

		str_format(aBuf, sizeof(aBuf), "update_war_type %d \"%s\" %d", i, WarType.m_aWarType, WarType.m_Color.Pack());
		pThis->WriteLine(aBuf);
	}
	for(CWarEntry &Entry : pThis->m_WarEntries)
	{
		// Imported entries don't get saved
		if(Entry.m_Imported)
			continue;

		str_format(aBuf, sizeof(aBuf), "add_war_entry \"%s\" \"%s\" \"%s\" \"%s\"", Entry.m_pWarType->m_aWarType, Entry.m_aName, Entry.m_aClan, Entry.m_aReason);
		pThis->WriteLine(aBuf);
	}

	if(io_sync(pThis->m_WarlistFile) != 0)
		Failed = true;

	if(io_close(pThis->m_WarlistFile) != 0)
		Failed = true;

	pThis->m_WarlistFile = {};

	if(Failed)
		dbg_msg("config", "ERROR: writing to %s failed", aBufTmp);

	return;
}
