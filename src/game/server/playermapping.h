// made by fokkonaut

#ifndef GAME_SERVER_PLAYERMAPPING_H
#define GAME_SERVER_PLAYERMAPPING_H

class CGameContext;
class CPlayer;

class CPlayerMapping
{
	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	struct CPlayerMap
	{
		enum class ESeeOthers
		{
			STATE_NONE = -1,
			STATE_PAGE_FIRST,
			STATE_PAGE_SECOND,

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
		SEE_OTHERS_IND_PLAYER,
		SEE_OTHERS_IND_BUTTON,

		SPEC_SELECT_FLAG_RED = LEGACY_MAX_CLIENTS - protocol7::SPEC_FLAGRED,
		SPEC_SELECT_FLAG_BLUE = LEGACY_MAX_CLIENTS - protocol7::SPEC_FLAGBLUE,
	};
	int GetSeeOthersId(int ClientId) const;
	void DoSeeOthers(int ClientId);
	void ResetSeeOthers(int ClientId);
	int GetTotalOverhang(int ClientId) const;
	int GetSeeOthersInd(int ClientId, int MapId) const;
	const char *GetSeeOthersName(int ClientId, char (&aName)[MAX_NAME_LENGTH]) const;
};

#endif
