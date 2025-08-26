#include "flyingpoint.h"
#include "game/server/entities/character.h"
#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/generated/protocol.h>
#include <game/server/gameworld.h>

CFlyingPoint::CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel, vec2 ToPos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_FLYINGPOINT, Pos)
{
	m_Pos = Pos;
	m_PrevPos = Pos;
	m_InitialVel = InitialVel;
	m_Owner = Owner;
	m_To = To;
	m_ToPos = ToPos;
	m_InitialAmount = 1.0f;

	GameWorld()->InsertEntity(this);
}

void CFlyingPoint::Reset()
{
	CCharacter *pToChar = GameServer()->GetPlayerChar(m_To);
	if(pToChar)
	{
		pToChar->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		GameServer()->CreateSoundGlobal(SOUND_PICKUP_HEALTH, m_To);
	}

	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CFlyingPoint::Tick()
{
	vec2 ToPos = m_ToPos;

	if(m_To != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_To);
		if(!pChr)
		{
			Reset();
			return;
		}

		ToPos = pChr->GetPos();
	}

	float Dist = distance(m_Pos, ToPos);
	if(Dist < 24.0f)
	{
		Reset();
		return;
	}

	vec2 Dir = normalize(ToPos - m_Pos);
	m_Pos += Dir * std::clamp(Dist, 1.0f, 24.0f) * (1.0f - m_InitialAmount) + m_InitialVel * m_InitialAmount;

	m_InitialAmount *= 0.98f;
	m_PrevPos = m_Pos;
}

void CFlyingPoint::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChar || !pSnapPlayer)
		return;

	// if(pSnapPlayer->m_HideCosmetics)
	//	return;

	if(pOwnerChar->IsPaused())
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChar)
		if(!pOwnerChar->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChar->GetPlayer()->m_Vanish && SnappingClient != pOwnerChar->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;
	CNetObj_Projectile *pProj = Server()->SnapNewItem<CNetObj_Projectile>(GetId());
	if(!pProj)
		return;

	pProj->m_X = (int)(m_Pos.x);
	pProj->m_Y = (int)(m_Pos.y);
	pProj->m_VelX = 0;
	pProj->m_VelY = 0;
	pProj->m_StartTick = 0;
	pProj->m_Type = WEAPON_HAMMER;
}