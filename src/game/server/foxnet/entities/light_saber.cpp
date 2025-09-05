// made by fokkonaut

#include "light_saber.h"
#include <game/server/entities/character.h>
#include <game/server/player.h>

#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/teams.h>

#include <generated/protocol.h>
#include <generated/server_data.h>

#include <base/vmath.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>
#include <game/teamscore.h>

CLightSaber::CLightSaber(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHT_SABER, Pos)
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
	m_From = pChr->m_Pos;
	vec2 WantedTo = m_Pos + normalize(vec2(pChr->Input()->m_TargetX, pChr->Input()->m_TargetY)) * m_Length;
	GameServer()->Collision()->IntersectLine(m_Pos, WantedTo, &m_To, 0);

	std::vector<CCharacter *> HitChars = GameWorld()->IntersectedCharacters(m_From, m_To, 6.0f, GameServer()->GetPlayerChar(m_Owner));
	if(HitChars.empty())
		return;

	for(CCharacter *pHit : HitChars)
	{
		if(pChr->Team() != TEAM_SUPER && pChr->Team() != pHit->Team())
			return;

		pHit->SetEmote(EMOTE_PAIN, Server()->Tick() + 2);
	}
}

void CLightSaber::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwnerChar)
		return;

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		if(!pSnapPlayer)
			return;

		CGameTeams Teams = GameServer()->m_pController->Teams();
		int Team = pOwnerChar->Team();

		if(!Teams.SetMask(SnappingClient, Team))
			return;

		if(pSnapPlayer->GetCharacter() && pOwnerChar)
			if(!pOwnerChar->CanSnapCharacter(SnappingClient))
				return;

		if(pOwnerChar->GetPlayer()->m_Vanish && SnappingClient != pOwnerChar->GetPlayer()->GetCid() && SnappingClient != -1)
			if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
				return;
	}

	if(m_Length <= 0)
		return;

	CNetObj_DDNetLaser *pLaser = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetId(), sizeof(CNetObj_DDNetLaser)));
	if(!pLaser)
		return;

	vec2 From = m_To + pOwnerChar->Core()->m_Vel / 2;
	vec2 To = m_From + pOwnerChar->Core()->m_Vel / 2;
	if(SnappingClient == m_Owner)
	{
		From = m_To + pOwnerChar->Core()->m_Vel;
		To = m_From + pOwnerChar->Core()->m_Vel;
	}

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	bool SixUp = Server()->IsSixup(SnappingClient);

	GameServer()->SnapLaserObject(CSnapContext(SnapVer, SixUp, SnappingClient), GetId(), To, From, Server()->Tick() - 3, m_Owner, LASERTYPE_GUN);
}