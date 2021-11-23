// ChillerDragon 2021 - chillerbot ux

#include <vector>

#include <engine/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>

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
	m_vWarlist.clear();
	m_vTeamlist.clear();
	m_vTraitorlist.clear();
	m_vWarClanlist.clear();
	m_vTeamClanlist.clear();
	LoadWarList();
	LoadTeamList();
	LoadTraitorList();
	LoadWarClanList();
	LoadTeamClanList();
	LoadWarClanPrefixList();
	for(auto &WarPlayer : m_aWarPlayers)
		WarPlayer.m_aName[0] = '\0';
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team: %d war: %d", m_TeamDirs, (m_WarDirs + m_TraitorDirs));
	// TODO: fix on initial load
	// 		 maybe https://github.com/chillerbot/chillerbot-ux/issues/22 is needed
	m_pClient->m_ChillerBotUX.SetComponentNoteLong("war list", aBuf);
}

void CWarList::GetWarlistPathByName(const char *pName, int Size, char *pPath)
{
	pPath[0] = '\0';
	for(auto &Entry : m_vWarlist)
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

int CWarList::LoadWarDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s", pDirname);
	return pSelf->LoadWarNames(aFilename);
}

int CWarList::LoadTeamDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/team/%s/names.txt", pDirname);
	return pSelf->LoadTeamNames(aFilename);
}

int CWarList::LoadTraitorDir(const char *pDirname, int IsDir, int DirType, void *pUser)
{
	CWarList *pSelf = (CWarList *)pUser;
	if(!IsDir || !str_comp(".", pDirname))
		return 0;
	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/traitor/%s", pDirname);
	return pSelf->LoadTraitorNames(aFilename);
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

bool CWarList::IsWarlist(const char *pName)
{
	for(auto &WarClanPrefix : m_vWarClanPrefixlist)
		if(str_startswith(pName, WarClanPrefix.c_str()))
			return true;
	for(auto &Entry : m_vWarlist)
		if(std::string(pName) == Entry.first)
			return true;
	return false;
}

bool CWarList::IsTeamlist(const char *pName)
{
	return (std::find(m_vTeamlist.begin(), m_vTeamlist.end(), std::string(pName)) != m_vTeamlist.end());
}

bool CWarList::IsTraitorlist(const char *pName)
{
	for(auto &Entry : m_vTraitorlist)
		if(std::string(pName) == Entry.first)
			return true;
	return false;
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

		if(!File)
			continue;

		char *pLine;
		CLineReader Reader;
		Reader.Init(File);
		// read one line only
		pLine = Reader.Get();
		if(pLine)
			str_copy(pReason, pLine, ReasonSize);

		io_close(File);
		break;
	}
}

bool CWarList::IsWar(const char *pName, const char *pClan)
{
	return IsWarlist(pName) || IsTraitorlist(pName) || IsWarClanlist(pClan);
}

bool CWarList::IsWar(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsWar;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTeam(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsTeam;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTraitor(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!str_comp(pName, m_aWarPlayers[ClientID].m_aName))
	{
		return m_aWarPlayers[ClientID].m_IsTraitor;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsWarClan(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientID].m_aClan))
	{
		return m_aWarPlayers[ClientID].m_IsWarClan;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsTeamClan(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientID].m_aClan))
	{
		return m_aWarPlayers[ClientID].m_IsTeamClan;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

bool CWarList::IsWarClanmate(int ClientID)
{
	const char *pName = m_pClient->m_aClients[ClientID].m_aName;
	const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
	if(!pClan[0])
		return false;
	if(!str_comp(pClan, m_aWarPlayers[ClientID].m_aClan))
	{
		return m_aWarPlayers[ClientID].m_IsWarClanmate;
	}
	str_copy(m_aWarPlayers[ClientID].m_aName, pName, sizeof(m_aWarPlayers[ClientID].m_aName));
	str_copy(m_aWarPlayers[ClientID].m_aClan, pClan, sizeof(m_aWarPlayers[ClientID].m_aClan));
	m_aWarPlayers[ClientID].m_IsWar = IsWarlist(pName);
	m_aWarPlayers[ClientID].m_IsTeam = IsTeamlist(pName);
	m_aWarPlayers[ClientID].m_IsTraitor = IsTraitorlist(pName);
	m_aWarPlayers[ClientID].m_IsWarClan = IsWarClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsTeamClan = IsTeamClanlist(pClan);
	m_aWarPlayers[ClientID].m_IsWarClanmate = IsWarClanmate(pClan);
	return false;
}

void CWarList::SetNameplateColor(int ClientID, STextRenderColor *pColor)
{
	if(!g_Config.m_ClWarList)
		return;

	if(IsWar(ClientID))
	{
		// TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f));
		pColor->Set(7.0f, 0.2f, 0.2f, 1.0f);
	}
	else if(IsTeam(ClientID))
	{
		// TextRender()->TextColor(ColorRGBA(2.0f, 7.0f, 2.0f));
		pColor->Set(0.0f, 0.9f, 0.2f, 1.0f);
	}
	else if(IsTraitor(ClientID))
	{
		// TextRender()->TextColor(ColorRGBA(0.1f, 0.1f, 0.1f));
		pColor->Set(0.1f, 0.1f, 0.1f, 1.0f);
	}
	else if(IsWarClan(ClientID))
	{
		pColor->Set(7.0f, 0.2f, 0.2f, 1.0f);
	}
	else if(IsTeamClan(ClientID))
	{
		pColor->Set(0.0f, 0.9f, 0.2f, 1.0f);
	}
	else if(IsWarClanmate(ClientID))
	{
		pColor->Set(7.0f, 0.5f, 0.2f, 1.0f);
	}
	else
	{
		// TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f));
		pColor->Set(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

int CWarList::LoadWarNames(const char *pDir)
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
	m_WarDirs++;
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war list file '%s'", aFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vWarlist.push_back(Entry);
	}

	io_close(File);
	return 0;
}

int CWarList::LoadTeamNames(const char *pFilename)
{
	if(!Storage())
		return 1;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		// str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", pFilename);
		// Print(aBuf);
		return 0;
	}
	m_TeamDirs++;
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading team list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vTeamlist.push_back(std::string(pLine));
	}

	io_close(File);
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
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading traitor list file '%s'", aFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		std::pair<std::string, std::string> Entry;
		Entry.first = std::string(pLine);
		Entry.second = std::string(pDir);
		m_vTraitorlist.push_back(Entry);
	}

	io_close(File);
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
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war clans list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vWarClanlist.push_back(std::string(pLine));
	}

	io_close(File);
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
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading team clans list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vTeamClanlist.push_back(std::string(pLine));
	}

	io_close(File);
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
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading war clan prefix list file '%s'", pFilename);
	Print(aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_skip_whitespaces(pLine)[0])
			continue;
		m_vWarClanPrefixlist.push_back(std::string(pLine));
	}

	io_close(File);
	return 0;
}

void CWarList::OnConsoleInit()
{
	Console()->Register("warlist", "?s[command]?s[arg1]?s[arg2]", CFGFLAG_CLIENT, ConWarlist, this, "Activate afk mode (auto chat respond)");

	Console()->Chain("cl_war_list", ConchainWarList, this);
}

void CWarList::ConchainWarList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
	{
		pSelf->m_pClient->m_ChillerBotUX.EnableComponent("war list");
		pSelf->ReloadList();
	}
	else
		pSelf->m_pClient->m_ChillerBotUX.DisableComponent("war list");
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
