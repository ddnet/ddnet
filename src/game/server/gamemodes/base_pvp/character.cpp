#include <engine/antibot.h>
#include <engine/shared/config.h>
#include <game/generated/server_data.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_pvp.h"

void CGameControllerPvp::OnCharacterConstruct(class CCharacter *pChr)
{
	pChr->m_IsGodmode = false;
}

bool CCharacter::OnFngFireWeapon(CCharacter &Character, int &Weapon, vec2 &Direction, vec2 &MouseTarget, vec2 &ProjStartPos)
{
	if(Weapon != WEAPON_HAMMER)
		return false;

	// reset objects Hit
	m_NumObjectsHit = 0;
	GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, TeamMask()); // NOLINT(clang-analyzer-unix.Malloc)

	Antibot()->OnHammerFire(m_pPlayer->GetCid());

	if(m_Core.m_HammerHitDisabled)
		return true;

	CEntity *apEnts[MAX_CLIENTS];
	int Hits = 0;
	int Num = GameServer()->m_World.FindEntities(ProjStartPos, GetProximityRadius() * 0.5f, apEnts,
		MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for(int i = 0; i < Num; ++i)
	{
		auto *pTarget = static_cast<CCharacter *>(apEnts[i]);

		//if ((pTarget == this) || Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
		if((pTarget == this || (pTarget->IsAlive() && !CanCollide(pTarget->GetPlayer()->GetCid()))))
			continue;

		// set his velocity to fast upward (for now)
		if(length(pTarget->m_Pos - ProjStartPos) > 0.0f)
			GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * 0.5f, TeamMask());
		else
			GameServer()->CreateHammerHit(ProjStartPos, TeamMask());

		pTarget->TakeHammerHit(this);

		if(m_FreezeHammer)
			pTarget->Freeze();

		Antibot()->OnHammerHit(m_pPlayer->GetCid(), pTarget->GetPlayer()->GetCid());

		Hits++;
	}

	// if we Hit anything, we have to wait for the reload
	if(Hits)
	{
		float FireDelay = GetTuning(m_TuneZone)->m_HammerHitFireDelay;
		m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
	}

	// we consume the CCharacter::FireWeapon() event
	// so we have to set the reload timer in the end

	m_AttackTick = Server()->Tick();

	if(!m_ReloadTimer)
	{
		float FireDelay;
		GetTuning(m_TuneZone)->Get(38 + m_Core.m_ActiveWeapon, &FireDelay);
		m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
	}

	return true;
}

void CCharacter::TakeHammerHit(CCharacter *pFrom)
{
	vec2 Dir;
	if(length(m_Pos - pFrom->m_Pos) > 0.0f)
		Dir = normalize(m_Pos - pFrom->m_Pos);
	else
		Dir = vec2(0.f, -1.f);

	vec2 Push = vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
	if(g_Config.m_SvFngHammer)
	{
		if(GameServer()->m_pController->IsTeamplay() && pFrom->GetPlayer() && m_pPlayer->GetTeam() == pFrom->GetPlayer()->GetTeam() && m_FreezeTime)
		{
			Push.x *= g_Config.m_SvMeltHammerScaleX * 0.01f;
			Push.y *= g_Config.m_SvMeltHammerScaleY * 0.01f;
		}
		else
		{
			Push.x *= g_Config.m_SvHammerScaleX * 0.01f;
			Push.y *= g_Config.m_SvHammerScaleY * 0.01f;
		}
	}

	vec2 Temp = m_Core.m_Vel + Push;
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, Temp);

	CPlayer *pPlayer = pFrom->GetPlayer();
	if(pPlayer && (GameServer()->m_pController->IsTeamplay() && pPlayer->GetTeam() == m_pPlayer->GetTeam()))
	{
		// interaction from team mates protects from spikes
		m_pPlayer->UpdateLastToucher(-1);

		if(m_FreezeTime)
		{
			m_FreezeTime -= Server()->TickSpeed() * 3;
		}
	}
	else
	{
		m_pPlayer->UpdateLastToucher(pPlayer->GetCid());
	}
}

void CCharacter::ResetInstaSettings()
{
	int Ammo = -1;
	// TODO: this should not check the default weapon
	//       but if any of the spawn weapons is a grenade
	if(GameServer()->m_pController->GetDefaultWeapon(GetPlayer()) == WEAPON_GRENADE)
	{
		Ammo = g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1;
		m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
	}
	GiveWeapon(GameServer()->m_pController->GetDefaultWeapon(GetPlayer()), false, Ammo);
}
