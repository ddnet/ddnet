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

		if(pChr->IsTouchingTile(TILE_FNG_SPIKE_RED))
			OnSpike(pChr, TILE_FNG_SPIKE_RED);
		if(pChr->IsTouchingTile(TILE_FNG_SPIKE_BLUE))
			OnSpike(pChr, TILE_FNG_SPIKE_BLUE);
		if(pChr->IsTouchingTile(TILE_FNG_SPIKE_NORMAL))
			OnSpike(pChr, TILE_FNG_SPIKE_NORMAL);
		if(pChr->IsTouchingTile(TILE_FNG_SPIKE_GOLD))
			OnSpike(pChr, TILE_FNG_SPIKE_GOLD);
		if(pChr->IsTouchingTile(TILE_FNG_SPIKE_GREEN))
			OnSpike(pChr, TILE_FNG_SPIKE_GREEN);
		if(pChr->IsTouchingTile(TILE_FNG_SPIKE_PURPLE))
			OnSpike(pChr, TILE_FNG_SPIKE_PURPLE);
	}
}

void CGameControllerBaseFng::OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason)
{
	while(true)
	{
		if(!g_Config.m_SvPunishFreezeDisconnect)
			break;

		CCharacter *pChr = pPlayer->GetCharacter();
		if(!pChr)
			break;
		if(!pChr->m_FreezeTime)
			break;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "ban %d %d \"disconnected while frozen\"", pPlayer->GetCid(), g_Config.m_SvPunishFreezeDisconnect);
		Console()->ExecuteLine(aBuf);
		break;
	}

	CGameControllerInstagib::OnPlayerDisconnect(pPlayer, pReason);
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

void CGameControllerBaseFng::OnWrongSpike(class CPlayer *pPlayer)
{
	pPlayer->AddScore(-6);
	CCharacter *pChr = pPlayer->GetCharacter();
	// this means you can selfkill before the wrong spike hits
	// to bypass getting frozen
	// but that seems fine
	if(!pChr)
		return;

	pPlayer->UpdateLastToucher(-1);
	pChr->Freeze(10);
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

		if(SpikeTile == TILE_FNG_SPIKE_NORMAL)
		{
			pKiller->AddScore(2);
			m_aTeamscore[pKiller->GetTeam()] += 5;
		}
		if(SpikeTile == TILE_FNG_SPIKE_GOLD)
		{
			pKiller->AddScore(7);
			m_aTeamscore[pKiller->GetTeam()] += 12;
		}
		if(SpikeTile == TILE_FNG_SPIKE_GREEN)
		{
			pKiller->AddScore(5);
			m_aTeamscore[pKiller->GetTeam()] += 15;
		}
		if(SpikeTile == TILE_FNG_SPIKE_PURPLE)
		{
			pKiller->AddScore(9);
			m_aTeamscore[pKiller->GetTeam()] += 18;
		}

		if(SpikeTile == TILE_FNG_SPIKE_RED)
		{
			if(pKiller->GetTeam() == TEAM_RED || !IsTeamPlay())
			{
				pKiller->AddScore(4);
				m_aTeamscore[pKiller->GetTeam()] += 10;
			}
			else
			{
				OnWrongSpike(pKiller);
			}
		}
		if(SpikeTile == TILE_FNG_SPIKE_BLUE)
		{
			if(pKiller->GetTeam() == TEAM_BLUE || !IsTeamPlay())
			{
				pKiller->AddScore(4);
				m_aTeamscore[pKiller->GetTeam()] += 10;
			}
			else
			{
				OnWrongSpike(pKiller);
			}
		}

		DoWincheckRound();
	}

	if(LastToucherId == -1)
		pChr->Die(pChr->GetPlayer()->GetCid(), WEAPON_WORLD);
	else
		pChr->Die(LastToucherId, WEAPON_NINJA);
}

void CGameControllerBaseFng::OnSnapDDNetCharacter(CCharacter *pChr, CNetObj_DDNetCharacter *pDDNetCharacter, int SnappingClient)
{
	CGameControllerInstagib::OnSnapDDNetCharacter(pChr, pDDNetCharacter, SnappingClient);

	CPlayer *pSnapReceiver = GameServer()->m_apPlayers[SnappingClient];
	bool IsTeamMate = pChr->GetPlayer()->GetCid() == SnappingClient;
	if(IsTeamPlay() && pChr->GetPlayer()->GetTeam() == pSnapReceiver->GetTeam())
		IsTeamMate = true;
	if(!IsTeamMate && pDDNetCharacter->m_FreezeEnd)
		pDDNetCharacter->m_FreezeEnd = -1;
}

CClientMask CGameControllerBaseFng::FreezeDamageIndicatorMask(class CCharacter *pChr)
{
	CClientMask Mask = pChr->TeamMask() & GameServer()->ClientsMaskExcludeClientVersionAndHigher(VERSION_DDNET_NEW_HUD);
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(pPlayer->GetTeam() == pChr->GetPlayer()->GetTeam() && GameServer()->m_pController->IsTeamPlay())
			continue;
		if(pPlayer->GetCid() == pChr->GetPlayer()->GetCid())
			continue;

		Mask.reset(pPlayer->GetCid());
	}
	return Mask;
}

bool CGameControllerBaseFng::OnSelfkill(int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return false;
	if(!pChr->m_FreezeTime)
		return false;

	GameServer()->SendChatTarget(ClientId, "You can't kill while being frozen");
	return true;
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
		m_aTeamscore[pKiller->GetTeam()]++;
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
