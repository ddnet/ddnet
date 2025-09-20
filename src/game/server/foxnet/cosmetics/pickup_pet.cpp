// Made by qxdFox
#include "pickup_pet.h"
#include "game/server/entities/character.h"
#include <base/math.h>
#include <base/vmath.h>
#include <cmath>
#include <cstdlib>
#include <engine/shared/protocol.h>
#include <game/gamecore.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>
#include <generated/protocol.h>

CPickupPet::CPickupPet(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_PetMode = 1;
	m_CurType = POWERUP_ARMOR;
	m_SwitchDelay = Server()->Tick() + Server()->TickSpeed();
	GameWorld()->InsertEntity(this);
}

void CPickupPet::Reset()
{
	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	if(pOwner->Core()->m_FakeTuned)
	{
		GameServer()->ResetFakeTunes(pOwner->GetPlayer()->GetCid(), pOwner->GetOverriddenTuneZone());
	}
}

void CPickupPet::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	if(!pOwner->GetPlayer()->m_Cosmetics.m_PickupPet)
	{
		Reset();
		return;
	}

	if(m_PetMode == PET_MODE_AFK)
		PlayerAfkMode(pOwner);
	else if(m_PetMode == PET_MODE_FOLLOW)
		FollowMode(pOwner);
	else
		StaticMode(pOwner);
}

void CPickupPet::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	const CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChr || !pSnapPlayer)
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	CGameTeams Teams = GameServer()->m_pController->Teams();
	const int Team = pOwnerChr->Team();

	if(!Teams.SetMask(SnappingClient, Team))
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChr)
		if(!pOwnerChr->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	if(Server()->Tick() > m_SwitchDelay)
	{
		if(m_CurType == POWERUP_ARMOR)
			m_CurType = POWERUP_HEALTH;
		else
			m_CurType = POWERUP_ARMOR;

		m_SwitchDelay = Server()->Tick() + Server()->TickSpeed();
	}

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	const bool SixUp = Server()->IsSixup(SnappingClient);
	GameServer()->SnapPickup(CSnapContext(SnapVer, SixUp, SnappingClient), GetId(), m_Pos, m_CurType, -1, -1, PICKUPFLAG_NO_PREDICT);
}

void CPickupPet::PlayerAfkMode(CCharacter *pOwner)
{
	// ToDo: add some afk animations to the pet
}

void CPickupPet::FollowMode(CCharacter *pOwner)
{
	vec2 TargetPos = pOwner->GetPos();
	vec2 Offset = vec2(0.0f, 0.0f);
	Offset.y = -72;

	m_aSpeed = 0.1f;
	bool LookingLeft = pOwner->Core()->m_Angle > 402;

	if(abs(pOwner->Core()->m_Vel.x) < 2.5f)
	{
		if(pOwner->IsGrounded())
			Offset.y += 10.0f * sin(Server()->Tick() * 1.0f * pi / Server()->TickSpeed());
		Offset.x = 45;
	}
	else
	{
		Offset.x = 35;
	}

	bool FollowMouse = pOwner->GetPlayer()->m_PlayerFlags & PLAYERFLAG_AIM && pOwner->IsGrounded();

	int Zone = pOwner->GetOverriddenTuneZone();

	if(FollowMouse)
	{
		Offset = vec2(0.0f, 0.0f);
		m_aSpeed = 0.08f;
		TargetPos = pOwner->GetCursorPos();

		const float MaxDistance = 300.0f;
		vec2 PlayerPos = pOwner->GetPos();
		vec2 Direction = TargetPos - PlayerPos;
		float Distance = length(Direction);

		CTuningParams FakeTuning = Zone > 0 ? GameServer()->TuningList()[Zone] : *GameServer()->Tuning();

		if(Distance > MaxDistance)
		{
			Direction = normalize(Direction);
			TargetPos = PlayerPos + Direction * MaxDistance;

			FakeTuning.m_HookLength = MaxDistance;
		}
		else
			FakeTuning.m_HookLength = Distance;

		GameServer()->SendFakeTuningParams(pOwner->GetPlayer()->GetCid(), FakeTuning);
	}
	else if(pOwner->Core()->m_FakeTuned)
	{
		GameServer()->ResetFakeTunes(pOwner->GetPlayer()->GetCid(), Zone);
	}

	bool ThreeBlocksUp = GameServer()->Collision()->CheckPoint(pOwner->GetPos() + vec2(0, -3.0f * 32.0f));

	bool OneHalfBlocksUp = GameServer()->Collision()->CheckPoint(pOwner->GetPos() + vec2(0, -1.5f * 32.0f));
	bool OneHalfBlocksDown = GameServer()->Collision()->CheckPoint(pOwner->GetPos() + vec2(0, 1.5f * 32.0f));

	bool OneBlockUp = GameServer()->Collision()->CheckPoint(pOwner->GetPos() + vec2(0, -32.0f));
	bool OneBlockDown = GameServer()->Collision()->CheckPoint(pOwner->GetPos() + vec2(0, 32.0f));

	if(OneBlockUp)
	{
		TargetPos.y += 72.0f;
	}
	else if(OneHalfBlocksUp)
	{
		if(OneHalfBlocksDown)
			TargetPos.y += 100.0f;
		else
			TargetPos.y += 58.0f;
	}
	else if(ThreeBlocksUp)
	{
		TargetPos.y += 36.0f;
	}

	if(!LookingLeft)
		Offset.x = -Offset.x;

	m_aPos.x = TargetPos.x + Offset.x;
	m_aPos.y = TargetPos.y + Offset.y;

	for(int i = -20; i <= 0; i++)
	{
		float ExtraOffset = abs(i);

		if(TargetPos.x < m_aPos.x && GameServer()->Collision()->CheckPoint(m_aPos + vec2(abs(i) / 10.0f * 32.0f, 0.0f)))
		{
			Offset.x = ExtraOffset / 10.0f * 32.0f;
			if(i == 0 && OneBlockUp && !OneBlockDown)
				Offset.y += 48.0f;
		}
		else if(TargetPos.x > m_aPos.x && GameServer()->Collision()->CheckPoint(m_aPos + vec2(abs(i) / 10.0f * -32.0f, 0.0f)))
		{
			Offset.x = ExtraOffset / 10.0f * -32.0f;
			if(i == 0 && OneBlockUp && !OneBlockDown)
				Offset.y += 48.0f;
		}
	}

	TargetPos.x += Offset.x;
	TargetPos.y += Offset.y;

	vec2 NewPos = vec2(0.0f, 0.0f);
	NewPos.x = m_Pos.x + m_aSpeed * (TargetPos.x - m_Pos.x);
	NewPos.y = m_Pos.y + m_aSpeed * (TargetPos.y - m_Pos.y);

	// Check for collision with blocks
	bool CollidesLeft = NewPos.x < m_Pos.x && GameServer()->Collision()->CheckPoint(vec2(NewPos.x, m_Pos.y));
	bool CollidesRight = NewPos.x > m_Pos.x && GameServer()->Collision()->CheckPoint(vec2(NewPos.x, m_Pos.y));
	bool CollidesFloor = NewPos.y > m_Pos.y && GameServer()->Collision()->CheckPoint(vec2(m_Pos.x, NewPos.y));
	bool CollidesCeiling = NewPos.y < m_Pos.y && GameServer()->Collision()->CheckPoint(vec2(m_Pos.x, NewPos.y));

	if((!CollidesLeft && !CollidesRight && !CollidesFloor && !CollidesCeiling) || !FollowMouse)
		m_Pos = NewPos;
	else
	{
		if(!CollidesLeft && !CollidesRight)
			m_Pos.x = NewPos.x;

		if(!CollidesFloor && !CollidesCeiling)
			m_Pos.y = NewPos.y;

		if(CollidesFloor)
			m_Pos.y = floor(m_Pos.y);

		if(CollidesCeiling)
			m_Pos.y = ceil(m_Pos.y);

		if(CollidesLeft)
			m_Pos.x = ceil(m_Pos.x);

		if(CollidesRight)
			m_Pos.x = floor(m_Pos.x);
	}
}

void CPickupPet::StaticMode(CCharacter *pOwner)
{
	int Zone = pOwner->GetOverriddenTuneZone();

	if(pOwner->Core()->m_FakeTuned)
	{
		GameServer()->ResetFakeTunes(pOwner->GetPlayer()->GetCid(), Zone);
	}
}