/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "dragger.h"
#include "character.h"
#include "dragger_beam.h"

#include <engine/server.h>
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/mapitems.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

CDragger::CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool IgnoreWalls, int Layer, int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Core = vec2(0.0f, 0.0f);
	m_Pos = Pos;
	m_Strength = Strength;
	m_IgnoreWalls = IgnoreWalls;
	m_Layer = Layer;
	m_Number = Number;
	m_EvalTick = Server()->Tick();

	for(auto &TargetId : m_aTargetIdInTeam)
	{
		TargetId = -1;
	}
	mem_zero(m_apDraggerBeam, sizeof(m_apDraggerBeam));
	GameWorld()->InsertEntity(this);
}

void CDragger::Tick()
{
	if(Server()->Tick() % (int)(Server()->TickSpeed() * 0.15f) == 0)
	{
		m_EvalTick = Server()->Tick();
		GameServer()->Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;

		// Adopt the new position for all outgoing laser beams
		for(auto &DraggerBeam : m_apDraggerBeam)
		{
			if(DraggerBeam != nullptr)
			{
				DraggerBeam->SetPos(m_Pos);
			}
		}

		LookForPlayersToDrag();
	}
}

void CDragger::LookForPlayersToDrag()
{
	// Create a list of players who are in the range of the dragger
	CEntity *apPlayersInRange[MAX_CLIENTS];
	mem_zero(apPlayersInRange, sizeof(apPlayersInRange));

	int NumPlayersInRange = GameServer()->m_World.FindEntities(m_Pos,
		g_Config.m_SvDraggerRange - CCharacterCore::PhysicalSize(),
		apPlayersInRange, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	// The closest player (within range) in a team is selected as the target
	int aClosestTargetIdInTeam[MAX_CLIENTS];
	bool aCanStillBeTeamTarget[MAX_CLIENTS];
	bool aIsTarget[MAX_CLIENTS];
	int aMinDistInTeam[MAX_CLIENTS];
	mem_zero(aCanStillBeTeamTarget, sizeof(aCanStillBeTeamTarget));
	mem_zero(aMinDistInTeam, sizeof(aMinDistInTeam));
	mem_zero(aIsTarget, sizeof(aIsTarget));
	for(int &TargetId : aClosestTargetIdInTeam)
	{
		TargetId = -1;
	}

	for(int i = 0; i < NumPlayersInRange; i++)
	{
		CCharacter *pTarget = static_cast<CCharacter *>(apPlayersInRange[i]);
		const int &TargetTeam = pTarget->Team();

		// Do not create a dragger beam for super player
		if(TargetTeam == TEAM_SUPER)
		{
			continue;
		}
		// If the dragger is disabled for the target's team, no dragger beam will be generated
		if(m_Layer == LAYER_SWITCH && m_Number > 0 &&
			!Switchers()[m_Number].m_aStatus[TargetTeam])
		{
			continue;
		}

		// Dragger beams can be created only for reachable, alive players
		int IsReachable =
			m_IgnoreWalls ?
				!GameServer()->Collision()->IntersectNoLaserNoWalls(m_Pos, pTarget->m_Pos, nullptr, nullptr) :
				!GameServer()->Collision()->IntersectNoLaser(m_Pos, pTarget->m_Pos, nullptr, nullptr);
		if(IsReachable && pTarget->IsAlive())
		{
			const int &TargetClientId = pTarget->GetPlayer()->GetCid();
			// Solo players are dragged independently from the rest of the team
			if(pTarget->Teams()->m_Core.GetSolo(TargetClientId))
			{
				aIsTarget[TargetClientId] = true;
			}
			else
			{
				int Distance = distance(pTarget->m_Pos, m_Pos);
				if(aMinDistInTeam[TargetTeam] == 0 || aMinDistInTeam[TargetTeam] > Distance)
				{
					aMinDistInTeam[TargetTeam] = Distance;
					aClosestTargetIdInTeam[TargetTeam] = TargetClientId;
				}
				aCanStillBeTeamTarget[TargetClientId] = true;
			}
		}
	}

	// Set the closest player for each team as a target if the team does not have a target player yet
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if((m_aTargetIdInTeam[i] != -1 && !aCanStillBeTeamTarget[m_aTargetIdInTeam[i]]) || m_aTargetIdInTeam[i] == -1)
		{
			m_aTargetIdInTeam[i] = aClosestTargetIdInTeam[i];
		}
		if(m_aTargetIdInTeam[i] != -1)
		{
			aIsTarget[m_aTargetIdInTeam[i]] = true;
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// Create Dragger Beams which have not been created yet
		if(aIsTarget[i] && m_apDraggerBeam[i] == nullptr)
		{
			m_apDraggerBeam[i] = new CDraggerBeam(&GameServer()->m_World, this, m_Pos, m_Strength, m_IgnoreWalls, i, m_Layer, m_Number);
			// The generated dragger beam is placed in the first position in the tick sequence and would therefore
			// no longer be executed automatically in this tick. To execute the dragger beam nevertheless already
			// this tick we call it manually (we do this to keep the old game logic)
			m_apDraggerBeam[i]->Tick();
		}
		// Remove dragger beams that have not yet been deleted
		else if(!aIsTarget[i] && m_apDraggerBeam[i] != nullptr)
		{
			m_apDraggerBeam[i]->Reset();
		}
	}
}

void CDragger::RemoveDraggerBeam(int ClientId)
{
	m_apDraggerBeam[ClientId] = nullptr;
}

bool CDragger::WillDraggerBeamUseDraggerId(int TargetClientId, int SnappingClientId)
{
	// For each snapping client, this must return true for at most one target (i.e. only one of the dragger beams),
	// in which case the dragger itself must not be snapped
	CCharacter *pTargetChar = GameServer()->GetPlayerChar(TargetClientId);
	CCharacter *pSnapChar = GameServer()->GetPlayerChar(SnappingClientId);
	if(pTargetChar && pSnapChar && m_apDraggerBeam[TargetClientId] != nullptr)
	{
		const int SnapTeam = pSnapChar->Team();
		const int TargetTeam = pTargetChar->Team();
		if(SnapTeam == TargetTeam && SnapTeam < MAX_CLIENTS)
		{
			if(pSnapChar->Teams()->m_Core.GetSolo(SnappingClientId) || m_aTargetIdInTeam[SnapTeam] < 0)
			{
				return SnappingClientId == TargetClientId;
			}
			else
			{
				return m_aTargetIdInTeam[SnapTeam] == TargetClientId;
			}
		}
	}
	return false;
}

void CDragger::Reset()
{
	m_MarkedForDestroy = true;
}

void CDragger::Snap(int SnappingClient)
{
	// Only players with the dragger in their field of view or who want to see everything will receive the snap
	if(NetworkClipped(SnappingClient))
		return;

	// Send the dragger in its resting position if the player would not otherwise see a dragger beam within its own team
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(WillDraggerBeamUseDraggerId(i, SnappingClient))
		{
			return;
		}
	}

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);

	int Subtype = (m_IgnoreWalls ? 1 : 0) | (clamp(round_to_int(m_Strength - 1.f), 0, 2) << 1);

	int StartTick;
	if(SnappingClientVersion >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		StartTick = -1;
	}
	else
	{
		// Emulate turned off blinking dragger for old clients
		CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);
		if(SnappingClient != SERVER_DEMO_CLIENT &&
			(GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS ||
				GameServer()->m_apPlayers[SnappingClient]->IsPaused()) &&
			GameServer()->m_apPlayers[SnappingClient]->m_SpectatorId != SPEC_FREEVIEW)
			pChar = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->m_SpectatorId);

		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
		if(pChar && m_Layer == LAYER_SWITCH && m_Number > 0 &&
			!Switchers()[m_Number].m_aStatus[pChar->Team()] && !Tick)
			return;

		StartTick = m_EvalTick;
		if(StartTick < Server()->Tick() - 4)
			StartTick = Server()->Tick() - 4;
		else if(StartTick > Server()->Tick())
			StartTick = Server()->Tick();
	}

	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion), GetId(),
		m_Pos, m_Pos, StartTick, -1, LASERTYPE_DRAGGER, Subtype, m_Number);
}

void CDragger::SwapClients(int Client1, int Client2)
{
	std::swap(m_apDraggerBeam[Client1], m_apDraggerBeam[Client2]);
	for(int &TargetId : m_aTargetIdInTeam)
	{
		TargetId = TargetId == Client1 ? Client2 : TargetId == Client2 ? Client1 : TargetId;
	}
}
