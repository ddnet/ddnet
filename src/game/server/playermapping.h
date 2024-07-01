#ifndef GAME_SERVER_PLAYERMAPPING_H
#define GAME_SERVER_PLAYERMAPPING_H

#include <engine/shared/protocol.h>

#include <generated/protocol7.h>

#include <game/teamscore.h>

class CGameContext;
class CPlayer;

// The teeworlds 0.7 client and older ddnet clients
// only support up to 64 clients. And also only up to 64 client ids (0-63).
// The CPlayerMapping class manages a mapping of real server internal
// client ids in the range of 0-127 and the fake client ids send to clients
// in the range of 0-63.
//
// If a client does not support 128 slots the server will only show the 63 (64-1 for empty client for chat)
// closest players (that closest selection currently tries to minimize changes by only changing when really required)
// to the client. Because these 64 closest players are not
// guaranteed to have client ids in the range of 0-63 we need to maintain a
// fake client id list for every old client.
//
// Additionally there is the "See Others" feature which lets you cycle through a list of
// all players manually in pause or spectator mode by holding right shift (+spectate).

class CPlayerMapping
{
	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	// Number of players per page for see others feature in +spectate
	static constexpr int ms_MaxNumSeeOthers = 34;
	// Teams are messy. Dont highlight teams bigger than 10 tees in playermapping so that big teams wont break anything
	static constexpr int ms_MaxTeamSizePlayerMap = 10;

	int m_aTeamSizes[NUM_DDRACE_TEAMS];
	char m_aSeeOthersName[MAX_NAME_LENGTH];

	class CPlayerMap
	{
	public:
		void Init(int ClientId, CPlayerMapping *pPlayerMapping);
		void InitPlayer(bool Timeout);
		CPlayerMapping *m_pPlayerMapping;
		CPlayer *Player() const;
		int m_ClientId;
		int m_NumReserved;
		bool m_UpdateTeamsState;
		bool m_aReserved[MAX_CLIENTS];
		bool m_ResortReserved;
		int *m_pMap;
		int *m_pReverseMap;
		void Update();
		void Add(int MapId, int ClientId);
		int Remove(int MapId);
		void InsertNextEmpty(int ClientId);
		int MapSize() const { return LEGACY_MAX_CLIENTS - m_NumReserved; }
		// See others
		int m_SeeOthersPage;
		int m_TotalOverhang;
		int m_NumPages;
		int m_NumSeeOthers;
		bool m_aWasSeeOthers[MAX_CLIENTS];
		bool m_DoSeeOthersByVote;
		void DoSeeOthers();
		void CycleSeeOthers();
		void UpdateSeeOthers() const;
		void ResetSeeOthers();
	} m_aMap[MAX_CLIENTS];
	void UpdatePlayerMap(int ClientId);

public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IServer *Server() { return m_pServer; }

	void Init(CGameContext *pGameServer);
	void Tick();

	void InitPlayerMap(int ClientId, bool Timeout = false) { m_aMap[ClientId].InitPlayer(Timeout); }
	void UpdateTeamsState(int ClientId) { m_aMap[ClientId].m_UpdateTeamsState = true; }
	void ForceInsertPlayer(int Insert, int ClientId) { m_aMap[ClientId].InsertNextEmpty(Insert); }

	enum class ESeeOthersInd
	{
		NONE = -1,
		PLAYER = 0,
		BUTTON = 1,
	};
	int SeeOthersId() const;
	bool DoSeeOthers(int ClientId, int SelectedId, bool DoByVote = false);
	void ResetSeeOthers(int ClientId);
	int TotalOverhang(int ClientId) const;
	ESeeOthersInd SeeOthersInd(int ClientId, int MapId) const;
	const char *SeeOthersName(int ClientId);
	bool ReserveTeamSlots(int DDTeam);
};

#endif
