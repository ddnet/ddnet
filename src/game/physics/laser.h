/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_PHYSICS_LASER_H
#define GAME_PHYSICS_LASER_H

#include <base/math.h>
#include <base/vmath.h>

#include <game/mapitems.h>

// Shared laser physics, used by the server entity and the client prediction
// entity. The surrounding bounce flow differs per side, the server also
// handles weapon teleporters, the telegun and the interact state, so only
// the parts that must stay bit identical live here.
template<typename TLaser, typename TCharacter>
class CLaserPhysics
{
public:
	// Deflect the laser at the obstacle it hit and pay the energy cost.
	// Res and Coltile come from the preceding IntersectLineTeleWeapon call.
	static void Bounce(TLaser *pThis, int Res, vec2 Coltile, vec2 To)
	{
		pThis->m_From = pThis->m_Pos;
		pThis->m_Pos = To;

		vec2 TempPos = pThis->m_Pos;
		vec2 TempDir = pThis->m_Dir * 4.0f;

		int f = 0;
		if(Res == -1)
		{
			f = pThis->Collision()->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
			pThis->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), TILE_SOLID);
		}
		pThis->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, nullptr);
		if(Res == -1)
		{
			pThis->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), f);
		}
		pThis->m_Pos = TempPos;
		pThis->m_Dir = normalize(TempDir);

		const float Distance = distance(pThis->m_From, pThis->m_Pos);
		// Prevent infinite bounces
		if(Distance == 0.0f && pThis->m_ZeroEnergyBounceInLastTick)
		{
			pThis->m_Energy = -1;
		}
		else
		{
			pThis->m_Energy -= Distance + pThis->GetTuning(pThis->m_TuneZone)->m_LaserBounceCost;
		}
		pThis->m_ZeroEnergyBounceInLastTick = Distance == 0.0f;
	}

	// Knock the hit character back, reproducing the 'shotgun bug' ddnet#5258
	// for stacked tees.
	static void ShotgunKnockback(TLaser *pThis, TCharacter *pHit, TCharacter *pOwnerChar)
	{
		const vec2 StackedLaserShotgunBugSpeed = vec2(-2147483648.0f, -2147483648.0f);
		float Strength = pThis->GetTuning(pThis->m_TuneZone)->m_ShotgunStrength;
		const vec2 &HitPos = pHit->Core()->m_Pos;
		if(!pThis->OldLaser())
		{
			if(pThis->m_PrevPos != HitPos)
			{
				pHit->AddVelocity(normalize(pThis->m_PrevPos - HitPos) * Strength);
			}
			else
			{
				pHit->SetRawVelocity(StackedLaserShotgunBugSpeed);
			}
		}
		else if(pThis->OldLaser() && pOwnerChar)
		{
			if(pOwnerChar->Core()->m_Pos != HitPos)
			{
				pHit->AddVelocity(normalize(pOwnerChar->Core()->m_Pos - HitPos) * Strength);
			}
			else
			{
				pHit->SetRawVelocity(StackedLaserShotgunBugSpeed);
			}
		}
		else
		{
			// Re-apply move restrictions as a part of 'shotgun bug' reproduction
			pHit->ApplyMoveRestrictions();
		}
	}
};

#endif // GAME_PHYSICS_LASER_H
