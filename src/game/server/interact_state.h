#ifndef GAME_SERVER_INTERACT_STATE_H
#define GAME_SERVER_INTERACT_STATE_H

#include <engine/shared/protocol.h>

class CInteractState
{
	class CGameContext *m_pGameServer;
	class CGameContext *GameServer();
	class CPlayer *GetPlayer(int ClientId);
	class CCharacter *Character(int ClientId);
	bool IsSolo(int ClientId);
	int GetDDRaceTeam(int ClientId);

	int m_OwnerId = 0;
	int m_UniqueOwnerId = 0;
	bool m_OwnerAlive = false;
	int m_DDRaceTeam = 0;
	bool m_Solo = false;
	bool m_NoHit = false;

public:
	void Init(class CGameContext *pGameServer, int OwnerId, int UniqueOwnerId);
	void FillOwnerConnected(
		bool OwnerAlive,
		int DDRaceTeam,
		bool Solo,
		bool NoHit);
	void FillOwnerDisconnected();
	bool CanSee(int ClientId, class CGameContext *pGameServer) const;
	bool CanHit(int ClientId, class CGameContext *pGameServer) const;
	CClientMask CanSeeMask();
	CClientMask CanHitMask();
};

#endif
