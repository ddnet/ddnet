#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_fng.h"

CGameControllerBaseFng::CGameControllerBaseFng(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
}

CGameControllerBaseFng::~CGameControllerBaseFng() = default;

void CGameControllerBaseFng::Tick()
{
	CGameControllerInstagib::Tick();
}

void CGameControllerBaseFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstagib::OnCharacterSpawn(pChr);
}

int CGameControllerBaseFng::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponId);
	return 0;
}

bool CGameControllerBaseFng::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

bool CGameControllerBaseFng::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(Character.m_IsGodmode)
		return true;
	if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCid(), From))
		return false;
	if(g_Config.m_SvOnlyHookKills && From >= 0 && From <= MAX_CLIENTS)
	{
		CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
		if(!pChr || pChr->GetCore().HookedPlayer() != Character.GetPlayer()->GetCid())
			return false;
	}

	// no self damage
	if(From == Character.GetPlayer()->GetCid())
		return false;

	if(Character.m_FreezeTime)
	{
		Dmg = 0;
		return false;
	}

	Character.Freeze(10);
	return false;
}

void CGameControllerBaseFng::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);
}
