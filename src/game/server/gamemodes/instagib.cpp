#include "instagib.h"

CGameControllerInstagib::CGameControllerInstagib(class CGameContext *pGameServer) :
	CGameControllerDDRace(pGameServer)
{
	m_pGameType = "instagib";

	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;
}

CGameControllerInstagib::~CGameControllerInstagib() = default;

void CGameControllerInstagib::Tick()
{
	CGameControllerDDRace::Tick();

	if(Config()->m_SvPlayerReadyMode && GameServer()->m_World.m_Paused)
		if(Server()->Tick() % Server()->TickSpeed() * 5 == 0)
			GameServer()->PlayerReadyStateBroadcast();
}
