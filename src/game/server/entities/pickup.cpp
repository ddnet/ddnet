/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"

#include "character.h"

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/physics/pickup.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/teamscore.h>

static constexpr int PICKUP_PHYSICS_RADIUS = 14;

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType, int Layer, int Number, int Flags) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, true, vec2(0, 0), PICKUP_PHYSICS_RADIUS)
{
	m_Core = vec2(0.0f, 0.0f);
	m_Type = Type;
	m_Subtype = SubType;

	m_Layer = Layer;
	m_Number = Number;
	m_Flags = Flags;

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	m_MarkedForDestroy = true;
}

void CPickup::Tick()
{
	CPickupPhysics<CPickup, CCharacter>::Tick(this);
}

void CPickup::CreatePickupSound(int Sound, CCharacter *pChr)
{
	GameServer()->CreateSound(m_Pos, Sound, pChr->TeamMask());
}

void CPickup::AnnounceWeaponPickup(CCharacter *pChr)
{
	if(pChr->GetPlayer())
		GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCid(), m_Subtype);
}

void CPickup::TickPaused()
{
}

void CPickup::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	bool Sixup = Server()->IsSixup(SnappingClient);

	if(SnappingClientVersion < VERSION_DDNET_ENTITY_NETOBJS)
	{
		CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);

		if(SnappingClient != SERVER_DEMO_CLIENT && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->SpectatorId() != SPEC_FREEVIEW)
			pChar = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->SpectatorId());

		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
		if(pChar && pChar->IsAlive() && m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChar->Team()] && !Tick)
			return;
	}

	GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, Sixup, SnappingClient), GetId().value(), m_Pos, m_Type, m_Subtype, m_Number, m_Flags);
}
