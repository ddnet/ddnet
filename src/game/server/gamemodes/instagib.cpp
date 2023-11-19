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
}
