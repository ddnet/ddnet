/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* copyright (c) 2008 rajh and gregwar. Score stuff */
#ifndef GAME_SERVER_FILESCORE_H
#define GAME_SERVER_FILESCORE_H

#include <base/tl/sorted_array.h>

#include "../score.h"

class CFileScore: public IScore
{
	CGameContext *m_pGameServer;
	IServer *m_pServer;

	class CPlayerScore
	{
	public:
		char m_aName[MAX_NAME_LENGTH];
		float m_Score;
		float m_aCpTime[NUM_CHECKPOINTS];

		CPlayerScore()
		{
		}
		;
		CPlayerScore(const char *pName, float Score,
				float aCpTime[NUM_CHECKPOINTS]);

		bool operator<(const CPlayerScore& other)
		{
			return (this->m_Score < other.m_Score);
		}
	};

	sorted_array<CPlayerScore> m_Top;

	CGameContext *GameServer()
	{
		return m_pGameServer;
	}
	IServer *Server()
	{
		return m_pServer;
	}

	CPlayerScore *SearchScore(int ID, int *pPosition)
	{
		return SearchName(Server()->ClientName(ID), pPosition, 0);
	}
	;

	CPlayerScore *SearchName(const char *pName, int *pPosition, bool MatchCase);
	void UpdatePlayer(int ID, float Score, float aCpTime[NUM_CHECKPOINTS]);

	void Init();
	void Save();
	static void SaveScoreThread(void *pUser);

public:

	CFileScore(CGameContext *pGameServer);
	~CFileScore();

	virtual void LoadScore(int ClientID);
	virtual void MapInfo(int ClientID, const char* MapName);
	virtual void MapVote(int ClientID, const char* MapName);
	virtual void SaveScore(int ClientID, float Time,
			float CpTime[NUM_CHECKPOINTS]);
	virtual void SaveTeamScore(int* ClientIDs, unsigned int Size, float Time);

	virtual void ShowTop5(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	virtual void ShowRank(int ClientID, const char* pName, bool Search = false);

	virtual void ShowTeamTop5(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	virtual void ShowTeamRank(int ClientID, const char* pName, bool Search = false);

	virtual void ShowTopPoints(IConsole::IResult *pResult, int ClientID, void *pUserData, int Debut);
	virtual void ShowPoints(int ClientID, const char* pName, bool Search);
	virtual void RandomMap(int ClientID, int stars);
	virtual void RandomUnfinishedMap(int ClientID, int stars);
	virtual void SaveTeam(int Team, const char* Code, int ClientID, const char* Server);
	virtual void LoadTeam(const char* Code, int ClientID);
};

#endif
