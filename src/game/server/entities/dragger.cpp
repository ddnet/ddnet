/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "dragger.h"
#include <engine/config.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/player.h>
#include <game/server/teams.h>
#include <game/version.h>

#include "character.h"

CDragger::CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool NW,
	int CaughtTeam, int Layer, int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_TargetID = -1;
	m_Layer = Layer;
	m_Number = Number;
	m_Pos = Pos;
	m_Strength = Strength;
	m_EvalTick = Server()->Tick();
	m_NW = NW;
	m_CaughtTeam = CaughtTeam;
	GameWorld()->InsertEntity(this);

	for(int &SoloID : m_SoloIDs)
	{
		SoloID = -1;
	}

	for(int &SoloEntID : m_SoloEntIDs)
	{
		SoloEntID = -1;
	}
}

void CDragger::Move()
{
	if(m_TargetID >= 0)
	{
		CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetID);
		if(!pTarget || pTarget->m_Super || pTarget->IsPaused() || (m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pTarget->Team()]))
		{
			m_TargetID = -1;
		}
	}

	CCharacter *TempEnts[MAX_CLIENTS];
	mem_zero(TempEnts, sizeof(TempEnts));

	for(int &SoloEntID : m_SoloEntIDs)
	{
		SoloEntID = -1;
	}

	int Num = GameServer()->m_World.FindEntities(m_Pos, g_Config.m_SvDraggerRange,
		(CEntity **)TempEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	int Id = -1;
	int MinLen = 0;
	CCharacter *Temp;
	for(int i = 0; i < Num; i++)
	{
		Temp = TempEnts[i];
		m_SoloEntIDs[i] = Temp->GetPlayer()->GetCID();
		if(Temp->Team() != m_CaughtTeam)
		{
			m_SoloEntIDs[i] = -1;
			continue;
		}
		if(m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Temp->Team()])
		{
			m_SoloEntIDs[i] = -1;
			continue;
		}
		int Res =
			m_NW ?
				GameServer()->Collision()->IntersectNoLaserNW(m_Pos, Temp->m_Pos, 0, 0) :
				GameServer()->Collision()->IntersectNoLaser(m_Pos, Temp->m_Pos, 0, 0);

		if(Res == 0)
		{
			int Len = length(Temp->m_Pos - m_Pos);
			if(MinLen == 0 || MinLen > Len)
			{
				MinLen = Len;
				Id = i;
			}

			if(!Temp->Teams()->m_Core.GetSolo(Temp->GetPlayer()->GetCID()))
				m_SoloEntIDs[i] = -1;
		}
		else
		{
			m_SoloEntIDs[i] = -1;
		}
	}

	if(m_TargetID < 0)
		m_TargetID = Id != -1 ? TempEnts[Id]->GetPlayer()->GetCID() : -1;

	if(m_TargetID >= 0)
	{
		const CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetID);
		for(auto &SoloEntID : m_SoloEntIDs)
		{
			if(GameServer()->GetPlayerChar(SoloEntID) == pTarget)
				SoloEntID = -1;
		}
	}
}

void CDragger::Drag()
{
	if(m_TargetID < 0)
		return;

	CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetID);

	for(int i = -1; i < MAX_CLIENTS; i++)
	{
		if(i >= 0)
			pTarget = GameServer()->GetPlayerChar(m_SoloEntIDs[i]);

		if(!pTarget)
			continue;

		int Res = 0;
		if(!m_NW)
			Res = GameServer()->Collision()->IntersectNoLaser(m_Pos,
				pTarget->m_Pos, 0, 0);
		else
			Res = GameServer()->Collision()->IntersectNoLaserNW(m_Pos,
				pTarget->m_Pos, 0, 0);
		if(Res || length(m_Pos - pTarget->m_Pos) > g_Config.m_SvDraggerRange)
		{
			pTarget = 0;
			if(i == -1)
				m_TargetID = -1;
			else
				m_SoloEntIDs[i] = -1;
		}
		else if(length(m_Pos - pTarget->m_Pos) > 28)
		{
			vec2 Temp = pTarget->Core()->m_Vel + (normalize(m_Pos - pTarget->m_Pos) * m_Strength);
			pTarget->Core()->m_Vel = ClampVel(pTarget->m_MoveRestrictions, Temp);
		}
	}
}

void CDragger::Reset()
{
	m_MarkedForDestroy = true;
}

void CDragger::Tick()
{
	if(((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.GetTeamState(m_CaughtTeam) == CGameTeams::TEAMSTATE_EMPTY)
		return;
	if(Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		m_EvalTick = Server()->Tick();
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y,
			&Flags);
		if(index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
		Move();
	}
	Drag();
}

void CDragger::Snap(int SnappingClient)
{
	if(((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.GetTeamState(m_CaughtTeam) == CGameTeams::TEAMSTATE_EMPTY)
		return;

	if(NetworkClipped(SnappingClient, m_Pos))
		return;

	CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);

	if(SnappingClient != SERVER_DEMO_CLIENT && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID != SPEC_FREEVIEW)
		pChar = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID);

	if(pChar && pChar->Team() != m_CaughtTeam)
		return;

	int SnappingClientVersion = SnappingClient != SERVER_DEMO_CLIENT ? GameServer()->GetClientVersion(SnappingClient) : CLIENT_VERSIONNR;

	CNetObj_EntityEx *pEntData = 0;
	if(SnappingClientVersion >= VERSION_DDNET_SWITCH)
	{
		pEntData = static_cast<CNetObj_EntityEx *>(Server()->SnapNewItem(NETOBJTYPE_ENTITYEX, GetID(), sizeof(CNetObj_EntityEx)));
		if(pEntData)
		{
			pEntData->m_SwitchNumber = m_Number;
			pEntData->m_Layer = m_Layer;
			pEntData->m_EntityClass = clamp(ENTITYCLASS_DRAGGER_WEAK + round_to_int(m_Strength) - 1, (int)ENTITYCLASS_DRAGGER_WEAK, (int)ENTITYCLASS_DRAGGER_STRONG);
		}
	}

	CCharacter *pTarget = m_TargetID < 0 ? 0 : GameServer()->GetPlayerChar(m_TargetID);

	for(int &SoloID : m_SoloIDs)
	{
		if(SoloID == -1)
			break;

		Server()->SnapFreeID(SoloID);
		SoloID = -1;
	}

	int pos = 0;

	for(int i = -1; i < MAX_CLIENTS; i++)
	{
		if(i >= 0)
		{
			pTarget = GameServer()->GetPlayerChar(m_SoloEntIDs[i]);

			if(!pTarget)
				continue;
		}

		if(pTarget && NetworkClipped(SnappingClient, pTarget->m_Pos))
			continue;

		if(i != -1 || SnappingClientVersion < VERSION_DDNET_SWITCH)
		{
			int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
			if(pChar && m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChar->Team()] && (!Tick))
				continue;
		}

		// send to spectators only active draggers and some inactive from team 0
		if(!pChar && !pTarget && m_CaughtTeam != 0)
			continue;

		if(pChar && pTarget && pTarget->GetPlayer()->GetCID() != pChar->GetPlayer()->GetCID() && ((pChar->GetPlayer()->m_ShowOthers == SHOW_OTHERS_OFF && (pChar->Teams()->m_Core.GetSolo(SnappingClient) || pChar->Teams()->m_Core.GetSolo(pTarget->GetPlayer()->GetCID()))) || (pChar->GetPlayer()->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM && !pTarget->SameTeam(SnappingClient))))
		{
			continue;
		}

		CNetObj_Laser *obj;

		if(i == -1)
		{
			obj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(
				NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		}
		else
		{
			m_SoloIDs[pos] = Server()->SnapNewID();
			obj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem( // TODO: Have to free IDs again?
				NETOBJTYPE_LASER, m_SoloIDs[pos], sizeof(CNetObj_Laser)));
			pos++;
		}

		if(!obj)
			continue;
		obj->m_X = (int)m_Pos.x;
		obj->m_Y = (int)m_Pos.y;
		if(pTarget)
		{
			obj->m_FromX = (int)pTarget->m_Pos.x;
			obj->m_FromY = (int)pTarget->m_Pos.y;
		}
		else
		{
			obj->m_FromX = (int)m_Pos.x;
			obj->m_FromY = (int)m_Pos.y;
		}

		if(pEntData && i == -1)
		{
			obj->m_StartTick = 0;
		}
		else
		{
			int StartTick = m_EvalTick;
			if(StartTick < Server()->Tick() - 4)
				StartTick = Server()->Tick() - 4;
			else if(StartTick > Server()->Tick())
				StartTick = Server()->Tick();
			obj->m_StartTick = StartTick;
		}
	}
}

CDraggerTeam::CDraggerTeam(CGameWorld *pGameWorld, vec2 Pos, float Strength,
	bool NW, int Layer, int Number)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_Draggers[i] = new CDragger(pGameWorld, Pos, Strength, NW, i, Layer, Number);
	}
}

//CDraggerTeam::~CDraggerTeam()
//{
//	for (int i = 0; i < MAX_CLIENTS; ++i)
//	{
//		delete m_Draggers[i];
//	}
//}
