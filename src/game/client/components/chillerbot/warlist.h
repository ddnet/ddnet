#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H

#include <vector>

#include <game/client/component.h>

class CWarList : public CComponent
{
	struct CWarPlayer
	{
		bool m_IsWar;
		bool m_IsTeam;
		bool m_IsTraitor;
		bool m_IsWarClan;
		bool m_IsTeamClan;
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
	};

	CWarPlayer m_aWarPlayers[MAX_CLIENTS];
	std::vector<std::string> m_vWarlist;
	std::vector<std::string> m_vTeamlist;
	std::vector<std::string> m_vTraitorlist;
	std::vector<std::string> m_vWarClanlist;
	std::vector<std::string> m_vTeamClanlist;
	std::vector<std::string> m_vWarClanPrefixlist;
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
	int LoadWarNames(const char *pFilename);
	int LoadTeamNames(const char *pFilename);
	int LoadTraitorNames(const char *pFilename);
	int LoadWarClanNames(const char *pFilename);
	int LoadTeamClanNames(const char *pFilename);
	int LoadWarClanPrefixNames(const char *pFilename);
	bool IsWarlist(const char *pName);
	bool IsTeamlist(const char *pName);
	bool IsTraitorlist(const char *pName);
	bool IsWarClanlist(const char *pClan);
	bool IsTeamClanlist(const char *pClan);

	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual void OnInit();

	static void ConWarlist(IConsole::IResult *pResult, void *pUserData);

	static void ConchainWarList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	bool IsWar(int ClientID);
	bool IsTeam(int ClientID);
	bool IsTraitor(int ClientID);
	bool IsWarClan(int ClientID);
	bool IsTeamClan(int ClientID);
	void SetNameplateColor(int ClientID, STextRenderColor *pColor);
	void ReloadList();
};

#endif
