// ChillerDragon 2021 - chillerbot ux

#include <vector>

#include <engine/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>

#include <base/system.h>

#include <engine/storage.h>

#include "chillerbotux.h"

#include "warlist.h"

void CWarList::OnInit()
{
	m_Verbose = true;
	ReloadList();
	m_Verbose = false;
	m_NextReload = time_get() + time_freq();
}

void CWarList::ReloadList()
{
	m_WarDirs = 0;
	m_TeamDirs = 0;
	m_TraitorDirs = 0;
	m_vTemplist.clear();
	m_vHelperlist.clear();
	m_vMutelist.clear();
	m_vWarlist.clear();
	m_vTeamlist.clear();
	m_vTraitorlist.clear();
	m_vNeutrallist.clear();
	m_vWarClanlist.clear();
	m_vTeamClanlist.clear();

	{
		LoadMuteNames("chillerbot/warlist/mutes/mutes");
		LoadHelperNames("chillerbot/warlist/helper/helper");
		LoadWarNames("chillerbot/warlist/war/war");
		LoadTeamNames("chillerbot/warlist/team/team");
		LoadTempNames("chillerbot/templist/temp/tempwar");
	}

	for(auto &WarPlayer : m_aWarPlayers)
		WarPlayer.m_aName[0] = '\0';
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team: %d war: %d", m_vHelperlist, (m_WarDirs + m_TraitorDirs));
	// TODO: fix on initial load
	// 		 maybe https://github.com/chillerbot/chillerbot-ux/issues/22 is needed
	m_pClient->m_ChillerBotUX.SetComponentNoteLong("war list", aBuf);
}

void CWarList::GetTemplistPathByNeedle(const char *pSearch, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTemplist)
		if(str_find(Entry.first.c_str(), pSearch))
			str_copy(pPath, Entry.second.c_str(), Size);

}


void CWarList::GetHelperlistPathByNeedle(const char *pSearch, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vHelperlist)
		if(str_find(Entry.first.c_str(), pSearch))
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetMutelistPathByNeedle(const char *pSearch, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vMutelist)
		if(str_find(Entry.first.c_str(), pSearch))
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetWarlistPathByNeedle(const char *pSearch, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTeamlist)
		if(str_find(Entry.first.c_str(), pSearch))
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetTeamlistPathByNeedle(const char *pSearch, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTraitorlist)
		if(str_find(Entry.first.c_str(), pSearch))
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetTraitorlistPathByNeedle(const char *pSearch, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTraitorlist)
		if(str_find(Entry.first.c_str(), pSearch))
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetNeutrallistPathByNeedle(const char *pSearch, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vNeutrallist)
		if(str_find(Entry.first.c_str(), pSearch))
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetMutelistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vMutelist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetHelperlistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vHelperlist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetTemplistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTemplist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetWarlistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vWarlist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetTeamlistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTeamlist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetTraitorlistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vTraitorlist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

void CWarList::GetNeutrallistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vNeutrallist)
		if(std::string(pName) == Entry.first)
			str_copy(pPath, Entry.second.c_str(), Size);
}

int CWarList::LoadTempDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname) || !str_comp("..", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/templist/temp/tempwar", pDirname);
	return pSelf->LoadTempNames(aFilename);
}

int CWarList::LoadHelperDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname) || !str_comp("..", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/helper/helper", pDirname);
	return pSelf->LoadHelperNames(aFilename);
}

int CWarList::LoadMuteDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname) || !str_comp("..", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/mutes/mutes", pDirname);
	return pSelf->LoadMuteNames(aFilename);
}

int CWarList::LoadWarDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname) || !str_comp("..", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s", pDirname);
	return pSelf->LoadWarNames(aFilename);
}

int CWarList::LoadTeamDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname) || !str_comp("..", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/team/%s", pDirname);
	return pSelf->LoadTeamNames(aFilename);
}

int CWarList::LoadTraitorDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname) || !str_comp("..", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/traitor/%s", pDirname);
	return pSelf->LoadTraitorNames(aFilename);
}

int CWarList::LoadNeutralDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname) || !str_comp("..", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/neutral/%s", pDirname);
	return pSelf->LoadNeutralNames(aFilename);
}

void CWarList::LoadTempList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/templist/temp", LoadHelperDir, this);
}


void CWarList::LoadHelperList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/helper", LoadHelperDir, this);
}

void CWarList::LoadMuteList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/mutes", LoadMuteDir, this);
}

void CWarList::LoadWarList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/war", LoadWarDir, this);
}

void CWarList::LoadTeamList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/team", LoadTeamDir, this);
}

void CWarList::LoadTraitorList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/traitor", LoadTraitorDir, this);
}

void CWarList::LoadNeutralList()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "chillerbot/warlist/neutral", LoadNeutralDir, this);
}

void CWarList::LoadWarClanList()
{
	LoadWarClanNames("chillerbot/warlist/warlist_clans.txt");
}

void CWarList::LoadTeamClanList()
{
	LoadTeamClanNames("chillerbot/warlist/teamlist_clans.txt");
}

void CWarList::LoadWarClanPrefixList()
{
	LoadWarClanPrefixNames("chillerbot/warlist/warlist_clans_prefix.txt");
}

bool CWarList::IsMutelist(const char *pName)
{
	return std::any_of(std::begin(m_vMutelist), std::end(m_vMutelist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsHelperlist(const char *pName)
{
	return std::any_of(std::begin(m_vHelperlist), std::end(m_vHelperlist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsTemplist(const char *pName)
{
	return std::any_of(std::begin(m_vTemplist), std::end(m_vTemplist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsWarlist(const char *pName)
{
	for(auto &WarClanPrefix : m_vWarClanPrefixlist)
		if(str_startswith(pName, WarClanPrefix.c_str()))
			return true;
	return std::any_of(std::begin(m_vWarlist), std::end(m_vWarlist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsTeamlist(const char *pName)
{
	return std::any_of(std::begin(m_vTeamlist), std::end(m_vTeamlist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsTraitorlist(const char *pName)
{
	return std::any_of(std::begin(m_vTraitorlist), std::end(m_vTraitorlist), [&pName](const std::pair<std::string, std::string> &Entry) { return std::string(pName) == Entry.first; });
}

bool CWarList::IsWarClanlist(const char *pClan)
{
	return (std::find(m_vWarClanlist.begin(), m_vWarClanlist.end(), std::string(pClan)) != m_vWarClanlist.end());
}

bool CWarList::IsTeamClanlist(const char *pClan)
{
	return (std::find(m_vTeamClanlist.begin(), m_vTeamClanlist.end(), std::string(pClan)) != m_vTeamClanlist.end());
}

bool CWarList::IsWarClanmate(const char *pClan)
{
	for(auto &WarPlayer : m_aWarPlayers)
	{
		if(WarPlayer.m_aClan[0] == '\0')
			continue;
		if(!WarPlayer.m_IsWar && !WarPlayer.m_IsTraitor)
			continue;

		if(!str_comp(pClan, WarPlayer.m_aClan))
			return true;
	}
	return false;
}

void CWarList::GetWarReason(const char *pName, char *pReason, int ReasonSize)
{
	if(!pReason)
		return;
	pReason[0] = '\0';
	if(!Storage())
		return;

	char aFilenames[2][128];
	char aBuf[182];
	GetWarlistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[0], sizeof(aFilenames[0]), "%s/reason.txt", aBuf);
	else
		aFilenames[0][0] = '\0';
	GetTraitorlistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[1], sizeof(aFilenames[1]), "%s/reason.txt", aBuf);
	else
		aFilenames[1][0] = '\0';

	for(auto &pFilename : aFilenames)
	{
		if(!pFilename[0])
			continue;

		IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
		CLineReader Reader;
		if(!File)
			continue;
		if(!Reader.OpenFile(File))
		{
			str_format(aBuf, sizeof(aBuf), "failed to open '%s'", pFilename);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
			continue;
		}

		const char *pLine;
		// read one line only
		pLine = Reader.Get();
		if(pLine)
			str_copy(pReason, pLine, ReasonSize);

		break;
	}
}

void CWarList::GetWarClansStr(char *pBuf, int Size)
{
	if(!pBuf)
		return;
	char aBuf[256];
	aBuf[0] = '\0';
	for(auto &War : m_vWarClanlist)
	{
		const char *pWar = War.c_str();
		if(str_startswith(pWar, "# "))
			continue;
		if(aBuf[0])
			str_append(aBuf, ", ", sizeof(aBuf));
		str_append(aBuf, pWar, sizeof(aBuf));
	}
	str_copy(pBuf, aBuf, Size);
}

bool CWarList::IsWar(const char *pName, const char *pClan)
{
	return IsWarlist(pName) || IsTraitorlist(pName) || IsWarClanlist(pClan);
}

bool CWarList::IsMute(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientId].m_aName))
	{
		return m_aWarPlayers[ClientId].m_IsMute;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsHelper(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientId].m_aName))
	{
		return m_aWarPlayers[ClientId].m_IsHelper;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsWar(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientId].m_aName))
	{
		return m_aWarPlayers[ClientId].m_IsWar;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTeam(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientId].m_aName))
	{
		return m_aWarPlayers[ClientId].m_IsTeam;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTraitor(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientId].m_aName))
	{
		return m_aWarPlayers[ClientId].m_IsTraitor;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsWarClan(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientId].m_aClan))
	{
		return m_aWarPlayers[ClientId].m_IsWarClan;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTeamClan(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientId].m_aClan))
	{
		return m_aWarPlayers[ClientId].m_IsTeamClan;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsWarClanmate(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientId].m_aClan))
	{
		return m_aWarPlayers[ClientId].m_IsWarClanmate;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTemp(int ClientId)
{
	const char *pName = m_pClient->m_aClients[ClientId].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientId].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientId].m_aClan))
	{
		return m_aWarPlayers[ClientId].m_IsTempWar;
	}
	str_copy(m_aWarPlayers[ClientId].m_aName, pName, sizeof(m_aWarPlayers[ClientId].m_aName));
	str_copy(m_aWarPlayers[ClientId].m_aClan, pClan, sizeof(m_aWarPlayers[ClientId].m_aClan));
	m_aWarPlayers[ClientId].m_IsTempWar = IsTemplist(pName);
	m_aWarPlayers[ClientId].m_IsHelper = IsHelperlist(pName);
	m_aWarPlayers[ClientId].m_IsMute = IsMutelist(pName);
	m_aWarPlayers[ClientId].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientId].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientId].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientId].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientId].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

void CWarList::SetNameplateColor(int ClientId, ColorRGBA *pColor)
{
	const auto &ClientData = m_pClient->m_aClients[ClientId];
	if(!g_Config.m_ClWarList)
		return;

	if(IsWar(ClientId) || IsTemp(ClientId) && g_Config.m_ClDoEnemyNameColor)
		*pColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClWarColor));
	else if(IsTeam(ClientId) && g_Config.m_ClDoTeammateNameColor)
		*pColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClTeamColor));
	else if(g_Config.m_ClDoFriendNameColor && ClientData.m_Friend)
		*pColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFriendColor));
	else if(g_Config.m_ClFoeNameColor && ClientData.m_Foe)
		*pColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFoeColor));
	else if(IsHelper(ClientId) && g_Config.m_ClDoHelperNameColor)
		*pColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHelperColor));
	else if(IsTraitor(ClientId))
		*pColor = ColorRGBA(0.1f, 0.1f, 0.1f, 1.0f);
	else if(IsWarClan(ClientId))
		*pColor = ColorRGBA(7.0f, 0.2f, 0.2f, 1.0f);
	else if(IsTeamClan(ClientId))
		*pColor = ColorRGBA(0.0f, 0.9f, 0.2f, 1.0f);
	else if(IsWarClanmate(ClientId))
		*pColor = ColorRGBA(7.0f, 0.5f, 0.2f, 1.0f);
}

bool CWarList::RemoveMuteNameFromVector(const char *pDir, const char *pName)
{
	int Hits = 0;
	m_vMutelist.erase(
		std::remove_if(m_vMutelist.begin(), m_vMutelist.end(),
			[pName, pDir, &Hits](const std::pair<std::string, std::string> &Entry) {
				// keep the same name in other directories
				if(str_comp(pDir, Entry.second.c_str()))
					return false;

				if(!str_comp(pName, Entry.first.c_str()))
				{
					Hits++;
					return true;
				}
				return false;
			}),
		m_vMutelist.end());
	return Hits > 0;
}

bool CWarList::RemoveTempNameFromVector(const char *pDir, const char *pName)
{
	int Hits = 0;
	m_vTemplist.erase(
		std::remove_if(m_vTemplist.begin(), m_vTemplist.end(),
			[pName, pDir, &Hits](const std::pair<std::string, std::string> &Entry) {
				// keep the same name in other directories
				if(str_comp(pDir, Entry.second.c_str()))
					return false;

				if(!str_comp(pName, Entry.first.c_str()))
				{
					Hits++;
					return true;
				}
				return false;
			}),
		m_vTemplist.end());
	return Hits > 0;
}


bool CWarList::RemoveHelperNameFromVector(const char *pDir, const char *pName)
{
	int Hits = 0;
	m_vHelperlist.erase(
		std::remove_if(m_vHelperlist.begin(), m_vHelperlist.end(),
			[pName, pDir, &Hits](const std::pair<std::string, std::string> &Entry) {
				// keep the same name in other directories
				if(str_comp(pDir, Entry.second.c_str()))
					return false;

				if(!str_comp(pName, Entry.first.c_str()))
				{
					Hits++;
					return true;
				}
				return false;
			}),
		m_vHelperlist.end());
	return Hits > 0;
}

bool CWarList::RemoveTeamNameFromVector(const char *pDir, const char *pName)
{
	int Hits = 0;
	m_vTeamlist.erase(
		std::remove_if(m_vTeamlist.begin(), m_vTeamlist.end(),
			[pName, pDir, &Hits](const std::pair<std::string, std::string> &Entry) {
				// keep the same name in other directories
				if(str_comp(pDir, Entry.second.c_str()))
					return false;

				if(!str_comp(pName, Entry.first.c_str()))
				{
					Hits++;
					return true;
				}
				return false;
			}),
		m_vTeamlist.end());
	return Hits > 0;
}



bool CWarList::RemoveWarNameFromVector(const char *pDir, const char *pName)
{
	int Hits = 0;
	m_vWarlist.erase(
		std::remove_if(m_vWarlist.begin(), m_vWarlist.end(),
			[pName, pDir, &Hits](const std::pair<std::string, std::string> &Entry) {
				// keep the same name in other directories
				if(str_comp(pDir, Entry.second.c_str()))
					return false;

				if(!str_comp(pName, Entry.first.c_str()))
				{
					Hits++;
					return true;
				}
				return false;
			}),
		m_vWarlist.end());
	return Hits > 0;
}

bool CWarList::WriteWarNames(const char *pDir)
{
	if(!Storage())
		return false;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		return false;
	}

	for(auto &Entry : m_vWarlist)
	{
		// only write names from that directory
		if(str_comp(Entry.second.c_str(), pDir))
			continue;

		io_write(File, Entry.first.c_str(), str_length(Entry.first.c_str()));
		io_write(File, "\n", 1);
	}

	io_close(File);
	return true;
}

bool CWarList::WriteTeamNames(const char *pDir)
{
	if(!Storage())
		return false;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		return false;
	}

	for(auto &Entry : m_vTeamlist)
	{
		// only write names from that directory
		if(str_comp(Entry.second.c_str(), pDir))
			continue;

		io_write(File, Entry.first.c_str(), str_length(Entry.first.c_str()));
		io_write(File, "\n", 1);
	}

	io_close(File);
	return true;
}

//MuteStuff

bool CWarList::WriteMuteNames(const char *pDir)
{
	if(!Storage())
		return false;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		return false;
	}

	for(auto &Entry : m_vMutelist)
	{
		// only write names from that directory
		if(str_comp(Entry.second.c_str(), pDir))
			continue;

		io_write(File, Entry.first.c_str(), str_length(Entry.first.c_str()));
		io_write(File, "\n", 1);
	}

	io_close(File);
	return true;
}

int CWarList::LoadMuteNames(const char *pDir)
{
	if(!Storage())
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open '%s' storage is nullptr", pDir);
		// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 1;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_MuteDirs++;
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading mute list file '%s'", aFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vMutelist.emplace_back(Entry);
	}

	return 0;
}

bool CWarList::WriteTempNames(const char *pDir)
{
	if(!Storage())
		return false;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		return false;
	}

	for(auto &Entry : m_vTemplist)
	{
		// only write names from that directory
		if(str_comp(Entry.second.c_str(), pDir))
			continue;

		io_write(File, Entry.first.c_str(), str_length(Entry.first.c_str()));
		io_write(File, "\n", 1);
	}

	io_close(File);
	return true;
}

int CWarList::LoadTempNames(const char *pDir)
{
	if(!Storage())
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open '%s' storage is nullptr", pDir);
		// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 1;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_TempDirs++;
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading temp list file '%s'", aFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vTemplist.emplace_back(Entry);
	}

	return 0;
}

// Helper sutff

bool CWarList::WriteHelperNames(const char *pDir)
{
	if(!Storage())
		return false;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		return false;
	}

	for(auto &Entry : m_vHelperlist)
	{
		// only write names from that directory
		if(str_comp(Entry.second.c_str(), pDir))
			continue;

		io_write(File, Entry.first.c_str(), str_length(Entry.first.c_str()));
		io_write(File, "\n", 1);
	}

	io_close(File);
	return true;
}

int CWarList::LoadHelperNames(const char *pDir)
{
	if(!Storage())
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open '%s' storage is nullptr", pDir);
		// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 1;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_HelperDirs++;
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading helper list file '%s'", aFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vHelperlist.emplace_back(Entry);
	}

	return 0;
}


int CWarList::LoadWarNames(const char *pDir)
{
	if(!Storage())
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open '%s' storage is nullptr", pDir);
		// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 1;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_WarDirs++;
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war list file '%s'", aFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vWarlist.emplace_back(Entry);
	}

	return 0;
}

int CWarList::LoadTeamNames(const char *pDir)
{
	if(!Storage())
		return 1;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_TeamDirs++;
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading team list file '%s'", aFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vTeamlist.emplace_back(Entry);
	}

	return 0;
}

int CWarList::LoadTraitorNames(const char *pDir)
{
	if(!Storage())
		return 1;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	m_TraitorDirs++;
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading traitor list file '%s'", aFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vTraitorlist.emplace_back(Entry);
	}

	return 0;
}

int CWarList::LoadNeutralNames(const char *pDir)
{
	if(!Storage())
		return 1;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/names.txt", pDir);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		// Print(aBuf);
		return 0;
	}
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading neutral list file '%s'", aFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vNeutrallist.emplace_back(Entry);
	}

	return 0;
}

int CWarList::LoadWarClanNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war clans list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war clans list file '%s'", pFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", pFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		m_vWarClanlist.emplace_back(pLine);
	}

	return 0;
}

int CWarList::LoadTeamClanNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open team clans list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading team clans list file '%s'", pFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", pFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		m_vTeamClanlist.emplace_back(pLine);
	}

	return 0;
}

int CWarList::LoadWarClanPrefixNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war clan prefix list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	const char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war clan prefix list file '%s'", pFilename);
	Print(aBuf);

	if(!Reader.OpenFile(File))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", pFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return 0;
	}

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces_const(pLine)[0])
			continue;
		m_vWarClanPrefixlist.emplace_back(pLine);
	}

	return 0;
}

void CWarList::OnConsoleInit()
{
	Console()->Register("warlist", "?s[command]?s[arg1]?s[arg2]", CFGFLAG_CLIENT, ConWarlist, this, "Activate afk mode (auto chat respond)");

	Console()->Chain("cl_war_list", ConchainWarList, this);

	// Lots of love to chillerbot! But please, if you read this, make this be a default thing
	// and also add silent commands with "." cause im too stupid! pls thx c:

	
	// Add Mute
	Console()->Register("addhelper", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddHelper, this, "Adds a Name to The Helperlist");
	Console()->Register("add_helper", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddHelper, this, "Adds a Name to The Helperlist");
	Console()->Register("helper", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddHelper, this, "Adds a Name to The Helperlist");

	// Remove Mute
	Console()->Register("unhelper", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveHelper, this, "Removes a Name From The Helperlist");
	Console()->Register("delhelper", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveHelper, this, "Removes a Namer From The Helperlist");
	Console()->Register("del_helper", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveHelper, this, "Removes a Name From The Helperlist");
	Console()->Register("removehelper", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveHelper, this, "Removes a Name From The Helperlist");

	// Add Mute
	Console()->Register("addmute", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddMute, this, "Adds a Muted Player to The Mutelist");
	Console()->Register("add_mute", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddMute, this, "Adds a Muted Player to The Mutelist");
	Console()->Register("mute", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddMute, this, "Adds a Muted Player to The Mutelist");

	// Remove Mute
	Console()->Register("unmute", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveMute, this, "Removes a Muted Player From The Mutelist");
	Console()->Register("delmute", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveMute, this, "Removes a Muted Player From The Mutelist");
	Console()->Register("del_mute", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveMute, this, "Removes a Muted Player From The Mutelist");
	Console()->Register("removemute", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveMute, this, "Removes a Muted Player From The Mutelist");

	// Add Team
	Console()->Register("addteam", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddTeam, this, "Adds a Teammate to The Teamlist");
	Console()->Register("add_team", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddTeam, this, "Adds a Teammate to The Teamlist");
	Console()->Register("team", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddTeam, this, "Adds a Teammate to The Teamlist");

	// Remove Team
	Console()->Register("unteam", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTeam, this, "removes a war to the Teamlist");
	Console()->Register("delteam", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTeam, this, "removes a Name from the Teamlist");
	Console()->Register("del_team", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTeam, this, "removes a Name from the Teamlist");
	Console()->Register("removeteam", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTeam, this, "removes a Name from the Teamlist");

	// Add War
	Console()->Register("addwar", "s[name] ?s[clan]", CFGFLAG_CLIENT,ConAddWar, this, "Adds an Enemy to The Warlist");
	Console()->Register("add_war", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddWar, this, "Adds an Enemy to The Warlist");
	Console()->Register("war", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddWar, this, "Adds an Enemy to The Warlist");

	// Remove War
	Console()->Register("unwar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveWar, this, "Removes an Enemy From The Warlist");
	Console()->Register("delwar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveWar, this, "Removes an Enemy From The Warlist");
	Console()->Register("del_war", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveWar, this, "Removes an Enemy From The Warlist");
	Console()->Register("removewar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveWar, this, "Removes an Enemy From The Warlist");

	Console()->Register("addtempwar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddTempWar, this, "Adds an Enemy to The Warlist");
	Console()->Register("add_temp_war", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddTempWar, this, "Adds an Enemy to The Warlist");
	Console()->Register("tempwar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddTempWar, this, "Adds an Enemy to The Warlist");

	// Remove War
	Console()->Register("untempwar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTempWar, this, "Removes an Enemy From The Warlist");
	Console()->Register("deltempwar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTempWar, this, "Removes an Enemy From The Warlist");
	Console()->Register("del_temp_war", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTempWar, this, "Removes an Enemy From The Warlist");
	Console()->Register("removetempwar", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveTempWar, this, "Removes an Enemy From The Warlist");


	
}

void CWarList::ConAddTempWar(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->AddSimpleTempWar(pResult->GetString(0));
}

void CWarList::ConRemoveTempWar(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->RemoveSimpleTempWar(pResult->GetString(0));
}


void CWarList::ConAddHelper(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->AddSimpleHelper(pResult->GetString(0));
}

void CWarList::ConRemoveHelper(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->RemoveSimpleHelper(pResult->GetString(0));
}




void CWarList::ConAddMute(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->AddSimpleMute(pResult->GetString(0));
}

void CWarList::ConRemoveMute(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->RemoveSimpleMute(pResult->GetString(0));
}

void CWarList::ConAddTeam(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->AddSimpleTeam(pResult->GetString(0));
}

void CWarList::ConRemoveTeam(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->RemoveSimpleTeam(pResult->GetString(0));
}

void CWarList::ConAddWar(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->AddSimpleWar(pResult->GetString(0));
}

void CWarList::ConRemoveWar(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pSelf->RemoveSimpleWar(pResult->GetString(0));
}

void CWarList::ConchainWarList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
	{
		pSelf->ReloadList();
	}

}

void CWarList::OnRender()
{
	if(!g_Config.m_ClWarList)
		return;

	if(g_Config.m_ClWarListAutoReload && time_get() > m_NextReload)
	{
		m_NextReload = time_get() + time_freq() * g_Config.m_ClWarListAutoReload;
		ReloadList();
	}
}

void CWarList::ConWarlist(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	if(!str_comp(pResult->GetString(0), "reload"))
	{
		pSelf->m_Verbose = true;
		pSelf->ReloadList();
		pSelf->m_Verbose = false;
	}
	else if(!str_comp(pResult->GetString(0), "help"))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "warlist ?s[command]?s[arg1]?s[arg2]");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "commands:");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "  reload - loads the war list again from filesystem");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "  help - shows this help");
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "unkown warlist command try help");
	}
}

void CWarList::Print(const char *pMsg)
{
	if(!m_Verbose)
		return;

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", pMsg);
}

bool CWarList::AddWar(const char *pFolder, const char *pName)
{
	char aBuf[512];
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s/names.txt", pFolder);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return false;
	}
	
	io_write(File, pName, str_length(pName));
	io_write_newline(File);
	io_close(File);

	str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", pName, pFolder);
	ReloadList();
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	return true;
}

bool CWarList::AddTeam(const char *pFolder, const char *pName)
{
	char aBuf[512];
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/team/%s/names.txt", pFolder);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return false;
	}

	io_write(File, pName, str_length(pName));
	io_write_newline(File);
	io_close(File);

	str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", pName, pFolder);
	ReloadList();
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	return true;
}

bool CWarList::AddTempWar(const char *pFolder, const char *pName)
{
	char aBuf[512];
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/templist/temp/tempwar/names.txt", pFolder);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open temp war list file '%s'", aFilename);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return false;
	}

	io_write(File, pName, str_length(pName));
	io_write_newline(File);
	io_close(File);

	str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", pName, pFolder);
	ReloadList();
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	return true;
}


bool CWarList::AddHelper(const char *pFolder, const char *pName)
{
	char aBuf[512];
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/helper/helper/names.txt", pFolder);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open helper list file '%s'", aFilename);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return false;
	}

	io_write(File, pName, str_length(pName));
	io_write_newline(File);
	io_close(File);

	str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", pName, pFolder);
	ReloadList();
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	return true;
}

bool CWarList::AddMute(const char *pFolder, const char *pName)
{
	char aBuf[512];
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/mutes/mutes/names.txt", pFolder);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open mute list file '%s'", aFilename);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		return false;
	}

	io_write(File, pName, str_length(pName));
	io_write_newline(File);
	io_close(File);

	str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", pName, pFolder);
	ReloadList();
	m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	return true;
}

bool CWarList::SearchName(const char *pName, bool AllowPartialMatch, bool Silent)
{
	char aBuf[512];
	char aFilenames[3][128];
	if(AllowPartialMatch)
		GetWarlistPathByNeedle(pName, sizeof(aBuf), aBuf);
	else
		GetWarlistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[0], sizeof(aFilenames[0]), "chillerbot/warlist/war/war/names.txt", aBuf);
	else
		aFilenames[0][0] = '\0';

	if(AllowPartialMatch)
		GetTeamlistPathByNeedle(pName, sizeof(aBuf), aBuf);
	else
		GetTeamlistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[1], sizeof(aFilenames[1]), "chillerbot/warlist/team/team/names.txt", aBuf);
	else
		aFilenames[1][0] = '\0';

	if(AllowPartialMatch)
		GetMutelistPathByNeedle(pName, sizeof(aBuf), aBuf);
	else
		GetMutelistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[2], sizeof(aFilenames[2]), "chillerbot/warlist/mutes/mutes/names.txt", aBuf);
	else
		aFilenames[2][0] = '\0';

	if(AllowPartialMatch)
		GetHelperlistPathByNeedle(pName, sizeof(aBuf), aBuf);
	else
		GetHelperlistPathByName(pName, sizeof(aBuf), aBuf);
	if(aBuf[0])
		str_format(aFilenames[2], sizeof(aFilenames[2]), "chillerbot/warlist/helper/helper/names.txt", aBuf);
	else
		aFilenames[2][0] = '\0';

	bool Found = false;
	for(auto &pFilename : aFilenames)
	{
		if(!pFilename[0])
			continue;

		IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

		if(!File)
			continue;

		const char *pLine;
		CLineReader Reader;

		if(!Reader.OpenFile(File))
		{
			str_format(aBuf, sizeof(aBuf), "failed to open '%s'", pFilename);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
			continue;
		}

		// read one line only
		pLine = Reader.Get();
		if(pLine)
		{
			str_format(aBuf, sizeof(aBuf), "[%s] names: %s", pFilename, pLine);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}

		Found = true;
	}
	if(!Found)
	{
		if(!Silent)
		{
			str_format(aBuf, sizeof(aBuf), "Name '%s' not found", pName);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}
		return false;
	}
	return true;
}

bool CWarList::OnChatCmd(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine)
{
	if(!g_Config.m_ClWarList)
		return false;
	if(ClientId != m_pClient->m_Snap.m_LocalClientId)
		return false;

	return OnChatCmdSimple(Prefix, ClientId, Team, pCmd, NumArgs, ppArgs, pRawArgLine);
}
