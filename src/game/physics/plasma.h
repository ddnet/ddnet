/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_PHYSICS_PLASMA_H
#define GAME_PHYSICS_PLASMA_H

#include <base/vmath.h>

inline constexpr float PLASMA_ACCEL = 1.1f;

// Shared plasma bullet physics, used by the server entity and the client
// prediction entity. TPlasma provides the state and the side specific
// Explode and Reset behavior, TCharacter the character interaction.
template<typename TPlasma, typename TCharacter>
class CPlasmaPhysics
{
public:
	static void Tick(TPlasma *pThis)
	{
		// A plasma bullet has only a limited lifetime
		if(pThis->m_LifeTime == 0)
		{
			pThis->Reset();
			return;
		}
		TCharacter *pTarget = pThis->GameWorld()->GetCharacterById(pThis->m_ForClientId);
		// Without a target, a plasma bullet has no reason to live
		if(!pTarget)
		{
			pThis->Reset();
			return;
		}
		pThis->m_LifeTime--;
		Move(pThis);
		HitCharacter(pThis, pTarget);
		// Plasma bullets may explode twice if they would hit both a player and an obstacle in the next move step
		HitObstacle(pThis, pTarget);
	}

private:
	static void Move(TPlasma *pThis)
	{
		pThis->m_Pos += pThis->m_Core;
		pThis->m_Core *= PLASMA_ACCEL;
	}

	static bool HitCharacter(TPlasma *pThis, TCharacter *pTarget)
	{
		vec2 IntersectPos;
		TCharacter *pHitPlayer = pThis->GameWorld()->IntersectCharacter(
			pThis->m_Pos, pThis->m_Pos + pThis->m_Core, 0.0f, IntersectPos, nullptr, pThis->m_ForClientId);
		if(!pHitPlayer)
		{
			return false;
		}

		// Super player should not be able to stop the plasma bullets
		if(pHitPlayer->Team() == pHitPlayer->TeamsCore()->TeamSuper())
		{
			return false;
		}

		pThis->m_Freeze ? pHitPlayer->Freeze() : pHitPlayer->Unfreeze();
		if(pThis->m_Explosive)
		{
			// Plasma Turrets are very precise weapons only one tee gets speed from it,
			// other tees near the explosion remain unaffected
			pThis->Explode(pTarget);
		}
		pThis->Reset();
		return true;
	}

	static bool HitObstacle(TPlasma *pThis, TCharacter *pTarget)
	{
		// Check if the plasma bullet is stopped by a solid block or a laser stopper
		int HasIntersection = pThis->Collision()->IntersectNoLaser(pThis->m_Pos, pThis->m_Pos + pThis->m_Core, nullptr, nullptr);
		if(HasIntersection)
		{
			if(pThis->m_Explosive)
			{
				// Even in the case of an explosion due to a collision with obstacles, only one player is affected
				pThis->Explode(pTarget);
			}
			pThis->Reset();
			return true;
		}
		return false;
	}
};

#endif // GAME_PHYSICS_PLASMA_H
