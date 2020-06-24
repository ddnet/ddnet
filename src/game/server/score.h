#ifndef GAME_SERVER_SCORE_H
#define GAME_SERVER_SCORE_H

#include <memory>
#include <atomic>

#include <game/voting.h>
#include <engine/map.h>
#include "save.h"

enum
{
	NUM_CHECKPOINTS = 25,
	TIMESTAMP_STR_LENGTH = 20, // 2019-04-02 19:38:36
};

struct CScorePlayerResult
{
	std::atomic_bool m_Done;
	CScorePlayerResult();

	enum {
		MAX_MESSAGES = 7,
	};

	enum Variant
	{
		DIRECT,
		ALL,
		BROADCAST,
		MAP_VOTE,
		PLAYER_INFO,
	} m_MessageKind;
	union {
		char m_aaMessages[MAX_MESSAGES][512];
		char m_Broadcast[1024];
		struct {
			float m_Time;
			float m_CpTime[NUM_CHECKPOINTS];
			int m_Score;
			int m_HasFinishScore;
			int m_Birthday; // 0 indicates no birthday
		} m_Info;
		struct
		{
			char m_Reason[VOTE_REASON_LENGTH];
			char m_Server[32+1];
			char m_Map[MAX_MAP_LENGTH+1];
		} m_MapVote;
	} m_Data; // PLAYER_INFO

	void SetVariant(Variant v);
};

struct CScoreRandomMapResult
{
	std::atomic_bool m_Done;
	CScoreRandomMapResult(int ClientID) :
		m_Done(false),
		m_ClientID(ClientID)
	{
		m_Map[0] = '\0';
		m_aMessage[0] = '\0';
	}
	int m_ClientID;
	char m_Map[MAX_MAP_LENGTH];
	char m_aMessage[512];
};

struct CScoreSaveResult
{
	CScoreSaveResult(int PlayerID, IGameController* Controller) :
		m_Status(SAVE_FAILED),
		m_SavedTeam(CSaveTeam(Controller)),
		m_RequestingPlayer(PlayerID)
	{
		m_aMessage[0] = '\0';
		m_aBroadcast[0] = '\0';
	}
	enum
	{
		SAVE_SUCCESS,
		// load team in the following two cases
		SAVE_FAILED,
		LOAD_SUCCESS,
		LOAD_FAILED,
	} m_Status;
	char m_aMessage[512];
	char m_aBroadcast[512];
	CSaveTeam m_SavedTeam;
	int m_RequestingPlayer;
	CUuid m_SaveID;
};

struct CScoreInitResult
{
	CScoreInitResult() :
		m_Done(false),
		m_CurrentRecord(0)
	{ }
	std::atomic_bool m_Done;
	float m_CurrentRecord;
};

class CPlayerData
{
public:
	CPlayerData()
	{
		Reset();
	}
	~CPlayerData() {}

	void Reset()
	{
		m_BestTime = 0;
		m_CurrentTime = 0;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_aBestCpTime[i] = 0;
	}

	void Set(float Time, float CpTime[NUM_CHECKPOINTS])
	{
		m_BestTime = Time;
		m_CurrentTime = Time;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_aBestCpTime[i] = CpTime[i];
	}

	float m_BestTime;
	float m_CurrentTime;
	float m_aBestCpTime[NUM_CHECKPOINTS];
};

class IScore
{
	CPlayerData m_aPlayerData[MAX_CLIENTS];

public:
	virtual ~IScore() {}

	CPlayerData *PlayerData(int ID) { return &m_aPlayerData[ID]; }

	virtual void MapInfo(int ClientID, const char *pMapName) = 0;
	virtual void MapVote(int ClientID, const char *pMapName) = 0;
	virtual void LoadPlayerData(int ClientID) = 0;
	virtual void SaveScore(int ClientID, float Time, const char *pTimestamp, float aCpTime[NUM_CHECKPOINTS], bool NotEligible) = 0;

	virtual void SaveTeamScore(int *pClientIDs, unsigned int Size, float Time, const char *pTimestamp) = 0;

	virtual void ShowTop5(int ClientID, int Offset=1) = 0;
	virtual void ShowRank(int ClientID, const char *pName) = 0;

	virtual void ShowTeamTop5(int ClientID, int Offset=1) = 0;
	virtual void ShowTeamRank(int ClientID, const char *pName) = 0;

	virtual void ShowTopPoints(int ClientID, int Offset=1) = 0;
	virtual void ShowPoints(int ClientID, const char *pName) = 0;

	virtual void ShowTimes(int ClientID, const char *pName, int Offset = 1) = 0;
	virtual void ShowTimes(int ClientID, int Offset = 1) = 0;

	virtual void RandomMap(int ClientID, int Stars) = 0;
	virtual void RandomUnfinishedMap(int ClientID, int Stars) = 0;

	virtual void SaveTeam(int ClientID, const char *pCode, const char *pServer) = 0;
	virtual void LoadTeam(const char *pCode, int ClientID) = 0;
	virtual void GetSaves(int ClientID) = 0;

	// called when the server is shut down but not on mapchange/reload
	virtual void OnShutdown() = 0;
};

#endif // GAME_SERVER_SCORE_H
