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
	/*
		m_vTeamlist

		pair<PlayerName, FilePath>
	*/
	std::vector<std::pair<std::string, std::string>> m_vTeamlist;
	/*
		m_vTraitorlist

		pair<PlayerName, FilePath>
	*/
	std::vector<std::pair<std::string, std::string>> m_vTraitorlist;
	/*
		m_vNeutrallist

		pair<PlayerName, FilePath>
	*/
	std::vector<std::pair<std::string, std::string>> m_vNeutrallist;
	std::vector<std::string> m_vWarClanlist;
	std::vector<std::string> m_vTeamClanlist;
	std::vector<std::string> m_vWarClanPrefixlist;
	void GetWarlistPathByNeedle(const char *pSearch, int Size, char *pPath);
	void GetTraitorlistPathByNeedle(const char *pSearch, int Size, char *pPath);
	void GetNeutrallistPathByNeedle(const char *pSearch, int Size, char *pPath);
	void GetWarlistPathByName(const char *pName, int Size, char *pPath);
	void GetTeamlistPathByName(const char *pName, int Size, char *pPath);
	void GetTraitorlistPathByName(const char *pName, int Size, char *pPath);
	void GetNeutrallistPathByName(const char *pName, int Size, char *pPath);
	int m_WarDirs;
	int m_TeamDirs;
	int m_TraitorDirs;

	static int LoadWarDir(const char *pDirname, int IsDir, int DirType, void *pUser);
	static int LoadTeamDir(const char *pDirname, int IsDir, int DirType, void *pUser);
	static int LoadTraitorDir(const char *pDirname, int IsDir, int DirType, void *pUser);
	static int LoadNeutralDir(const char *pDirname, int IsDir, int DirType, void *pUser);
	void LoadWarList();
	void LoadTeamList();
	void LoadTraitorList();
	void LoadNeutralList();
	void LoadWarClanList();
	void LoadTeamClanList();
	void LoadWarClanPrefixList();
	/*
		RemoveWarNameFromVector

		only removes from the m_vWarlist vector
		does not update anything else
	*/
	bool RemoveWarNameFromVector(const char *pDir, const char *pName);
	/*
		RemoveTeamNameFromVector

		only removes from the m_vTeamlist vector
		does not update anything else
	*/
	bool RemoveTeamNameFromVector(const char *pDir, const char *pName);
	bool WriteWarNames(const char *pDir);
	bool WriteTeamNames(const char *pDir);
	int LoadWarNames(const char *pDir);
	int LoadTeamNames(const char *pDir);
	int LoadTraitorNames(const char *pDir);
	int LoadNeutralNames(const char *pDir);
	int LoadWarClanNames(const char *pFilename);
	int LoadTeamClanNames(const char *pFilename);
	int LoadWarClanPrefixNames(const char *pFilename);

	virtual void OnRender() override;
	virtual void OnConsoleInit() override;
	
	virtual void OnInit() override;

	static void ConRemoveTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveWar(IConsole::IResult *pResult, void *pUserData);
	static void ConAddTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConAddWar(IConsole::IResult *pResult, void *pUserData);
	static void ConWarlist(IConsole::IResult *pResult, void *pUserData);

	static void ConchainWarList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	/*
		AddWar

		Add username to warlist file
		Returns true on success
	*/
	bool AddWar(const char *pFolder, const char *pName);

	/*
		AddTeam

		Add username to warlist file
		Returns true on success
	*/
	bool AddTeam(const char *pFolder, const char *pName);

	/*
		SearchName

		Search given name in all folders: war, traitor, neutral, team
		Print all matches

		Return true if found and false otherwise
	*/
	bool SearchName(const char *pName, bool AllowPartialMatch = false, bool Silent = false);

	void Print(const char *pMsg);
	bool m_Verbose;
	int64_t m_NextReload;

public:
	virtual int Sizeof() const override { return sizeof(*this); }

	bool OnChatCmd(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine);

	bool OnChatCmdSimple(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine);
	bool OnChatCmdAdvanced(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine);



	void RemoveWarNoMsg(const char *pName);
	void RemoveTeamNoMsg(const char *pName);

	void AddSimpleWar(const char *pName);
	void RemoveSimpleWar(const char *pName);

	void AddSimpleTeam(const char *pName);
	void RemoveSimpleTeam(const char *pName);

	void GetWarReason(const char *pName, char *pReason, int ReasonSize);
	void GetWarClansStr(char *pBuf, int Size);

	// non cached used when its about the name and there is no up to date id
	bool IsWar(const char *pName, const char *pClan);
	bool IsWarlist(const char *pName);
	bool IsTeamlist(const char *pName);
	bool IsTraitorlist(const char *pName);
	bool IsWarClanlist(const char *pClan);
	bool IsTeamClanlist(const char *pClan);
	bool IsWarClanmate(const char *pClan);

	// cached use during render

	bool IsWar(int ClientId);
	bool IsTeam(int ClientId);
	bool IsTraitor(int ClientId);
	bool IsWarClan(int ClientId);
	bool IsTeamClan(int ClientId);
	bool IsWarClanmate(int ClientId);
	void SetNameplateColor(int ClientId, ColorRGBA *pColor);
	void ReloadList();

	int NumEnemies() { return m_WarDirs + m_TraitorDirs; }
	int NumTeam() { return m_TeamDirs; };
};

#endif
