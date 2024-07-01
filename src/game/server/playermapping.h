// made by fokkonaut

#ifndef GAME_SERVER_PLAYERMAPPING_H
#define GAME_SERVER_PLAYERMAPPING_H

#include <engine/shared/config.h>
#include <game/server/gameworld.h>

class CGameContext;
class CPlayer;

class CPlayerMapping
{
	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	struct PlayerMap
	{
		enum SSeeOthers {
			STATE_NONE = -1,
			STATE_PAGE_FIRST,
			STATE_PAGE_SECOND,

			MAX_NUM_SEE_OTHERS = 34,
		};

		void Init(int ClientID, CPlayerMapping *pPlayerMapping);
		void InitPlayer(bool Rejoin);
		CPlayerMapping *m_pPlayerMapping;
		CPlayer *GetPlayer();
		int m_ClientID;
		int m_NumReserved;
		bool m_UpdateTeamsState;
		bool m_aReserved[MAX_CLIENTS];
		bool m_ResortReserved;
		int *m_pMap;
		int *m_pReverseMap;
		void Update();
		void Add(int MapID, int ClientID);
		int Remove(int MapID);
		void InsertNextEmpty(int ClientID);
		int GetMapSize() { return LEGACY_MAX_CLIENTS - m_NumReserved; }
		// See others
		int m_SeeOthersState;
		int m_TotalOverhang;
		int m_NumSeeOthers;
		bool m_aWasSeeOthers[MAX_CLIENTS];
		void DoSeeOthers();
		void CycleSeeOthers();
		void UpdateSeeOthers();
		void ResetSeeOthers();
	} m_aMap[MAX_CLIENTS];
	void UpdatePlayerMap(int ClientID);

public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IServer *Server() { return m_pServer; }

	void Init(CGameContext *pGameServer);
	void Tick();

	void InitPlayerMap(int ClientID, bool Rejoin = false) { m_aMap[ClientID].InitPlayer(Rejoin); }
	void UpdateTeamsState(int ClientID) { m_aMap[ClientID].m_UpdateTeamsState = true; }
	void ForceInsertPlayer(int Insert, int ClientID) { m_aMap[ClientID].InsertNextEmpty(Insert); }

	enum
	{
		SEE_OTHERS_IND_NONE = -1,
		SEE_OTHERS_IND_PLAYER,
		SEE_OTHERS_IND_BUTTON,

		SPEC_SELECT_FLAG_RED = LEGACY_MAX_CLIENTS - protocol7::SPEC_FLAGRED,
		SPEC_SELECT_FLAG_BLUE = LEGACY_MAX_CLIENTS - protocol7::SPEC_FLAGBLUE,
	};
	int GetSeeOthersID(int ClientID);
	void DoSeeOthers(int ClientID);
	void ResetSeeOthers(int ClientID);
	int GetTotalOverhang(int ClientID);
	int GetSeeOthersInd(int ClientID, int MapID);
	const char *GetSeeOthersName(int ClientID);
};

#endif
