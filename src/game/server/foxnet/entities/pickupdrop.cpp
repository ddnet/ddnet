// Made by qxdFox
#include <game/server/entities/character.h>
#include <game/server/player.h>

#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/teams.h>

#include <generated/protocol.h>

#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <game/server/entities/pickup.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>
#include <game/teamscore.h>

#include <engine/shared/protocol.h>

#include <base/vmath.h>
#include <vector>

#include "pickupdrop.h"

CPickupDrop::CPickupDrop(CGameWorld *pGameWorld, int LastOwner, vec2 Pos, int Team, int TeleCheckpoint, vec2 Dir, int Lifetime, int Type) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUPDROP, Pos, 28)
{
	m_LastOwner = LastOwner;
	m_PrevPos = m_Pos;
	m_Pos = Pos;
	m_Team = Team;
	m_Vel = Dir;
	m_Lifetime = Lifetime * Server()->TickSpeed();
	m_Type = Type;
	m_TuneZone = -1;
	m_TeleCheckpoint = TeleCheckpoint;

	if(CCharacter *pChr = GameServer()->GetPlayerChar(LastOwner))
		m_TeamMask = pChr->TeamMask();
	else
		m_TeamMask = CClientMask().set();

	m_PickupDelay = Server()->TickSpeed() * 1.5f;
	m_GroundElasticity = vec2(0.5f, 0.5f);

	for(int i = 0; i < 2; i++)
		m_aIds[i] = Server()->SnapNewId();

	GameWorld()->InsertEntity(this);
}

void CPickupDrop::Reset(bool PickedUp)
{
	for(int i = 0; i < 2; i++)
		Server()->SnapFreeId(m_aIds[i]);
	Server()->SnapFreeId(GetId());

	if(m_LastOwner >= 0)
	{
		if(CPlayer *pPl = GameServer()->m_apPlayers[m_LastOwner])
		{
			for(size_t i = 0; i < pPl->m_vPickupDrops.size(); i++)
			{
				if(pPl->m_vPickupDrops[i] == this)
					pPl->m_vPickupDrops.erase(pPl->m_vPickupDrops.begin() + i);
			}
		}
	}

	if(!PickedUp)
		GameServer()->CreateDeath(m_Pos, m_LastOwner, m_TeamMask);

	GameWorld()->RemoveEntity(this);
}

bool CPickupDrop::IsSwitchActiveCb(int Number, void *pUser)
{
	CPickupDrop *pThis = (CPickupDrop *)pUser;
	auto &aSwitchers = pThis->Switchers();
	return !aSwitchers.empty() && pThis->m_Team != TEAM_SUPER && aSwitchers[Number].m_aStatus[pThis->m_Team];
}

void CPickupDrop::Tick()
{
	if(GameLayerClipped(m_Pos))
	{
		Reset();
		return;
	}
	if(m_LastOwner >= 0 && (!GameServer()->m_apPlayers[m_LastOwner] && g_Config.m_SvResetDropsOnLeave))
	{
		Reset();
		return;
	}
	m_Lifetime--;
	if(m_Lifetime <= 0)
	{
		Reset();
		return;
	}
	if(CheckArmor())
		return;
	if(CollectItem())
		return;

	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	m_MoveRestrictions = Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos, 18.0f, CurrentIndex);

	if(!m_TuneZone)
		m_Vel.y += GameServer()->Tuning()->m_Gravity;
	else
		m_Vel.y += GameServer()->TuningList()[m_TuneZone].m_Gravity;

	if(Collision()->IsSpeedup(CurrentIndex))
	{
		vec2 Direction, TempVel = m_Vel;
		int Force, Type, MaxSpeed = 0;
		Collision()->GetSpeedup(CurrentIndex, &Direction, &Force, &MaxSpeed, &Type);

		if(Type == TILE_SPEED_BOOST_OLD)
		{
			float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
			if(Force == 255 && MaxSpeed)
			{
				m_Vel = Direction * (MaxSpeed / 5);
			}
			else
			{
				if(MaxSpeed > 0 && MaxSpeed < 5)
					MaxSpeed = 5;
				if(MaxSpeed > 0)
				{
					if(Direction.x > 0.0000001f)
						SpeederAngle = -std::atan(Direction.y / Direction.x);
					else if(Direction.x < 0.0000001f)
						SpeederAngle = std::atan(Direction.y / Direction.x) + 2.0f * std::asin(1.0f);
					else if(Direction.y > 0.0000001f)
						SpeederAngle = std::asin(1.0f);
					else
						SpeederAngle = std::asin(-1.0f);

					if(SpeederAngle < 0)
						SpeederAngle = 4.0f * std::asin(1.0f) + SpeederAngle;

					if(TempVel.x > 0.0000001f)
						TeeAngle = -std::atan(TempVel.y / TempVel.x);
					else if(TempVel.x < 0.0000001f)
						TeeAngle = std::atan(TempVel.y / TempVel.x) + 2.0f * std::asin(1.0f);
					else if(TempVel.y > 0.0000001f)
						TeeAngle = std::asin(1.0f);
					else
						TeeAngle = std::asin(-1.0f);

					if(TeeAngle < 0)
						TeeAngle = 4.0f * std::asin(1.0f) + TeeAngle;

					TeeSpeed = std::sqrt(std::pow(TempVel.x, 2) + std::pow(TempVel.y, 2));

					DiffAngle = SpeederAngle - TeeAngle;
					SpeedLeft = MaxSpeed / 5.0f - std::cos(DiffAngle) * TeeSpeed;
					if(absolute((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
						TempVel += Direction * Force;
					else if(absolute((int)SpeedLeft) > Force)
						TempVel += Direction * -Force;
					else
						TempVel += Direction * SpeedLeft;
				}
				else
					TempVel += Direction * Force;

				m_Vel = ClampVel(m_MoveRestrictions, TempVel);
			}
		}
		else if(Type == TILE_SPEED_BOOST)
		{
			constexpr float MaxSpeedScale = 5.0f;
			if(MaxSpeed == 0)
			{
				float MaxRampSpeed = GetTuning(m_TuneZone)->m_VelrampRange / (50 * log(maximum((float)GetTuning(m_TuneZone)->m_VelrampCurvature, 1.01f)));
				MaxSpeed = maximum(MaxRampSpeed, GetTuning(m_TuneZone)->m_VelrampStart / 50) * MaxSpeedScale;
			}

			// (signed) length of projection
			float CurrentDirectionalSpeed = dot(Direction, m_Vel);
			float TempMaxSpeed = MaxSpeed / MaxSpeedScale;
			if(CurrentDirectionalSpeed + Force > TempMaxSpeed)
				TempVel += Direction * (TempMaxSpeed - CurrentDirectionalSpeed);
			else
				TempVel += Direction * Force;

			m_Vel = ClampVel(m_MoveRestrictions, TempVel);
		}
	}

	// tiles
	std::vector<int> vIndices = Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if(!vIndices.empty())
	{
		for(int &Index : vIndices)
		{
			HandleTiles(Index);
		}
	}
	else
	{
		HandleTiles(CurrentIndex);
	}

	bool Grounded = false;
	Collision()->MoveBox(&m_Pos, &m_Vel, CCharacterCore::PhysicalSizeVec2(), m_GroundElasticity, &Grounded);
	if(Grounded)
		m_Vel.x *= 0.88f;
	m_Vel.x *= 0.98f;

	if(m_InsideFreeze)
	{
		m_Vel.y -= 0.05f; // slowly float up
		if(!m_TuneZone)
			m_Vel.y -= GameServer()->Tuning()->m_Gravity;
		else
			m_Vel.y -= GameServer()->TuningList()[m_TuneZone].m_Gravity;
		m_InsideFreeze = false; // Reset for the next tick
	}

	m_PrevPos = m_Pos;
}

bool CPickupDrop::CheckArmor()
{
	CPickup *apEnts[9];
	int Num = GameWorld()->FindEntities(m_Pos, 34, (CEntity **)apEnts, 9, CGameWorld::ENTTYPE_PICKUP);

	for(int i = 0; i < Num; i++)
	{
		switch(apEnts[i]->Type())
		{
		case POWERUP_ARMOR:
		{
			if(m_Type > WEAPON_GUN && m_Type < NUM_WEAPONS && apEnts[i]->GetOwnerId() < 0)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
				Reset(false);
				return true;
			}
		}
		break;

		case POWERUP_ARMOR_SHOTGUN:
		{
			if(m_Type == WEAPON_SHOTGUN && apEnts[i]->GetOwnerId() < 0)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
				Reset(false);
				return true;
			}
		}
		break;

		case POWERUP_ARMOR_GRENADE:
		{
			if(m_Type == WEAPON_GRENADE && apEnts[i]->GetOwnerId() < 0)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
				Reset();
				return true;
			}
		}
		break;

		case POWERUP_ARMOR_NINJA:
		{
			if(m_Type == WEAPON_NINJA && apEnts[i]->GetOwnerId() < 0)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
				Reset();
				return true;
			}
		}
		break;

		case POWERUP_ARMOR_LASER:
		{
			if(m_Type == WEAPON_LASER && apEnts[i]->GetOwnerId() < 0)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
				Reset();
				return true;
			}
		}
		break;

		default:
			break;
		}
	}
	return false;
}

bool CPickupDrop::IsGrounded()
{
	if(Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if(Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;

	int MoveRestrictionsBelow = Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f);
	return (MoveRestrictionsBelow & CANTMOVE_DOWN) != 0;
}

bool CPickupDrop::CollectItem()
{
	if(m_PickupDelay > 0)
	{
		m_PickupDelay--;
		return false;
	}

	float Radius = 32.0f;
	CCharacter *pClosest = GameServer()->m_World.ClosestCharacter(m_Pos, Radius, this);
	if(!pClosest)
		return false;
	if(m_Team != TEAM_SUPER && pClosest->Team() != TEAM_SUPER && m_Team != pClosest->Team())
		return false;
	if(pClosest->Core()->m_aWeapons[m_Type].m_Got)
		return false;

	pClosest->GiveWeapon(m_Type);
	pClosest->SetActiveWeapon(m_Type);
	GameServer()->CreateSound(pClosest->m_Pos, SOUND_PICKUP_HEALTH, pClosest->TeamMask());

	if(pClosest->GetPlayer())
		GameServer()->SendWeaponPickup(pClosest->GetPlayer()->GetCid(), m_Type);

	Reset(true);
	return true;
}

void CPickupDrop::HandleTiles(int Index)
{
	int MapIndex = Index;
	// int PureMapIndex = Collision()->GetPureMapIndex(m_Pos);
	m_TileIndex = Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = Collision()->GetFrontTileIndex(MapIndex);

	int TeleCheckpoint = Collision()->IsTeleCheckpoint(MapIndex);
	if(TeleCheckpoint)
		m_TeleCheckpoint = TeleCheckpoint;

	m_Vel = ClampVel(m_MoveRestrictions, m_Vel);
	if(g_Config.m_SvDropsInFreezeFloat && (m_TileIndex == TILE_FREEZE || m_TileFIndex == TILE_FREEZE))
	{
		if(g_Config.m_SvDropsInFreezeFloat)
			m_InsideFreeze = true;
	}

	// teleporters
	int z = Collision()->IsTeleport(MapIndex);
	if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons && z && !Collision()->TeleOuts(z - 1).empty())
	{
		int TeleOut = GameWorld()->m_Core.RandomOr0(Collision()->TeleOuts(z - 1).size());
		m_Pos = Collision()->TeleOuts(z - 1)[TeleOut];
		return;
	}
	int evilz = Collision()->IsEvilTeleport(MapIndex);
	if(evilz && !Collision()->TeleOuts(evilz - 1).empty())
	{
		int TeleOut = GameWorld()->m_Core.RandomOr0(Collision()->TeleOuts(evilz - 1).size());
		m_Pos = Collision()->TeleOuts(evilz - 1)[TeleOut];
		if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons)
		{
			m_Vel = vec2(0, 0);
		}
		return;
	}
	if(Collision()->IsCheckEvilTeleport(MapIndex))
	{
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if(!Collision()->TeleCheckOuts(k).empty())
			{
				int TeleOut = GameWorld()->m_Core.RandomOr0(Collision()->TeleCheckOuts(k).size());
				m_Pos = Collision()->TeleCheckOuts(k)[TeleOut];
				m_Vel = vec2(0, 0);

				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(GameServer()->m_pController->CanSpawn(0, &SpawnPos, m_Team))
		{
			m_Pos = SpawnPos;
			m_Vel = vec2(0, 0);
		}
		return;
	}
	if(Collision()->IsCheckTeleport(MapIndex))
	{
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if(!Collision()->TeleCheckOuts(k).empty())
			{
				int TeleOut = GameWorld()->m_Core.RandomOr0(Collision()->TeleCheckOuts(k).size());
				m_Pos = Collision()->TeleCheckOuts(k)[TeleOut];
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(GameServer()->m_pController->CanSpawn(0, &SpawnPos, m_Team))
		{
			m_Pos = SpawnPos;
		}
		return;
	}
}

void CPickupDrop::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		if(!pSnapPlayer)
			return;
	}

	CGameTeams Teams = GameServer()->m_pController->Teams();
	if(!Teams.SetMaskWithFlags(SnappingClient, m_Team, CGameTeams::IGNORE_SOLO))
		return;

	// Make the pickup blink when about to disappear
	if(m_Lifetime < Server()->TickSpeed() * 10 && (Server()->Tick() / (Server()->TickSpeed() / 4)) % 2 == 0)
		return;

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	bool SixUp = Server()->IsSixup(SnappingClient);
	int SubType = GameServer()->GetWeaponType(m_Type);

	GameServer()->SnapPickup(CSnapContext(SnapVer, SixUp, SnappingClient), GetId(), m_Pos, POWERUP_WEAPON, SubType, -1, PICKUPFLAG_NO_PREDICT);

	vec2 OffSet = vec2(0.0f, -32.0f);
	if(m_Type == WEAPON_HEARTGUN)
	{
		GameServer()->SnapPickup(CSnapContext(SnapVer, SixUp, SnappingClient), m_aIds[0], m_Pos + OffSet, POWERUP_HEALTH, 0, -1, PICKUPFLAG_NO_PREDICT);
	}
	else if(m_Type == WEAPON_LIGHTSABER)
	{
		GameServer()->SnapLaserObject(CSnapContext(SnapVer, SixUp, SnappingClient), m_aIds[0], m_Pos + OffSet, m_Pos + OffSet, Server()->Tick(), -1, LASERTYPE_GUN);
	}
	else if(m_Type == WEAPON_PORTALGUN)
	{
		GameServer()->SnapLaserObject(CSnapContext(SnapVer, SixUp, SnappingClient), m_aIds[0], m_Pos + OffSet, m_Pos + OffSet, Server()->Tick(), -1, LASERTYPE_GUN);
		vec2 Spin = vec2(cos(Server()->Tick() / 5.0f), sin(Server()->Tick() / 5.0f)) * 17.0f;
		Spin += OffSet;

		CNetObj_Projectile *pProj = Server()->SnapNewItem<CNetObj_Projectile>(m_aIds[1]);
		if(!pProj)
			return;

		pProj->m_X = (int)(m_Pos.x + Spin.x);
		pProj->m_Y = (int)(m_Pos.y + Spin.y);
		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
		pProj->m_StartTick = 0;
		pProj->m_Type = WEAPON_HAMMER;
	}
}

void CPickupDrop::SetVelocity(vec2 Vel)
{
	m_Vel = ClampVel(m_MoveRestrictions, Vel);
}

void CPickupDrop::ForceSetPos(vec2 NewPos)
{
	m_Pos = NewPos;
	m_PrevPos = m_Pos;
}