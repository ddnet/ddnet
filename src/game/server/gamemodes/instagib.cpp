#include <game/server/entities/character.h>
#include <game/server/player.h>

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

void CGameControllerInstagib::OnCharacterSpawn(class CCharacter *pChr)
{
	pChr->SetTeams(&Teams());
	Teams().OnCharacterSpawn(pChr->GetPlayer()->GetCID());

	// default health
	pChr->IncreaseHealth(10);

	pChr->SetTeleports(&m_TeleOuts, &m_TeleCheckOuts);
}
