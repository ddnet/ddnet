#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_ICHATHELPER_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_ICHATHELPER_H

class FWarList
{
public:
	// non cached used when its about the name and there is no up to date id
	bool IsWar(const char *pName, const char *pClan) { return false; }
	bool IsWarlist(const char *pName) { return false; }
	bool IsTeamlist(const char *pName) { return false; }
	bool IsTraitorlist(const char *pName) { return false; }
	bool IsWarClanlist(const char *pClan) { return false; }
	bool IsTeamClanlist(const char *pClan) { return false; }
	bool IsWarClanmate(const char *pClan) { return false; }

	// cached use during render
	bool IsWar(int ClientID) { return false; }
	bool IsTeam(int ClientID) { return false; }
	bool IsTraitor(int ClientID) { return false; }
	bool IsWarClan(int ClientID) { return false; }
	bool IsTeamClan(int ClientID) { return false; }
	bool IsWarClanmate(int ClientID) { return false; }

	int NumEnemies() { return 0; }
	int NumTeam() { return 0; };
};

class FUXGameClient
{
public:
    FWarList m_WarList;
};

class FChatHelper
{
public:
    FUXGameClient *m_pGameClient;
    FUXGameClient *GameClient() { return m_pGameClient; }
};

#endif
