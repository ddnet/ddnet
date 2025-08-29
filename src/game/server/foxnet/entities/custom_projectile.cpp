// made by fokkonaut

#include "custom_projectile.h"
#include <game/server/entities/character.h>
#include <game/server/player.h>

#include <game/generated/protocol.h>
#include <game/generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/teams.h>

#include <base/vmath.h>
#include <game/mapitems.h>
#include <game/server/entity.h>

CCustomProjectile::CCustomProjectile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir,
	bool Explosive, bool Freeze, bool Unfreeze, int Type, bool IsNormalWeapon, float Lifetime, float Accel, float Speed) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_CUSTOM_PROJECTILE, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	m_Core = normalize(Dir) * Speed;
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	m_Unfreeze = Unfreeze;
	m_Direction = Dir;
	m_EvalTick = Server()->Tick();
	m_LifeTime = Server()->TickSpeed() * Lifetime;
	m_Type = Type;
	m_Accel = Accel;

	m_Weapon = IsNormalWeapon;
	m_PrevPos = m_Pos;

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));

	GameWorld()->InsertEntity(this);
}

void CCustomProjectile::Reset()
{
	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CCustomProjectile::Tick()
{
	m_pOwner = 0;
	if(GameServer()->GetPlayerChar(m_Owner))
		m_pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(m_Owner >= 0 && !m_pOwner && Config()->m_SvDestroyBulletsOnDeath)
	{
		Reset();
		return;
	}

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwnerChar ? pOwnerChar->TeamMask() : CClientMask();

	m_LifeTime--;
	if(m_LifeTime <= 0)
	{
		Reset();
		return;
	}

	Move();
	HitCharacter();

	if(GameServer()->Collision()->IsSolid(m_Pos.x, m_Pos.y))
	{
		if(m_Explosive)
		{
			GameServer()->CreateExplosion(m_Pos, m_Owner, m_Type, m_Owner == -1, m_pOwner ? m_pOwner->Team() : -1, m_TeamMask);
			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
		}

		if(m_CollisionState == NOT_COLLIDED)
			m_CollisionState = COLLIDED_ONCE;
	}
	else if(m_CollisionState == COLLIDED_ONCE)
		m_CollisionState = COLLIDED_TWICE;

	// weapon teleport
	int x = GameServer()->Collision()->GetIndex(m_PrevPos, m_Pos);
	int z;
	if(Config()->m_SvOldTeleportWeapons)
		z = GameServer()->Collision()->IsTeleport(x);
	else
		z = GameServer()->Collision()->IsTeleportWeapon(x);
	// if(z && ((CGameControllerDDRace *)GameServer()->m_pController)->m_TeleOuts[z - 1].size())
	//{
	//	int Num = ((CGameControllerDDRace *)GameServer()->m_pController)->m_TeleOuts[z - 1].size();
	//	m_Pos = ((CGameControllerDDRace *)GameServer()->m_pController)->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
	//	m_EvalTick = Server()->Tick();
	// }

	m_PrevPos = m_Pos;
}

void CCustomProjectile::Move()
{
	m_Pos += m_Core;
	m_Core *= m_Accel;
}

void CCustomProjectile::HitCharacter()
{
	vec2 NewPos = m_Pos + m_Core;
	CCharacter *pHit = GameWorld()->IntersectCharacter(m_PrevPos, NewPos, 6.0f, NewPos, m_pOwner, m_Owner);
	if(!pHit)
		return;

	if(m_Freeze)
		pHit->Freeze();
	else if(m_Unfreeze)
		pHit->UnFreeze();

	if(m_Explosive)
	{
		GameServer()->CreateExplosion(m_Pos, m_Owner, m_Type, m_Owner == -1, pHit->Team(), m_TeamMask);
		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}
	// else
	// pHit->TakeDamage(vec2(0, 0), g_pData->m_Weapons.m_aId[GameServer()->GetWeaponType(m_Type)].m_Damage, m_Owner, m_Type);

	if(GameServer()->GetPlayerChar(m_Owner)->GetActiveWeapon() == WEAPON_HEARTGUN || GameServer()->GetPlayerChar(m_Owner)->GetPlayer()->m_Cosmetics.m_Ability == ABILITY_HEART)
	{
		pHit->SetEmote(EMOTE_HAPPY, Server()->Tick() + 2 * Server()->TickSpeed());
		GameServer()->SendEmoticon(pHit->GetPlayer()->GetCid(), EMOTICON_HEARTS, -1);
	}

	Reset();
}

void CCustomProjectile::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChar || !pSnapPlayer)
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	if(pOwnerChar->IsPaused())
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChar)
		if(!pOwnerChar->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChar->GetPlayer()->m_Vanish && SnappingClient != pOwnerChar->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	CCharacter *pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	if(SnappingClientVersion < VERSION_DDNET_ENTITY_NETOBJS)
	{
		int Tick = (Server()->Tick() % Server()->TickSpeed()) % ((m_Explosive) ? 6 : 20);
		if(pSnapChar && pSnapChar->IsAlive() && (m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pSnapChar->Team()] && (!Tick)))
			return;
	}

	if(m_Weapon)
	{
		if(m_Type >= NUM_WEAPONS && pSnapChar && pSnapChar->GetPlayer()->m_HideCosmetics && pOwnerChar->GetPlayer()->m_Cosmetics.m_Trail == TRAIL_DOT)
			return;

		CNetObj_DDRaceProjectile DDRaceProjectile;
		if(SnappingClientVersion >= VERSION_DDNET_ENTITY_NETOBJS)
		{
			CNetObj_DDNetProjectile *pDDNetProjectile = static_cast<CNetObj_DDNetProjectile *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPROJECTILE, GetId(), sizeof(CNetObj_DDNetProjectile)));
			if(!pDDNetProjectile)
			{
				return;
			}

			FillExtraInfo(pDDNetProjectile);
		}
		else if(SnappingClientVersion >= VERSION_DDNET_ANTIPING_PROJECTILE && FillExtraInfoLegacy(&DDRaceProjectile))
		{
			int Type = SnappingClientVersion < VERSION_DDNET_MSG_LEGACY ? (int)NETOBJTYPE_PROJECTILE : NETOBJTYPE_DDRACEPROJECTILE;
			void *pProj = Server()->SnapNewItem(Type, GetId(), sizeof(DDRaceProjectile));
			if(!pProj)
			{
				return;
			}
			mem_copy(pProj, &DDRaceProjectile, sizeof(DDRaceProjectile));
		}
		else
		{
			CNetObj_Projectile *pProj = Server()->SnapNewItem<CNetObj_Projectile>(GetId());
			if(!pProj)
			{
				return;
			}
			FillInfo(pProj);
		}
	}
	else if(GameServer()->GetClientVersion(SnappingClient) >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		CNetObj_DDNetPickup *pPickup = Server()->SnapNewItem<CNetObj_DDNetPickup>(GetId());
		if(!pPickup)
			return;

		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = m_Type;
		pPickup->m_Flags = PICKUPFLAG_NO_PREDICT;
	}
	else
	{
		CNetObj_Pickup *pPickup = Server()->SnapNewItem<CNetObj_Pickup>(GetId());
		if(!pPickup)
			return;

		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = m_Type;
	}
}

void CCustomProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x * 100.0f);
	pProj->m_VelY = (int)(m_Direction.y * 100.0f);
	pProj->m_StartTick = m_EvalTick;
	pProj->m_Type = m_Type;
}

bool CCustomProjectile::FillExtraInfoLegacy(CNetObj_DDRaceProjectile *pProj)
{
	const int MaxPos = 0x7fffffff / 100;
	if(absolute((int)m_Pos.y) + 1 >= MaxPos || absolute((int)m_Pos.x) + 1 >= MaxPos)
	{
		// If the modified data would be too large to fit in an integer, send normal data instead
		return false;
	}
	// Send additional/modified info, by modifying the fields of the netobj
	float Angle = -std::atan2(m_Direction.x, m_Direction.y);

	int Data = 0;
	Data |= (absolute(m_Owner) & 255) << 0;
	if(m_Owner < 0)
		Data |= LEGACYPROJECTILEFLAG_NO_OWNER;
	// This bit tells the client to use the extra info
	Data |= LEGACYPROJECTILEFLAG_IS_DDNET;
	// LEGACYPROJECTILEFLAG_BOUNCE_HORIZONTAL, LEGACYPROJECTILEFLAG_BOUNCE_VERTICAL

	if(m_Explosive)
		Data |= LEGACYPROJECTILEFLAG_EXPLOSIVE;
	if(m_Freeze)
		Data |= LEGACYPROJECTILEFLAG_FREEZE;

	pProj->m_X = (int)(m_Pos.x * 100.0f);
	pProj->m_Y = (int)(m_Pos.y * 100.0f);
	pProj->m_Angle = (int)(Angle * 1000000.0f);
	pProj->m_Data = Data;
	pProj->m_StartTick = m_EvalTick;
	pProj->m_Type = m_Type;
	return true;
}

void CCustomProjectile::FillExtraInfo(CNetObj_DDNetProjectile *pProj)
{
	int Flags = 0;
	if(m_Explosive)
	{
		Flags |= PROJECTILEFLAG_EXPLOSIVE;
	}
	if(m_Freeze)
	{
		Flags |= PROJECTILEFLAG_FREEZE;
	}

	if(m_Owner < 0)
	{
		pProj->m_VelX = round_to_int(m_Direction.x * 1e6f);
		pProj->m_VelY = round_to_int(m_Direction.y * 1e6f);
	}
	else
	{
		pProj->m_VelX = round_to_int(m_Direction.x);
		pProj->m_VelY = round_to_int(m_Direction.y);
		Flags |= PROJECTILEFLAG_NORMALIZE_VEL;
	}

	pProj->m_X = round_to_int(m_Pos.x * 100.0f);
	pProj->m_Y = round_to_int(m_Pos.y * 100.0f);
	pProj->m_Type = m_Type;
	pProj->m_StartTick = m_EvalTick;
	pProj->m_Owner = m_Owner;
	pProj->m_Flags = Flags;
	pProj->m_SwitchNumber = m_Number;
	pProj->m_TuneZone = m_TuneZone;
}
