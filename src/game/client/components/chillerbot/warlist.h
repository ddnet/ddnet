#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H

#include <vector>

#include <game/client/component.h>

class CWarList : public CComponent
{
	struct CWarPlayer
	{
		CWarPlayer()
		{
			m_IsWar = false;
			m_IsTeam = false;
			m_IsTraitor = false;
			m_IsWarClan = false;
			m_IsWarClanmate = false;
			m_aName[0] = '\0';
			m_aClan[0] = '\0';
		}
		bool m_IsWar;
		bool m_IsTeam;
		bool m_IsTraitor;
		bool m_IsWarClan;
		bool m_IsTeamClan;
		bool m_IsWarClanmate;
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
	};

	CWarPlayer m_aWarPlayers[MAX_CLIENTS];
	/*
		m_vWarlist

		pair<PlayerName, FilePath>
	*/
	std::vector<std::pair<std::string, std::string>> m_vWarlist;
	std::vector<std::string> m_vTeamlist;
	/*
		m_vTraitorlist

		pair<PlayerName, FilePath>
	*/
	std::vector<std::pair<std::string, std::string>> m_vTraitorlist;
	std::vector<std::string> m_vWarClanlist;
	std::vector<std::string> m_vTeamClanlist;
	std::vector<std::string> m_vWarClanPrefixlist;
	void GetWarlistPathByName(const char *pName, int Size, char *pPath);
	void GetTraitorlistPathByName(const char *pName, int Size, char *pPath);
	int m_WarDirs;
	int m_TeamDirs;
	int m_TraitorDirs;

	static int LoadWarDir(const char *pImgName, int IsDir, int DirType, void *pUser);
	static int LoadTeamDir(const char *pImgName, int IsDir, int DirType, void *pUser);
	static int LoadTraitorDir(const char *pImgName, int IsDir, int DirType, void *pUser);
	void LoadWarList();
	void LoadTeamList();
	void LoadTraitorList();
	void LoadWarClanList();
	void LoadTeamClanList();
	void LoadWarClanPrefixList();
	int LoadWarNames(const char *pDir);
	int LoadTeamNames(const char *pFilename);
	int LoadTraitorNames(const char *pFilename);
	int LoadWarClanNames(const char *pFilename);
	int LoadTeamClanNames(const char *pFilename);
	int LoadWarClanPrefixNames(const char *pFilename);

	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual void OnInit();

	static void ConWarlist(IConsole::IResult *pResult, void *pUserData);

	static void ConchainWarList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	void GetWarReason(const char *pName, char *pReason, int ReasonSize);

	// non cached used when its about the name and there is no up to date id
	bool IsWar(const char *pName, const char *pClan);
	bool IsWarlist(const char *pName);
	bool IsTeamlist(const char *pName);
	bool IsTraitorlist(const char *pName);
	bool IsWarClanlist(const char *pClan);
	bool IsTeamClanlist(const char *pClan);
	bool IsWarClanmate(const char *pClan);

	// cached use during render
	bool IsWar(int ClientID);
	bool IsTeam(int ClientID);
	bool IsTraitor(int ClientID);
	bool IsWarClan(int ClientID);
	bool IsTeamClan(int ClientID);
	bool IsWarClanmate(int ClientID);
	void SetNameplateColor(int ClientID, STextRenderColor *pColor);
	void ReloadList();
};

#endif
