#include "block.h"

#include <base/log.h>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/player.h>

CGameControllerBlock::CGameControllerBlock(class CGameContext *pGameServer) :
	CGameControllerDDRace(pGameServer)
{
	m_pGameType = "block";
}

CGameControllerBlock::~CGameControllerBlock() = default;

int CGameControllerBlock::SnapGameInfoExFlags(int SnappingClient)
{
	int Flags = CGameControllerDDRace::SnapGameInfoExFlags(SnappingClient);
	Flags &= ~(GAMEINFOFLAG_TIMESCORE);
	return Flags;
}

void CGameControllerBlock::OnPlayerConnect(CPlayer *pPlayer)
{
	CGameControllerDDRace::OnPlayerConnect(pPlayer);

	m_aLastToucher[pPlayer->GetCid()] = std::nullopt;
}

void CGameControllerBlock::OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason)
{
	CGameControllerDDRace::OnPlayerDisconnect(pPlayer, pReason);

	m_aLastToucher[pPlayer->GetCid()] = std::nullopt;
}

bool CGameControllerBlock::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(From < 0 || From >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = GameServer()->m_apPlayers[From];
	m_aLastToucher[Character.GetId()] = CLastToucher(From, pPlayer->GetUniqueCid(), pPlayer->GetTeam(), Weapon, Server()->Tick());
	return false;
}

int CGameControllerBlock::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int &KillerId, int &Weapon)
{
	bool IsFrozen = pVictim->m_FreezeTime != 0;
	CGameControllerDDRace::OnCharacterDeath(pVictim, pKiller, KillerId, Weapon);

	if(!IsFrozen)
		return 0;
	std::optional<CLastToucher> &LastToucher = m_aLastToucher[pVictim->GetId()];
	if(!LastToucher.has_value())
		return 0;
	// killer already left the game
	CPlayer *pToucher = GameServer()->m_apPlayers[LastToucher.value().m_ClientId];
	if(!pToucher || pToucher->GetUniqueCid() != LastToucher.value().m_UniqueClientId)
		return 0;

	pToucher->m_BlockPoints++;

	KillerId = LastToucher.value().m_ClientId;
	Weapon = LastToucher.value().m_Weapon;

	// TODO: support weapon hook on the client side
	if(Weapon == WEAPON_HOOK)
		Weapon = WEAPON_NINJA;
	return 0;
}

int CGameControllerBlock::SnapPlayerScore(int SnappingClient, class CPlayer *pPlayer)
{
	return pPlayer->m_BlockPoints;
}

void CGameControllerBlock::Tick()
{
	CGameControllerDDRace::Tick();

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		CCharacter *pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
			continue;

		// set hook touch
		int HookedId = pChr->Core()->HookedPlayer();
		if(HookedId >= 0 && HookedId < MAX_CLIENTS)
		{
			CPlayer *pHooked = GameServer()->m_apPlayers[HookedId];
			if(pHooked)
			{
				SetLastToucher(pChr->GetId(), HookedId, WEAPON_HOOK);
			}
		}

		// clear expired old touches
		if(pChr->m_FreezeTime)
			continue;
		std::optional<CLastToucher> &LastToucher = m_aLastToucher[pPlayer->GetCid()];
		if(!LastToucher.has_value())
			continue;
		int TicksSinceTouch = Server()->Tick() - LastToucher.value().m_TouchTick;
		int SecsSinceTouch = TicksSinceTouch / Server()->TickSpeed();
		if(SecsSinceTouch > 3)
			m_aLastToucher[pPlayer->GetCid()] = std::nullopt;
	}
}

void CGameControllerBlock::SetLastToucher(int ToucherId, int TouchedId, int Weapon)
{
	CPlayer *pToucher = GameServer()->m_apPlayers[ToucherId];
	m_aLastToucher[TouchedId] = CLastToucher(
		ToucherId,
		pToucher->GetUniqueCid(),
		pToucher->GetTeam(),
		Weapon,
		Server()->Tick());
}
