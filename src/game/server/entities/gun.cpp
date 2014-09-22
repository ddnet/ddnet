/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>

#include "gun.h"
#include "plasma.h"

//////////////////////////////////////////////////
// CGun
//////////////////////////////////////////////////
CGun::CGun(CGameWorld *pGameWorld, vec2 Pos, bool Freeze, bool Explosive, int Layer, int Number)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Layer = Layer;
	m_Number = Number;
	m_LastFire = Server()->Tick();
	m_Pos = Pos;
	m_EvalTick = Server()->Tick();
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	
	GameWorld()->InsertEntity(this);
}


void CGun::Fire()
{
	CCharacter *Ents[MAX_CLIENTS];
	int IdInTeam[MAX_CLIENTS]; 
	int LenInTeam[MAX_CLIENTS];
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		IdInTeam[i] = -1;
		LenInTeam[i] = 0;
	}
	
	int Num = -1;
	Num =  GameServer()->m_World.FindEntities(m_Pos, g_Config.m_SvPlasmaRange, (CEntity**)Ents, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for (int i = 0; i < Num; i++)
	{
		CCharacter *Target = Ents[i];
		//now gun doesn't affect on super
		if(Target->Team() == TEAM_SUPER)
			continue;
		if(m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Target->Team()])
			continue;
		int res = GameServer()->Collision()->IntersectLine(m_Pos, Target->m_Pos,0,0,false);
		if (!res)
		{
			int Len = length(Target->m_Pos - m_Pos);
			if (LenInTeam[Target->Team()] == 0 || LenInTeam[Target->Team()] > Len)
			{
				LenInTeam[Target->Team()] = Len;
				IdInTeam[Target->Team()] = i;
			}
		}
	}
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IdInTeam[i] != -1)
		{
			CCharacter *Target = Ents[IdInTeam[i]];
			new CPlasma(&GameServer()->m_World, m_Pos, normalize(Target->m_Pos - m_Pos), m_Freeze, m_Explosive, i);
			m_LastFire = Server()->Tick();
		}
	}
	for (int i = 0; i < Num; i++)
	{
		CCharacter *Target = Ents[i];
		if (Target->IsAlive() && Target->Teams()->m_Core.GetSolo(Target->GetPlayer()->GetCID()))
		{
			if (IdInTeam[Target->Team()] != i)
			{
				int res = GameServer()->Collision()->IntersectLine(m_Pos, Target->m_Pos,0,0,false);
				if (!res)
				{
					new CPlasma(&GameServer()->m_World, m_Pos, normalize(Target->m_Pos - m_Pos), m_Freeze, m_Explosive, Target->Team());
					m_LastFire = Server()->Tick();
				}
			}
		}
	}
	
}
	
void CGun::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CGun::Tick()
{
	if (Server()->Tick()%int(Server()->TickSpeed()*0.15f)==0)
	{
		int Flags;
		m_EvalTick=Server()->Tick();
		int index = GameServer()->Collision()->IsMover(m_Pos.x,m_Pos.y, &Flags);
		if (index)
		{
			m_Core=GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos+=m_Core;
	}
	if (m_LastFire + Server()->TickSpeed() / g_Config.m_SvPlasmaPerSec <= Server()->Tick())
		Fire();

}

void CGun::Snap(int SnappingClient)
{	
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *Char = GameServer()->GetPlayerChar(SnappingClient);

	if((GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1
				|| GameServer()->m_apPlayers[SnappingClient]->m_Paused)
			&& GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID != SPEC_FREEVIEW)
		Char = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID);

	int Tick = (Server()->Tick()%Server()->TickSpeed())%11;
	if (Char && Char->IsAlive() && (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()]) && (!Tick)) return;
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));

	if (!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_Pos.x;
	pObj->m_FromY = (int)m_Pos.y;
	pObj->m_StartTick = m_EvalTick;
}
