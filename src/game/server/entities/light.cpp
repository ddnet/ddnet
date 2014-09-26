/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include <engine/config.h>
#include <engine/server.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "light.h"
#include <game/mapitems.h>

CLight::CLight(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
		int Layer, int Number) :
		CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
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
	std::list<CCharacter *> HitCharacters =
			GameServer()->m_World.IntersectedCharacters(m_Pos, m_To, 0.0f, 0);
	if (HitCharacters.empty())
		return false;
	for (std::list<CCharacter *>::iterator i = HitCharacters.begin();
			i != HitCharacters.end(); i++)
	{
		CCharacter * Char = *i;
		if (m_Layer == LAYER_SWITCH
				&& !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()])
			continue;
		Char->Freeze();
	}
	return true;
}

void CLight::Move()
{
	if (m_Speed != 0)
	{
		if ((m_CurveLength >= m_Length && m_Speed > 0)
				|| (m_CurveLength <= 0 && m_Speed < 0))
			m_Speed = -m_Speed;
		m_CurveLength += m_Speed * m_Tick + m_LengthL;
		m_LengthL = 0;
		if (m_CurveLength > m_Length)
		{
			m_LengthL = m_CurveLength - m_Length;
			m_CurveLength = m_Length;
		}
		else if (m_CurveLength < 0)
		{
			m_LengthL = 0 + m_CurveLength;
			m_CurveLength = 0;
		}
	}

	m_Rotation += m_AngularSpeed * m_Tick;
	if (m_Rotation > pi * 2)
		m_Rotation -= pi * 2;
	else if (m_Rotation < 0)
		m_Rotation += pi * 2;
}

void CLight::Step()
{
	Move();
	vec2 dir(sin(m_Rotation), cos(m_Rotation));
	vec2 to2 = m_Pos + normalize(dir) * m_CurveLength;
	GameServer()->Collision()->IntersectNoLaser(m_Pos, to2, &m_To, 0);
}

void CLight::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CLight::Tick()
{

	if (Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		m_EvalTick = Server()->Tick();
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y,
				&Flags);
		if (index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
		Step();
	}

	HitCharacter();
	return;

}

void CLight::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_Pos)
			&& NetworkClipped(SnappingClient, m_To))
		return;

	CCharacter *Char = GameServer()->GetPlayerChar(SnappingClient);

	if(SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1
				|| GameServer()->m_apPlayers[SnappingClient]->m_Paused)
			&& GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID != SPEC_FREEVIEW)
		Char = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID);

	int Tick = (Server()->Tick() % Server()->TickSpeed()) % 6;

	if (Char && Char->IsAlive()
			&& m_Layer == LAYER_SWITCH
			&& !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()]
			&& (Tick))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(
			NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));

	if (!pObj)
		return;

	pObj->m_X = (int) m_Pos.x;
	pObj->m_Y = (int) m_Pos.y;

	if (Char && Char->Team() == TEAM_SUPER)
	{
		pObj->m_FromX = (int) m_Pos.x;
		pObj->m_FromY = (int) m_Pos.y;
	}
	else if (Char && m_Layer == LAYER_SWITCH
			&& GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()])
	{
		pObj->m_FromX = (int) m_To.x;
		pObj->m_FromY = (int) m_To.y;
	}
	else if (m_Layer != LAYER_SWITCH)
	{
		pObj->m_FromX = (int) m_To.x;
		pObj->m_FromY = (int) m_To.y;
	}
	else
	{
		pObj->m_FromX = (int) m_Pos.x;
		pObj->m_FromY = (int) m_Pos.y;
	}

	int StartTick = m_EvalTick;
	if (StartTick < Server()->Tick() - 4)
		StartTick = Server()->Tick() - 4;
	else if (StartTick > Server()->Tick())
		StartTick = Server()->Tick();
	pObj->m_StartTick = StartTick;
}
