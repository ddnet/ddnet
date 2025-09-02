#include <game/gamecore.h>
#include <game/server/entities/character.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>

#include <generated/protocol.h>

#include <engine/shared/protocol.h>

#include <base/vmath.h>
#include <iterator>

#include "portal.h"
#include <algorithm>
#include <base/system.h>
#include <game/teamscore.h>

constexpr float PortalRadius = 52.0f;
constexpr float MaxDistanceFromPlayer = 1200.0f;
constexpr int Lifetime = 12.5 * SERVER_TICK_SPEED;

CPortal::CPortal(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTALS, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	m_State = STATE_NONE;

	for(int i = 0; i < NUM_IDS; i++)
		m_Snap.m_aIds[i] = Server()->SnapNewId();
	std::sort(std::begin(m_Snap.m_aIds), std::end(m_Snap.m_aIds)); // Ensures lasers dont overlap weirdly

	for(int i = 0; i < NUM_PRTCL; i++)
		m_Snap.m_aParticleIds[i] = Server()->SnapNewId();

	GameWorld()->InsertEntity(this);
}

void CPortal::Reset()
{
	Server()->SnapFreeId(GetId());
	for(int i = 0; i < NUM_IDS; i++)
		Server()->SnapFreeId(m_Snap.m_aIds[i]);
	for(int i = 0; i < NUM_PRTCL; i++)
		Server()->SnapFreeId(m_Snap.m_aParticleIds[i]);

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(pOwnerChar)
		pOwnerChar->m_pPortal = nullptr;
	GameWorld()->RemoveEntity(this);
}

inline bool PointInCircle(vec2 pos, vec2 center, float radius)
{
	return length_squared(pos - center) <= radius * radius;
}

void CPortal::Tick()
{
	m_Lifetime--;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if((!GameServer()->m_apPlayers[m_Owner] || (pOwnerChar && !pOwnerChar->GetWeaponGot(WEAPON_PORTALGUN))) && m_Lifetime <= 0)
	{
		Reset();
		return;
	}

	if(m_Lifetime <= 0)
	{
		RemovePortals();
		return;
	}

	if(m_State == STATE_FIRST_SET)
	{
		if(m_PortalData[0].m_Team != pOwnerChar->Team())
		{
			RemovePortals();
			return;
		}
	}
	else if(m_State == STATE_BOTH_SET)
	{
		if(m_PortalData[0].m_Team != m_PortalData[1].m_Team)
		{
			RemovePortals();
			return;
		}
	}

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
		if(!pChr || !pChr->IsAlive())
			continue;

		if(m_PortalData[0].m_Team != TEAM_SUPER && pChr->Team() != TEAM_SUPER && pChr->Team() != m_PortalData[0].m_Team)
			continue;

		bool Teleported = false;
		if(m_PortalData[0].m_Active && m_PortalData[1].m_Active)
		{
			if(PointInCircle(pChr->m_Pos, m_PortalData[0].m_Pos, PortalRadius))
			{
				if(!m_CanTeleport[ClientId])
					return;

				pChr->m_PrevPos = m_PortalData[1].m_Pos;
				pChr->SetPosition(m_PortalData[1].m_Pos);
				pChr->ReleaseHook();
				m_CanTeleport[ClientId] = false;
				Teleported = true;
			}
			else if(PointInCircle(pChr->m_Pos, m_PortalData[1].m_Pos, PortalRadius))
			{
				if(!m_CanTeleport[ClientId])
					return;
				pChr->m_PrevPos = m_PortalData[0].m_Pos;
				pChr->SetPosition(m_PortalData[0].m_Pos);
				pChr->ReleaseHook();
				m_CanTeleport[ClientId] = false;
				Teleported = true;
			}
			else
				m_CanTeleport[ClientId] = true;
		}
		if(Teleported)
		{
			if(distance(m_PortalData[0].m_Pos, m_PortalData[1].m_Pos) > 450.0f)
			{
				GameServer()->CreateSound(m_PortalData[0].m_Pos, SOUND_WEAPON_SPAWN);
				GameServer()->CreateSound(m_PortalData[1].m_Pos, SOUND_WEAPON_SPAWN);
			}
			else
				GameServer()->CreateSound(pChr->m_Pos, SOUND_WEAPON_SPAWN);
		}
	}
	SetPortalVisual();
}

inline vec2 CPortal::CirclePos(int Part)
{
	vec2 Direction = direction(360.0f / CPortal::SEGMENTS * Part * (pi / 180.0f));
	Direction *= PortalRadius;

	return Direction;
}

void CPortal::SetPortalVisual()
{
	for(int i = 0; i < SEGMENTS + 1; i++)
	{
		m_Snap.m_To[i] = CirclePos(i);
		m_Snap.m_From[i] = CirclePos(i + 1);
	}
	m_Snap.m_From[SEGMENTS] = CirclePos(SEGMENTS);
}

void CPortal::OnFire()
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(TrySetPortal())
	{
		GameServer()->CreateSound(pOwnerChar->m_Pos, SOUND_PICKUP_HEALTH);
	}
	else
	{
		GameServer()->CreateSound(pOwnerChar->m_Pos, SOUND_WEAPON_NOAMMO);
		pOwnerChar->SetReloadTimer(250 * Server()->TickSpeed() / 1000);
	}
}

bool CPortal::TrySetPortal()
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	vec2 CursorPos = pOwnerChar->GetCursorPos();

	if(Collision()->TestBox(CursorPos, CCharacterCore::PhysicalSizeVec2()))
		return false; // teleport position is inside a block
	if(!pOwnerChar->HasLineOfSight(CursorPos))
		return false; // Theres blocks in the way
	if(distance(pOwnerChar->m_Pos, CursorPos) > MaxDistanceFromPlayer)
		return false; // Too far away
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
		if(!pChr || !pChr->IsAlive())
			continue;
		if(PointInCircle(pChr->m_Pos, CursorPos, PortalRadius + CCharacterCore::PhysicalSize()))
			return false; // Don't place portal on players
	}
	if(m_State == STATE_NONE)
	{
		m_PortalData[0].m_Active = true;
		m_PortalData[0].m_Pos = CursorPos;
		m_PortalData[0].m_Team = pOwnerChar->Team();
		m_Lifetime = Lifetime;
		m_State = STATE_FIRST_SET;
	}
	else if(m_State == STATE_FIRST_SET)
	{
		m_PortalData[1].m_Active = true;
		m_PortalData[1].m_Pos = CursorPos;
		m_PortalData[1].m_Team = pOwnerChar->Team();
		m_Lifetime = Lifetime;
		m_State = STATE_BOTH_SET;
	}
	else if(m_State == STATE_BOTH_SET)
		return false;
	return true;
}

void CPortal::RemovePortals()
{
	for(int i = 0; i < NUM_PORTALS; i++)
	{
		if(m_PortalData[i].m_Active)
		{
			m_PortalData[i].m_Active = false;
			m_PortalData[i].m_Pos = vec2(0, 0);
			m_State = STATE_NONE;
		}
	}
}

inline vec2 GetRandomPointInCircle(vec2 center, float radius)
{
	float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * pi;
	float r = radius * sqrtf(static_cast<float>(rand()) / RAND_MAX);
	return center + vec2(cosf(angle), sinf(angle)) * r;
}

void CPortal::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_PortalData[0].m_Pos) && NetworkClipped(SnappingClient, m_PortalData[1].m_Pos))
		return;

	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
	if(!pSnapPlayer)
		return;

	if(CCharacter *pSnapChar = pSnapPlayer->GetCharacter())
	{
		if(m_PortalData[0].m_Team == TEAM_SUPER)
			;
		else if(pSnapPlayer->IsPaused())
		{
			if(pSnapChar->Team() != m_PortalData[0].m_Team && pSnapPlayer->m_SpecTeam)
				return;
		}
		else
		{
			if(pSnapChar->Team() != TEAM_SUPER && pSnapChar->Team() != m_PortalData[0].m_Team)
				return;
		}
	}

	const int snapVer = pSnapPlayer->GetClientVersion();
	const bool sixUp = Server()->IsSixup(SnappingClient);
	const int StartTick = Server()->Tick() + 2;

	for(int p = 0; p < NUM_PORTALS; ++p)
	{
		if(!m_PortalData[p].m_Active)
			continue;
		const int baseId = p ? 13 : 0;
		for(int i = 0; i < NUM_POS; ++i)
		{
			vec2 To = m_Snap.m_To[i];
			vec2 From = m_Snap.m_From[i];

			const float Spin = (Server()->Tick() / 30.0) + (p * pi);
			Collision()->Rotate(vec2(0, 0), &To, Spin);
			Collision()->Rotate(vec2(0, 0), &From, Spin);

			To += m_PortalData[p].m_Pos;
			From += m_PortalData[p].m_Pos;

			GameServer()->SnapLaserObject(CSnapContext(snapVer, sixUp, SnappingClient),
				m_Snap.m_aIds[baseId + i], To, From, StartTick);
		}
	}

	if(m_State == STATE_BOTH_SET)
	{
		for(size_t i = 0; i < NUM_PRTCL; i++)
		{
			CNetObj_Projectile *pProj = Server()->SnapNewItem<CNetObj_Projectile>(m_Snap.m_aParticleIds[i]);
			if(!pProj)
				return;

			vec2 Pos = GetRandomPointInCircle(m_PortalData[i % 2].m_Pos, PortalRadius - 6.0f);
			pProj->m_X = (int)(Pos.x);
			pProj->m_Y = (int)(Pos.y);
			pProj->m_VelX = 0;
			pProj->m_VelY = 0;
			pProj->m_StartTick = 0;
			pProj->m_Type = WEAPON_HAMMER;
		}
	}
}