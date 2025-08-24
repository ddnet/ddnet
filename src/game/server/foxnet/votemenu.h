#ifndef GAME_SERVER_FOXNET_VOTEMENU_H
#define GAME_SERVER_FOXNET_VOTEMENU_H

#include <game/generated/protocol.h>

class CGameContext;
class IServer;

struct CVoteMenu
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

public:
	void Init(CGameContext *pGameServer);

	bool OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId);

};

#endif // GAME_SERVER_FOXNET_VOTEMENU_H