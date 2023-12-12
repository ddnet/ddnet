#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "zcatch.h"

CGameControllerZcatch::CGameControllerZcatch(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
	m_GameFlags = GAMEFLAG_FLAGS;
	m_pGameType = "zCatch";
}

CGameControllerZcatch::~CGameControllerZcatch() = default;

void CGameControllerZcatch::Tick()
{
	CGameControllerInstagib::Tick();
}

void CGameControllerZcatch::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstagib::OnCharacterSpawn(pChr);

	SetSpawnWeapons(pChr);
}

void CGameControllerZcatch::OnCaught(class CPlayer *pVictim, class CPlayer *pKiller)
{
	if(pVictim->GetCID() == pKiller->GetCID())
		return;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "You are spectator until '%s' dies", Server()->ClientName(pKiller->GetCID()));
	GameServer()->SendChatTarget(pVictim->GetCID(), aBuf);

	pVictim->SetTeamRaw(TEAM_SPECTATORS);
	pVictim->m_IsDead = true;
	pVictim->m_KillerID = pKiller->GetCID();
}

int CGameControllerZcatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponID);

	// TODO: revisit this edge case when zcatch is done
	//       a killer leaving while the bullet is flying
	if(!pKiller)
		return 0;

	OnCaught(pVictim->GetPlayer(), pKiller);

	char aBuf[512];
	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(pPlayer->m_KillerID == -1)
			continue;
		if(pPlayer->GetCID() == pVictim->GetPlayer()->GetCID())
			continue;
		if(pPlayer->m_KillerID != pVictim->GetPlayer()->GetCID())
			continue;

		str_format(aBuf, sizeof(aBuf), "You respawned because '%s' died", Server()->ClientName(pVictim->GetPlayer()->GetCID()));
		GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
		pPlayer->m_IsDead = false;
		pPlayer->SetTeamRaw(TEAM_RED);
	}

	return 0;
}

bool CGameControllerZcatch::CanJoinTeam(int Team, int NotThisID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[NotThisID];
	if(!pPlayer)
		return false;

	return !pPlayer->m_IsDead || Team == TEAM_SPECTATORS;
}

bool CGameControllerZcatch::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

void CGameControllerZcatch::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);
}
