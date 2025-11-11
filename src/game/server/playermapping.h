// made by fokkonaut

#ifndef GAME_SERVER_PLAYERMAPPING_H
#define GAME_SERVER_PLAYERMAPPING_H

#include <engine/shared/protocol.h>

#include <generated/protocol7.h>

class CGameContext;
class CPlayer;

// The teeworlds 0.7 client and older ddnet clients
// only support up to 64 clients. And also only up to 64 client ids (0-63).
// The CPlayerMapping class manages a mapping of real server internal
// client ids in the range of 0-127 and the fake client ids send to clients
// in the range of 0-63.
//
// If a client does not support 128 slots the server will only show the 64
// closest players (that closest selection is currently not perfect)
// to the client. Because these 64 closest players are not
// guaranteed to have client ids in the range of 0-63 we need to maintain a
// fake client id list for every old client.
class CPlayerMapping
{
	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	class CPlayerMap
	{
	public:
		enum class ESeeOthers
		{
			STATE_NONE = -1,
			STATE_PAGE_FIRST = 0,
			STATE_PAGE_SECOND = 1,

			MAX_NUM_SEE_OTHERS = 34,
		};

		void Init(int ClientId, CPlayerMapping *pPlayerMapping);
		void InitPlayer(bool Rejoin);
		CPlayerMapping *m_pPlayerMapping;
		CPlayer *GetPlayer() const;
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
		int GetMapSize() const { return LEGACY_MAX_CLIENTS - m_NumReserved; }
		// See others
		int m_SeeOthersState;
		int m_TotalOverhang;
		int m_NumSeeOthers;
		bool m_aWasSeeOthers[MAX_CLIENTS];
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

	void InitPlayerMap(int ClientId, bool Rejoin = false) { m_aMap[ClientId].InitPlayer(Rejoin); }
	void UpdateTeamsState(int ClientId) { m_aMap[ClientId].m_UpdateTeamsState = true; }
	void ForceInsertPlayer(int Insert, int ClientId) { m_aMap[ClientId].InsertNextEmpty(Insert); }

	enum
	{
		SEE_OTHERS_IND_NONE = -1,
		SEE_OTHERS_IND_PLAYER = 0,
		SEE_OTHERS_IND_BUTTON = 1,

		SPEC_SELECT_FLAG_RED = LEGACY_MAX_CLIENTS - (int)protocol7::SPEC_FLAGRED,
		SPEC_SELECT_FLAG_BLUE = LEGACY_MAX_CLIENTS - (int)protocol7::SPEC_FLAGBLUE,
	};
	int GetSeeOthersId(int ClientId) const;
	void DoSeeOthers(int ClientId);
	void ResetSeeOthers(int ClientId);
	int GetTotalOverhang(int ClientId) const;
	int GetSeeOthersInd(int ClientId, int MapId) const;
	const char *GetSeeOthersName(int ClientId, char (&aName)[MAX_NAME_LENGTH]) const;
};

#endif
