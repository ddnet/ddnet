/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "light.h"
#include "character.h"

#include <engine/server.h>

#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/teamscore.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

CLight::CLight(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
	int Layer, int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_To = vec2(0.0f, 0.0f);
	m_Core = vec2(0.0f, 0.0f);
	m_Layer = Layer;
	m_Number = Number;
	m_Tick = (Server()->TickSpeed() * 0.15f);
	m_Pos = Pos;
	m_Rotation = Rotation;
	m_Length = Length;
	m_EvalTick = Server()->Tick();
	GameWorld()->InsertEntity(this);
	Step();
}

bool CLight::HitCharacter()
{
	std::vector<CCharacter *> vpHitCharacters = GameServer()->m_World.IntersectedCharacters(m_Pos, m_To, 0.0f, 0);
	if(vpHitCharacters.empty())
		return false;
	for(auto *pChar : vpHitCharacters)
	{
		if(m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChar->Team()])
			continue;
		pChar->Freeze();
	}
	return true;
}

void CLight::Move()
{
	if(m_Speed != 0)
	{
		if((m_CurveLength >= m_Length && m_Speed > 0) || (m_CurveLength <= 0 && m_Speed < 0))
			m_Speed = -m_Speed;
		m_CurveLength += m_Speed * m_Tick + m_LengthL;
		m_LengthL = 0;
		if(m_CurveLength > m_Length)
		{
			m_LengthL = m_CurveLength - m_Length;
			m_CurveLength = m_Length;
		}
		else if(m_CurveLength < 0)
		{
			m_LengthL = 0 + m_CurveLength;
			m_CurveLength = 0;
		}
	}

	m_Rotation += m_AngularSpeed * m_Tick;
	if(m_Rotation > pi * 2)
		m_Rotation -= pi * 2;
	else if(m_Rotation < 0)
		m_Rotation += pi * 2;
}

void CLight::Step()
{
	Move();
	vec2 dir(std::sin(m_Rotation), std::cos(m_Rotation));
	vec2 to2 = m_Pos + normalize(dir) * m_CurveLength;
	GameServer()->Collision()->IntersectNoLaser(m_Pos, to2, &m_To, 0);
}

void CLight::Reset()
{
	m_MarkedForDestroy = true;
}

void CLight::Tick()
{
	if(Server()->Tick() % (int)(Server()->TickSpeed() * 0.15f) == 0)
	{
		m_EvalTick = Server()->Tick();
		GameServer()->Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;
		Step();
	}

	HitCharacter();
}

void CLight::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_To))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);

	if(SnappingClient != SERVER_DEMO_CLIENT && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->m_SpectatorId != SPEC_FREEVIEW)
		pChr = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->m_SpectatorId);

	vec2 From = m_Pos;
	int StartTick = -1;

	if(pChr && pChr->Team() == TEAM_SUPER)
	{
		From = m_Pos;
	}
	else if(pChr && m_Layer == LAYER_SWITCH && Switchers()[m_Number].m_aStatus[pChr->Team()])
	{
		From = m_To;
	}
	else if(m_Layer != LAYER_SWITCH)
	{
		From = m_To;
	}

	if(SnappingClientVersion < VERSION_DDNET_ENTITY_NETOBJS)
	{
		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 6;
		if(pChr && pChr->IsAlive() && m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChr->Team()] && Tick)
			return;

		StartTick = m_EvalTick;
		if(StartTick < Server()->Tick() - 4)
			StartTick = Server()->Tick() - 4;
		else if(StartTick > Server()->Tick())
			StartTick = Server()->Tick();
	}

	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion), GetId(),
		m_Pos, From, StartTick, -1, LASERTYPE_FREEZE, 0, m_Number);
}
