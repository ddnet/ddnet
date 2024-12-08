/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "door.h"
#include "character.h"

#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/teamscore.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

CDoor::CDoor(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
	int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Number = Number;
	m_Pos = Pos;
	m_Length = Length;
	m_Direction = vec2(std::sin(Rotation), std::cos(Rotation));
	vec2 To = Pos + normalize(m_Direction) * m_Length;

	GameServer()->Collision()->IntersectNoLaser(Pos, To, &this->m_To, 0);
	ResetCollision();
	GameWorld()->InsertEntity(this);
}

void CDoor::ResetCollision()
{
	for(int i = 0; i < m_Length - 1; i++)
	{
		vec2 CurrentPos(m_Pos.x + (m_Direction.x * i),
			m_Pos.y + (m_Direction.y * i));
		if(GameServer()->Collision()->CheckPoint(CurrentPos) || GameServer()->Collision()->GetTile(m_Pos.x, m_Pos.y) || GameServer()->Collision()->GetFrontTile(m_Pos.x, m_Pos.y))
			break;
		else
			GameServer()->Collision()->SetDoorCollisionAt(
				m_Pos.x + (m_Direction.x * i),
				m_Pos.y + (m_Direction.y * i), TILE_STOPA, 0 /*Flags*/,
				m_Number);
	}
}

void CDoor::Reset()
{
	m_MarkedForDestroy = true;
}

void CDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_To))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);

	vec2 From;
	int StartTick;

	if(SnappingClientVersion >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		From = m_To;
		StartTick = -1;
	}
	else
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);

		if(SnappingClient != SERVER_DEMO_CLIENT && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->m_SpectatorId != SPEC_FREEVIEW)
			pChr = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->m_SpectatorId);

		if(pChr && pChr->Team() != TEAM_SUPER && pChr->IsAlive() && !Switchers().empty() && Switchers()[m_Number].m_aStatus[pChr->Team()])
		{
			From = m_To;
		}
		else
		{
			From = m_Pos;
		}
		StartTick = Server()->Tick();
	}

	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion), GetId(),
		m_Pos, From, StartTick, -1, LASERTYPE_DOOR, 0, m_Number);
}
