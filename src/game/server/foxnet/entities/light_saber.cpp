// Made by qxdFox
#include <game/server/entities/character.h>
#include <game/server/player.h>

#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/teams.h>

#include <generated/protocol.h>

#include <base/vmath.h>
#include <game/gamecore.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>
#include <game/teamscore.h>

#include "light_saber.h"
#include <engine/shared/config.h>

CLightSaber::CLightSaber(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHTSABER, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	m_From = Pos;
	m_To = Pos;

	GameWorld()->InsertEntity(this);
}

void CLightSaber::Reset()
{
	if(CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner))
		pChr->m_pLightSaber = nullptr;
	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CLightSaber::OnFire()
{
	if(m_State == STATE_RETRACTED || m_State == STATE_RETRACTING)
		m_State = STATE_EXTENDING;
	else if(m_State == STATE_EXTENDED || m_State == STATE_EXTENDING)
		m_State = STATE_RETRACTING;
}

void CLightSaber::Tick()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);

	if(!pChr)
	{
		Reset();
		return;
	}
	if(pChr->GetActiveWeapon() != WEAPON_LIGHTSABER)
	{
		if(m_Length > 0)
			m_State = STATE_RETRACTING;
		else
		{
			Reset();
			return;
		}
	}
	if(pChr->m_FreezeTime > 0 || pChr->IsPaused())
	{
		if(m_Length > 0)
			m_State = STATE_RETRACTING;
	}

	if(m_State == STATE_EXTENDING)
	{
		if(Server()->Tick() % 5 == 0)
			GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE, pChr->TeamMask());
		m_Length += LIGHT_SABER_SPEED;
		if(m_Length > LIGHT_SABER_MAX_LENGTH)
		{
			m_Length = LIGHT_SABER_MAX_LENGTH;
			m_State = STATE_EXTENDED;
		}
	}
	else if(m_State == STATE_RETRACTING)
	{
		if(Server()->Tick() % 5 == 0)
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, pChr->TeamMask());
		m_Length -= LIGHT_SABER_SPEED;
		if(m_Length < 0)
		{
			m_Length = 0;
			m_State = STATE_RETRACTED;
		}
	}
	m_Pos = pChr->m_Pos;
	m_To = pChr->m_Pos;
	vec2 WantedFrom = m_Pos + normalize(vec2(pChr->Input()->m_TargetX, pChr->Input()->m_TargetY)) * m_Length;
	GameServer()->Collision()->IntersectLine(m_Pos, WantedFrom, &m_From, 0);

	if(pChr->Core()->m_Solo)
		return;

	std::vector<CCharacter *> HitChars = GameWorld()->IntersectedCharacters(m_From, m_To, 6.0f, GameServer()->GetPlayerChar(m_Owner));
	if(HitChars.empty())
		return;

	for(CCharacter *pHit : HitChars)
	{
		if(pChr->Team() != TEAM_SUPER && pChr->Team() != pHit->Team())
			continue;
		if(pChr->Core()->m_Solo)
			continue;

		pHit->SetEmote(EMOTE_PAIN, Server()->Tick() + 2);
	}
}

void CLightSaber::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwnerChr)
		return;

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		const CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		if(!pSnapPlayer)
			return;

		CGameTeams Teams = GameServer()->m_pController->Teams();
		int Team = pOwnerChr->Team();

		if(!Teams.SetMask(SnappingClient, Team))
			return;

		if(pSnapPlayer->GetCharacter() && pOwnerChr)
			if(!pOwnerChr->CanSnapCharacter(SnappingClient))
				return;

		if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
			if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
				return;
	}

	if(m_Length <= 0)
		return;

	vec2 From = m_From;
	vec2 To = m_To;
	if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient)
	{
		const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
		const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);
		const vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;

		To = m_Pos + nVel;
		const vec2 WantedFrom = To + normalize(vec2(pOwnerChr->Input()->m_TargetX, pOwnerChr->Input()->m_TargetY)) * m_Length;
		GameServer()->Collision()->IntersectLine(To, WantedFrom, &From, 0);
	}

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	const bool SixUp = Server()->IsSixup(SnappingClient);
	GameServer()->SnapLaserObject(CSnapContext(SnapVer, SixUp, SnappingClient), GetId(), To, From, Server()->Tick() - 3, m_Owner, LASERTYPE_GUN, -1, -1, LASERFLAG_NO_PREDICT);
}