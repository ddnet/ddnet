#ifndef GAME_SERVER_INTERACTIONS_H
#define GAME_SERVER_INTERACTIONS_H

#include <engine/shared/protocol.h>

class CInteractions
{
	class CGameContext *m_pGameServer;
	class CGameContext *GameServer();
	class CPlayer *GetPlayer(int ClientId);
	class CCharacter *Character(int ClientId);
	bool IsSolo(int ClientId);
	int GetDDRaceTeam(int ClientId);

	int m_OwnerId = 0;
	uint32_t m_UniqueOwnerId = 0;
	bool m_OwnerAlive = false;
	int m_DDRaceTeam = 0;
	bool m_Solo = false;
	bool m_NoHitOthers = false;
	bool m_NoHitSelf = false;

public:
	void Init(class CGameContext *pGameServer, int OwnerId, uint32_t UniqueOwnerId);
	void FillOwnerConnected(
		bool OwnerAlive,
		int DDRaceTeam,
		bool Solo,
		bool NoHitOthers,
		bool NoHitSelf);
	void FillOwnerDisconnected();
	bool CanSee(int ClientId);
	bool CanHit(int ClientId);
	CClientMask CanSeeMask();
	CClientMask CanHitMask();
};

#endif
