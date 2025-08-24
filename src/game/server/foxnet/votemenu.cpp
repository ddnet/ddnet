#include "votemenu.h"
#include "../gamecontext.h"
#include <base/system.h>
#include <engine/server.h>

IServer *CVoteMenu::Server() const { return GameServer()->Server(); }

void CVoteMenu::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

bool CVoteMenu::OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId)
{
	if(str_comp_nocase(pMsg->m_pType, "option") != 0)
		return false;
}