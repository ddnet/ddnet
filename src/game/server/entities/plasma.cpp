/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "plasma.h"

#include "character.h"

#include <engine/server.h>

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/physics/plasma.h>
#include <game/server/gamecontext.h>
#include <game/teamscore.h>

CPlasma::CPlasma(CGameWorld *pGameWorld, vec2 Pos, vec2 Dir, bool Freeze,
	bool Explosive, int ForClientId) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, true)
{
	m_Pos = Pos;
	m_Number = 0;
	m_Layer = LAYER_GAME;
	m_Core = Dir;
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	m_ForClientId = ForClientId;
	m_EvalTick = Server()->Tick();
	m_LifeTime = Server()->TickSpeed() * 1.5f;

	GameWorld()->InsertEntity(this);
}

void CPlasma::Tick()
{
	CPlasmaPhysics<CPlasma, CCharacter>::Tick(this);
}

void CPlasma::Explode(CCharacter *pTarget)
{
	GameServer()->CreateExplosion(
		m_Pos, m_ForClientId, WEAPON_GRENADE, true, pTarget->Team(), pTarget->TeamMask());
}

void CPlasma::Reset()
{
	m_MarkedForDestroy = true;
}

void CPlasma::Snap(int SnappingClient)
{
	// Only players who can see the targeted player can see the plasma bullet
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_ForClientId);
	if(!pTarget || !pTarget->CanSnapCharacter(SnappingClient))
	{
		return;
	}

	// Only players with the plasma bullet in their field of view or who want to see everything will receive the snap
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);

	int Subtype = (m_Explosive ? 1 : 0) | (m_Freeze ? 2 : 0);
	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSixup(SnappingClient), SnappingClient), GetId().value(),
		m_Pos, m_Pos, m_EvalTick, m_ForClientId, LASERTYPE_PLASMA, Subtype, m_Number);
}

void CPlasma::SwapClients(int Client1, int Client2)
{
	m_ForClientId = m_ForClientId == Client1 ? Client2 : (m_ForClientId == Client2 ? Client1 : m_ForClientId);
}
