#include "pickupdrop.h"
#include <game/server/entities/character.h>
#include <game/server/player.h>

#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/teams.h>

#include <generated/protocol.h>
#include <generated/server_data.h>

#include <base/vmath.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <game/server/entities/pickup.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>
#include <vector>

CPickupDrop::CPickupDrop(CGameWorld *pGameWorld, int LastOwner, vec2 Pos, int Team, int TeleCheckpoint, vec2 Dir, int Lifetime, int Type) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUPDROP, Pos)
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
	GameWorld()->RemoveEntity(this);

	CClientMask TeamMask = CClientMask().set();
	if(!PickedUp)
		GameServer()->CreateDeath(m_Pos, m_LastOwner, TeamMask);
}

bool CPickupDrop::IsSwitchActiveCb(int Number, void *pUser)
{
	CPickupDrop *pThis = (CPickupDrop *)pUser;
	auto &aSwitchers = pThis->Switchers();
	return !aSwitchers.empty() && pThis->m_Team != TEAM_SUPER && aSwitchers[Number].m_aStatus[pThis->m_Team];
}

void CPickupDrop::Tick()
{
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

	// quads
	for(const auto *pQuadLayer : Collision()->QuadLayers())
	{
		for(int QuadIndex = 0; QuadIndex < pQuadLayer->m_NumQuads; QuadIndex++)
		{
			HandleQuads(pQuadLayer, QuadIndex);
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

	CheckArmor();
	CollectItem();
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

void CPickupDrop::CheckArmor()
{
	CPickup *apEnts[9];
	int Num = GameWorld()->FindEntities(m_Pos, 34, (CEntity **)apEnts, 9, CGameWorld::ENTTYPE_PICKUP);

	for(int i = 0; i < Num; i++)
	{
		switch(apEnts[i]->Type())
		{
		case POWERUP_ARMOR:
		{
			if(m_Type > 0 && m_Type < NUM_WEAPONS && apEnts[i]->GetOwnerId() < 0)
			{
				CClientMask TeamMask = CClientMask().set();
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, TeamMask);
				Reset();
			}
		}
		break;

		case POWERUP_ARMOR_SHOTGUN:
		{
			if(m_Type == WEAPON_SHOTGUN && apEnts[i]->GetOwnerId() < 0)
			{
				CClientMask TeamMask = CClientMask().set();
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, TeamMask);
				Reset();
			}
		}
		break;

		case POWERUP_ARMOR_GRENADE:
		{
			if(m_Type == WEAPON_GRENADE && apEnts[i]->GetOwnerId() < 0)
			{
				CClientMask TeamMask = CClientMask().set();
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, TeamMask);
				Reset();
			}
		}
		break;

		case POWERUP_ARMOR_NINJA:
		{
			if(m_Type == WEAPON_NINJA && apEnts[i]->GetOwnerId() < 0)
			{
				CClientMask TeamMask = CClientMask().set();
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, TeamMask);
				Reset();
			}
		}
		break;

		case POWERUP_ARMOR_LASER:
		{
			if(m_Type == WEAPON_LASER && apEnts[i]->GetOwnerId() < 0)
			{
				CClientMask TeamMask = CClientMask().set();
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, TeamMask);
				Reset();
			}
		}
		break;

		default:
			break;
		}
	}
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

void CPickupDrop::CollectItem()
{
	if(m_PickupDelay > 0)
	{
		m_PickupDelay--;
		return;
	}

	float Radius = 32.0f;
	CCharacter *pClosest = GameServer()->m_World.ClosestCharacter(m_Pos, Radius, this);
	if(!pClosest)
		return;
	if(m_Team != TEAM_SUPER && pClosest->Team() != TEAM_SUPER && m_Team != pClosest->Team())
		return;
	if(pClosest->Core()->m_aWeapons[m_Type].m_Got)
		return;

	pClosest->GiveWeapon(m_Type);
	pClosest->SetActiveWeapon(m_Type);
	CClientMask TeamMask = CClientMask().set();
	GameServer()->CreateSound(pClosest->m_Pos, SOUND_PICKUP_HEALTH, TeamMask);

	if(pClosest->GetPlayer())
		GameServer()->SendWeaponPickup(pClosest->GetPlayer()->GetCid(), m_Type);

	Reset(true);
}

void CPickupDrop::HandleQuads(const CMapItemLayerQuads *pQuadLayer, int QuadIndex)
{
	if(!pQuadLayer)
		return;

	char QuadName[30] = "";
	IntsToStr(pQuadLayer->m_aName, std::size(pQuadLayer->m_aName), QuadName, std::size(QuadName));

	bool IsFreeze = !str_comp("QFr", QuadName);
	bool IsDeath = !str_comp("QDeath", QuadName);
	bool IsStopa = !str_comp("QStopa", QuadName);
	bool IsCfrm = !str_comp("QCfrm", QuadName);

	vec2 TopL, TopR, BottomL, BottomR;
	int FoundNum = Collision()->GetQuadCorners(QuadIndex, pQuadLayer, 0.00f, &TopL, &TopR, &BottomL, &BottomR);
	if(FoundNum < 0 || FoundNum >= pQuadLayer->m_NumQuads)
		return;

	float Radius = 0.0f;
	if(IsDeath)
		Radius = 8.0f;
	if(IsStopa)
		Radius = CCharacterCore::PhysicalSize() / 2.0f;

	bool Inside = Collision()->InsideQuad(m_Pos, Radius, TopL, TopR, BottomL, BottomR);
	if(!Inside)
		return;

	if(IsFreeze)
	{
		if(g_Config.m_SvDropsInFreezeFloat)
			m_InsideFreeze = true;
	}
	else if(IsDeath)
	{
		Reset();
	}
	else if(IsStopa)
	{
		HandleQuadStopa(pQuadLayer, QuadIndex);
	}
	else if(IsCfrm)
	{
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
		vec2 SpawnPos;
		if(GameServer()->m_pController->CanSpawn(0, &SpawnPos, m_Team))
		{
			m_Pos = SpawnPos;
			m_Vel = vec2(0, 0);
		}
	}
}

void CPickupDrop::HandleQuadStopa(const CMapItemLayerQuads *pQuadLayer, int QuadIndex)
{
	if(!pQuadLayer)
		return;

	vec2 TL, TR, BL, BR;
	int Found = Collision()->GetQuadCorners(QuadIndex, pQuadLayer, 0.0f, &TL, &TR, &BL, &BR);
	if(Found < 0 || Found >= pQuadLayer->m_NumQuads)
		return;

	bool Inside = Collision()->InsideQuad(m_Pos, CCharacterCore::PhysicalSize() / 1.5f, TL, TR, BL, BR);
	if(!Inside)
		return;

	constexpr float R = CCharacterCore::PhysicalSize() * 0.5f;
	const vec2 P = m_Pos;

	const vec2 aA[4] = {TL, TR, BR, BL};
	const vec2 aB[4] = {TR, BR, BL, TL};

	float MinPenetration = std::numeric_limits<float>::infinity();
	vec2 BestInwardNormal = vec2(0.0f, 0.0f);
	vec2 BestEdgeVec = vec2(0.0f, 0.0f);

	for(int i = 0; i < 4; ++i)
	{
		vec2 E = aB[i] - aA[i];
		float Elen2 = dot(E, E);
		if(Elen2 <= 1e-6f)
			continue;

		vec2 N_in = normalize(vec2(-E.y, E.x));
		float d = dot(P - aA[i], N_in);
		float penetration = d + R;

		if(penetration < MinPenetration)
		{
			MinPenetration = penetration;
			BestInwardNormal = N_in;
			BestEdgeVec = E;
		}
	}

	if(MinPenetration == std::numeric_limits<float>::infinity())
		return;

	if(MinPenetration > 0.0f)
	{
		const float Epsilon = 0.0f;
		vec2 MTV = -BestInwardNormal * (MinPenetration + Epsilon);

		auto CanPlace = [&](const vec2 &Pos) {
			return !Collision()->TestBox(Pos, CCharacterCore::PhysicalSizeVec2());
		};

		auto MoveAxis = [&](vec2 &Pos, const vec2 &Delta) {
			if(Delta.x == 0.0f && Delta.y == 0.0f)
				return vec2(0.f, 0.f);

			vec2 Target = Pos + Delta;
			if(CanPlace(Target))
			{
				Pos = Target;
				return Delta;
			}

			float lo = 0.0f;
			float hi = 1.0f;
			for(int i = 0; i < 10; ++i)
			{
				float mid = (lo + hi) * 0.5f;
				vec2 MidPos = Pos + Delta * mid;
				if(CanPlace(MidPos))
					lo = mid;
				else
					hi = mid;
			}
			if(lo > 0.0f)
			{
				vec2 Applied = Delta * lo;
				Pos += Applied;
				return Applied;
			}
			return vec2(0.0f, 0.0f);
		};

		vec2 NewPos = m_Pos;

		vec2 AppliedX = MoveAxis(NewPos, vec2(MTV.x, 0.0f));
		vec2 AppliedY = MoveAxis(NewPos, vec2(0.0f, MTV.y));

		m_Pos = NewPos;

		float vIn = dot(m_Vel, BestInwardNormal);
		if(vIn > 0.0f)
			m_Vel -= BestInwardNormal * vIn;

		if(AppliedX.x == 0.0f && MTV.x != 0.0f)
			m_Vel.x = 0.0f;
		if(AppliedY.y == 0.0f && MTV.y != 0.0f)
			m_Vel.y = 0.0f;
	}
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

	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pSnapPlayer)
		return;

	if(CCharacter *pSnapChar = pSnapPlayer->GetCharacter())
	{
		if(m_Team == TEAM_SUPER) {}
		else if(pSnapPlayer->IsPaused())
		{
			if(pSnapChar->Team() != m_Team && pSnapPlayer->m_SpecTeam)
				return;
		}
		else
		{
			if(pSnapChar->Team() != TEAM_SUPER && pSnapChar->Team() != m_Team)
				return;
		}
	}

	// Make the pickup blink when about to disappear
	if(m_Lifetime < Server()->TickSpeed() * 10 && (Server()->Tick() / (Server()->TickSpeed() / 4)) % 2 == 0)
		return;

	int SnappingClientVersion = pSnapPlayer->GetClientVersion();
	bool SixUp = Server()->IsSixup(SnappingClient);
	int SubType = GameServer()->GetWeaponType(m_Type);

	GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), GetId(), m_Pos, POWERUP_WEAPON, SubType, -1, PICKUPFLAG_NO_PREDICT);

	vec2 OffSet = vec2(0.0f, -32.0f);
	if(m_Type == WEAPON_HEARTGUN)
	{
		GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), m_aIds[0], m_Pos + OffSet, POWERUP_HEALTH, 0, -1, PICKUPFLAG_NO_PREDICT);
	}
	else if(m_Type == WEAPON_LIGHTSABER)
	{
		GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), m_aIds[0], m_Pos + OffSet, m_Pos + OffSet, Server()->Tick(), -1, LASERTYPE_GUN);
	}
	else if(m_Type == WEAPON_PORTALGUN)
	{
		GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), m_aIds[0], m_Pos + OffSet, m_Pos + OffSet, Server()->Tick(), -1, LASERTYPE_GUN);
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