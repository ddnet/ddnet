
#include <game/server/entities/character.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <game/gamecore.h>
#include <game/teamscore.h>

#include <generated/protocol.h>

#include <engine/shared/protocol.h>

#include <base/vmath.h>
#include <iterator>

#include "portal.h"
#include <algorithm>
#include <engine/server.h>

constexpr float MaxPortalRad = 56.0f;
constexpr float MinPortalRad = 15.0f;
constexpr int FadeOutTicks = SERVER_TICK_SPEED;
constexpr int GrowTicks = SERVER_TICK_SPEED / 4;
constexpr float MaxDistanceFromPlayer = 1200.0f;
constexpr int Lifetime = 12.5 * SERVER_TICK_SPEED;

CPortal::CPortal(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTALS, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	m_State = STATE_NONE;

	if(CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner))
		m_TeamMask = pChr->TeamMask();
	else
		m_TeamMask = CClientMask().set();

	for(int p = 0; p < NUM_PORTALS; p++)
	{
		m_apData[p].m_PortalRadius = MinPortalRad;

		for(int i = 0; i < NUM_IDS; i++)
			m_Snap[p].m_aIds[i] = Server()->SnapNewId();
		std::sort(std::begin(m_Snap[p].m_aIds), std::end(m_Snap[p].m_aIds)); // Ensures lasers dont overlap weirdly
		for(int i = 0; i < NUM_PRTCL; i++)
			m_Snap[p].m_aParticleIds[i] = Server()->SnapNewId();
	}

	GameWorld()->InsertEntity(this);
}

void CPortal::Reset()
{
	Server()->SnapFreeId(GetId());

	for(int p = 0; p < NUM_PORTALS; p++)
	{
		for(int i = 0; i < NUM_IDS; i++)
			Server()->SnapFreeId(m_Snap[p].m_aIds[i]);
		for(int i = 0; i < NUM_PRTCL; i++)
			Server()->SnapFreeId(m_Snap[p].m_aParticleIds[i]);
	}

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
	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);

	if((!GameServer()->m_apPlayers[m_Owner] || (pOwnerChr && !pOwnerChr->GetWeaponGot(WEAPON_PORTALGUN))) && m_Lifetime <= 0)
	{
		Reset();
		return;
	}

	if(m_Lifetime > 0 && (m_State == STATE_FIRST_SET || m_State == STATE_BOTH_SET))
	{
		for(int p = 0; p < NUM_PORTALS; p++)
		{
			if(!m_apData[p].m_Active)
				continue;

			if(m_Lifetime <= FadeOutTicks)
			{
				// Shrink phase (last FadeOutTicks)
				float a = static_cast<float>(m_Lifetime) / static_cast<float>(FadeOutTicks);
				m_apData[p].m_PortalRadius = MinPortalRad + (MaxPortalRad - MinPortalRad) * a;
				if(m_apData[p].m_PortalRadius < MinPortalRad)
					m_apData[p].m_PortalRadius = MinPortalRad;
			}
			else
			{
				if(m_apData[p].m_PortalRadius < MaxPortalRad)
				{
					float growPerTick = (MaxPortalRad - MinPortalRad) / static_cast<float>(GrowTicks);
					if(growPerTick < 0.001f)
						growPerTick = MaxPortalRad - MinPortalRad;
					m_apData[p].m_PortalRadius += growPerTick;
					if(m_apData[p].m_PortalRadius > MaxPortalRad)
						m_apData[p].m_PortalRadius = MaxPortalRad;
				}
				else
				{
					m_apData[p].m_PortalRadius = MaxPortalRad;
				}
			}
		}
	}
	else if(m_Lifetime <= 0)
	{
		for(int p = 0; p < NUM_PORTALS; p++)
			m_apData[p].m_PortalRadius = MinPortalRad;
		RemovePortals();
		return;
	}

	if(m_State == STATE_FIRST_SET)
	{
		if(!pOwnerChr)
		{
			RemovePortals();
			return;
		}

		if(m_apData[0].m_Team != pOwnerChr->Team())
		{
			RemovePortals();
			return;
		}
	}
	else if(m_State == STATE_BOTH_SET)
	{
		if(m_apData[0].m_Team != m_apData[1].m_Team)
		{
			RemovePortals();
			return;
		}
	}

	HandleTele();
	SetPortalVisual();
}

void CPortal::HandleTele()
{
	if(!m_apData[0].m_Active || !m_apData[1].m_Active)
		return;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
		if(!pChr || !pChr->IsAlive())
			continue;

		if(m_apData[0].m_Team != TEAM_SUPER && pChr->Team() != TEAM_SUPER && pChr->Team() != m_apData[0].m_Team)
			continue;

		const bool InP0 = PointInCircle(pChr->m_Pos, m_apData[0].m_Pos, m_apData[0].m_PortalRadius);
		const bool InP1 = !InP0 && PointInCircle(pChr->m_Pos, m_apData[1].m_Pos, m_apData[1].m_PortalRadius);

		if(InP0 || InP1)
		{
			bool &Can = m_aCanTeleport[ClientId];
			if(!Can)
				continue;

			const vec2 Target = InP0 ? m_apData[1].m_Pos : m_apData[0].m_Pos;
			pChr->ForceSetPos(Target);
			pChr->ReleaseHook();
			Can = false;

			if(distance(m_apData[0].m_Pos, m_apData[1].m_Pos) > 550.0f)
			{
				GameServer()->CreateSound(m_apData[0].m_Pos, SOUND_WEAPON_SPAWN, pChr->TeamMask());
				GameServer()->CreateSound(m_apData[1].m_Pos, SOUND_WEAPON_SPAWN, pChr->TeamMask());
			}
			else
			{
				GameServer()->CreateSound(Target, SOUND_WEAPON_SPAWN, pChr->TeamMask());
			}
		}
		else
		{
			m_aCanTeleport[ClientId] = true;
		}
	}
}
inline vec2 CPortal::CirclePos(int Portal, int Part) const
{
	vec2 Direction = direction(360.0f / CPortal::SEGMENTS * Part * (pi / 180.0f));
	Direction *= m_apData[Portal].m_PortalRadius;
	return Direction;
}

void CPortal::SetPortalVisual()
{
	for(int p = 0; p < NUM_PORTALS; p++)
	{
		for(int i = 0; i < SEGMENTS + 1; i++)
		{
			m_Snap[p].m_aTo[i] = CirclePos(p, i);
			m_Snap[p].m_aFrom[i] = CirclePos(p, i + 1);
		}
		m_Snap[p].m_aFrom[SEGMENTS] = CirclePos(p, SEGMENTS);
	}
}

void CPortal::OnFire()
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(TrySetPortal())
	{
		GameServer()->CreateSound(pOwnerChar->m_Pos, SOUND_PICKUP_HEALTH, m_TeamMask);
	}
	else
	{
		GameServer()->CreateSound(pOwnerChar->m_Pos, SOUND_WEAPON_NOAMMO, m_TeamMask);
		pOwnerChar->SetReloadTimer(250 * Server()->TickSpeed() / 1000);
	}
}

bool CPortal::TrySetPortal()
{
	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	vec2 CursorPos = pOwnerChr->GetCursorPos();

	if(Collision()->TestBox(CursorPos, CCharacterCore::PhysicalSizeVec2()))
		return false; // teleport position is inside a block
	if(!pOwnerChr->HasLineOfSight(CursorPos))
		return false; // Theres blocks in the way
	if(distance(pOwnerChr->m_Pos, CursorPos) > MaxDistanceFromPlayer)
		return false; // Too far away
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
		if(!pChr || !pChr->IsAlive())
			continue;
		if(ClientId == m_Owner)
			continue;
		if(PointInCircle(pChr->m_Pos, CursorPos, MaxPortalRad + CCharacterCore::PhysicalSize() - 6))
			return false; // Don't place portal on players
	}
	if(m_State == STATE_NONE)
	{
		m_apData[0].m_Active = true;
		m_apData[0].m_Pos = CursorPos;
		m_apData[0].m_Team = pOwnerChr->Team();
		m_Lifetime = Lifetime;
		m_State = STATE_FIRST_SET;
	}
	else if(m_State == STATE_FIRST_SET)
	{
		m_apData[1].m_Active = true;
		m_apData[1].m_Pos = CursorPos;
		m_apData[1].m_Team = pOwnerChr->Team();
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
		if(m_apData[i].m_Active)
		{
			GameServer()->CreateDeath(m_apData[i].m_Pos, m_Owner, m_TeamMask);
			m_apData[i].m_Active = false;
			m_apData[i].m_Pos = vec2(0, 0);
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
	if(NetworkClipped(SnappingClient, m_apData[0].m_Pos) && NetworkClipped(SnappingClient, m_apData[1].m_Pos))
		return;

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		if(!pSnapPlayer)
			return;
	}

	CGameTeams Teams = GameServer()->m_pController->Teams();
	if(!Teams.SetMaskWithFlags(SnappingClient, m_apData[0].m_Team, CGameTeams::EXTRAFLAG_IGNORE_SOLO))
		return;

	int Team = Teams.m_Core.Team(SnappingClient);
	if(Team != m_apData[0].m_Team && Team != TEAM_SUPER)
		Team = MAX_CLIENTS;

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	const bool SixUp = Server()->IsSixup(SnappingClient);
	const int StartTick = Server()->Tick() + 2;

	for(int p = 0; p < NUM_PORTALS; ++p)
	{
		if(!m_apData[p].m_Active)
			continue;
		for(int i = 0; i < NUM_POS; ++i)
		{
			vec2 To = m_Snap[p].m_aTo[i];
			vec2 From = m_Snap[p].m_aFrom[i];

			const float Spin = (Server()->Tick() / 30.0) + (p * pi);
			Collision()->Rotate(vec2(0, 0), &To, Spin);
			Collision()->Rotate(vec2(0, 0), &From, Spin);

			To += m_apData[p].m_Pos;
			From += m_apData[p].m_Pos;

			GameServer()->SnapLaserObject(CSnapContext(SnapVer, SixUp, SnappingClient),
				m_Snap[p].m_aIds[i], To, From, StartTick, Team);
		}

		if(m_State == STATE_BOTH_SET)
		{
			for(size_t i = 0; i < NUM_PRTCL; i++)
			{
				CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(m_Snap[p].m_aParticleIds[i]);
				if(!pProj)
					continue;

				vec2 Pos = GetRandomPointInCircle(m_apData[i % 2].m_Pos, m_apData[p].m_PortalRadius - 6.0f);
				pProj->m_X = round_to_int(Pos.x * 100.0f);
				pProj->m_Y = round_to_int(Pos.y * 100.0f);
				pProj->m_StartTick = 0;
				pProj->m_VelX = 0;
				pProj->m_VelY = 0;
				pProj->m_Type = WEAPON_HAMMER;
				pProj->m_Owner = Team;
			}
		}
	}
}