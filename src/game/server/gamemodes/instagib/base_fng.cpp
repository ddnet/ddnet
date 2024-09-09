#include <base/system.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/mapitems_insta.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/instagib/base_instagib.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_fng.h"

CGameControllerBaseFng::CGameControllerBaseFng(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
}

CGameControllerBaseFng::~CGameControllerBaseFng() = default;

int CGameControllerBaseFng::GameInfoExFlags(int SnappingClient)
{
	int Flags = CGameControllerPvp::GameInfoExFlags(SnappingClient);
	Flags &= ~(GAMEINFOFLAG_ENTITIES_DDNET);
	Flags &= ~(GAMEINFOFLAG_ENTITIES_DDRACE);
	Flags &= ~(GAMEINFOFLAG_ENTITIES_RACE);
	Flags |= GAMEINFOFLAG_ENTITIES_FNG;
	return Flags;
}

void CGameControllerBaseFng::Tick()
{
	CGameControllerInstagib::Tick();

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		CCharacter *pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
			continue;

		int HookedId = pChr->Core()->HookedPlayer();
		if(HookedId >= 0 && HookedId < MAX_CLIENTS)
		{
			CPlayer *pHooked = GameServer()->m_apPlayers[HookedId];
			if(pHooked)
			{
				pHooked->UpdateLastToucher(pChr->GetPlayer()->GetCid());
			}
		}
	}
}

void CGameControllerBaseFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstagib::OnCharacterSpawn(pChr);

	pChr->GiveWeapon(WEAPON_HAMMER, false, -1);
}

int CGameControllerBaseFng::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponId);
	DoWincheckRound();
	return 0;
}

bool CGameControllerBaseFng::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

void CGameControllerBaseFng::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	// //Sensitivity
	// int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	// int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	// int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	// int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	// int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	// int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	// int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	// int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	// int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	// int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	// int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	// int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);

	if(((TileIndex == TILE_SPIKE_RED) || (TileFIndex == TILE_SPIKE_RED)))
		OnSpike(pChr, TILE_SPIKE_RED);
	else if(((TileIndex == TILE_SPIKE_BLUE) || (TileFIndex == TILE_SPIKE_BLUE)))
		OnSpike(pChr, TILE_SPIKE_BLUE);
	else if(((TileIndex == TILE_SPIKE_NEUTRAL) || (TileFIndex == TILE_SPIKE_NEUTRAL)))
		OnSpike(pChr, TILE_SPIKE_NEUTRAL);
	else if(((TileIndex == TILE_SPIKE_GOLD) || (TileFIndex == TILE_SPIKE_GOLD)))
		OnSpike(pChr, TILE_SPIKE_GOLD);

	CGameControllerDDRace::HandleCharacterTiles(pChr, MapIndex);
}

void CGameControllerBaseFng::OnSpike(class CCharacter *pChr, int SpikeTile)
{
	if(!pChr->m_FreezeTime)
	{
		pChr->Die(pChr->GetPlayer()->GetCid(), WEAPON_WORLD);
		return;
	}

	CPlayer *pKiller = nullptr;
	int LastToucherId = pChr->GetPlayer()->m_LastToucherId;
	if(LastToucherId >= 0 && LastToucherId < MAX_CLIENTS)
		pKiller = GameServer()->m_apPlayers[LastToucherId];

	if(pKiller)
	{
		// all scores are +1
		// from the kill it self

		if(SpikeTile == TILE_SPIKE_NEUTRAL)
			pKiller->AddScore(2);
		if(SpikeTile == TILE_SPIKE_GOLD)
			pKiller->AddScore(7);
		if(SpikeTile == TILE_SPIKE_RED)
		{
			if(pKiller->GetTeam() == TEAM_RED || !IsTeamPlay())
				pKiller->AddScore(4);
			else
				pKiller->AddScore(-6);
		}
		if(SpikeTile == TILE_SPIKE_BLUE)
		{
			if(pKiller->GetTeam() == TEAM_BLUE || !IsTeamPlay())
				pKiller->AddScore(4);
			else
				pKiller->AddScore(-6);
		}

		DoWincheckRound();
	}

	pChr->Die(LastToucherId, WEAPON_NINJA);
}

bool CGameControllerBaseFng::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	Character.GetPlayer()->UpdateLastToucher(From);

	if(Character.m_IsGodmode)
		return true;
	if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCid(), From))
		return false;
	CPlayer *pKiller = nullptr;
	if(From >= 0 && From <= MAX_CLIENTS)
		pKiller = GameServer()->m_apPlayers[From];
	if(g_Config.m_SvOnlyHookKills && pKiller)
	{
		CCharacter *pChr = pKiller->GetCharacter();
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

	if(pKiller)
	{
		pKiller->IncrementScore();
		DoWincheckRound();
	}

	Character.Freeze(10);
	return false;
}

bool CGameControllerBaseFng::OnFireWeapon(CCharacter &Character, int &Weapon, vec2 &Direction, vec2 &MouseTarget, vec2 &ProjStartPos)
{
	if(Weapon == WEAPON_HAMMER)
		if(Character.OnFngFireWeapon(Character, Weapon, Direction, MouseTarget, ProjStartPos))
			return true;
	return CGameControllerInstagib::OnFireWeapon(Character, Weapon, Direction, MouseTarget, ProjStartPos);
}

void CGameControllerBaseFng::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);
}
