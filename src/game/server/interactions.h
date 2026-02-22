#ifndef GAME_SERVER_INTERACTIONS_H
#define GAME_SERVER_INTERACTIONS_H

#include <engine/shared/protocol.h>

class CGameContext;
class CPlayer;
class CCharacter;

class CInteractions
{
	const CPlayer *GetPlayer(const CGameContext *pGameServer, int ClientId) const;
	const CCharacter *Character(const CGameContext *pGameServer, int ClientId) const;
	bool IsSolo(const CGameContext *pGameServer, int ClientId) const;
	int GetDDRaceTeam(const CGameContext *pGameServer, int ClientId) const;

	int m_OwnerId = 0;
	uint32_t m_UniqueOwnerId = 0;
	bool m_OwnerAlive = false;
	int m_DDRaceTeam = 0;
	bool m_Solo = false;
	bool m_NoHitOthers = false;
	bool m_NoHitSelf = false;

public:
	void Init(int OwnerId, uint32_t UniqueOwnerId);
	void FillOwnerConnected(
		bool OwnerAlive,
		int DDRaceTeam,
		bool Solo,
		bool NoHitOthers,
		bool NoHitSelf);
	void FillOwnerDisconnected();
	bool CanSee(const CGameContext *pGameServer, int ClientId) const;
	bool CanHit(const CGameContext *pGameServer, int ClientId) const;
	CClientMask CanSeeMask(const CGameContext *pGameServer) const;
	CClientMask CanHitMask(const CGameContext *pGameServer) const;
};

#endif
