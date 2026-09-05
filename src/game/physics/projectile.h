/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_PHYSICS_PROJECTILE_H
#define GAME_PHYSICS_PROJECTILE_H

#include <base/math.h>
#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/mapitems.h>

#include <type_traits>

// Shared projectile physics, used by the server entity and the client
// prediction entity. The tick flow differs per side, the server also handles
// damage, the telegun and weapon teleporters, so only the parts that must
// stay bit identical live here.
template<typename TProjectile, typename TCharacter>
class CProjectilePhysics
{
public:
	// Reflect a bouncing projectile off the wall it just hit.
	static void Bounce(TProjectile *pThis, vec2 NewPos)
	{
		pThis->m_StartTick = pThis->GameWorld()->GameTick();
		pThis->m_Pos = NewPos + (-(pThis->m_Direction * 4));
		if(pThis->m_Bouncing == 1)
			pThis->m_Direction.x = -pThis->m_Direction.x;
		else if(pThis->m_Bouncing == 2)
			pThis->m_Direction.y = -pThis->m_Direction.y;
		if(absolute(pThis->m_Direction.x) < 1e-6f)
			pThis->m_Direction.x = 0;
		if(absolute(pThis->m_Direction.y) < 1e-6f)
			pThis->m_Direction.y = 0;
		pThis->m_Pos += pThis->m_Direction;
	}

	// Freeze all characters within reach of a freezing projectile impact.
	static void FreezeCharactersNear(TProjectile *pThis, vec2 Pos)
	{
		using TWorld = std::remove_pointer_t<decltype(pThis->GameWorld())>;
		using TEntity = std::remove_pointer_t<decltype(pThis->GameWorld()->FindFirst(0))>;

		TEntity *apEnts[MAX_CLIENTS];
		int Num = pThis->GameWorld()->FindEntities(Pos, 1.0f, apEnts, MAX_CLIENTS, TWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; ++i)
		{
			auto *pChr = static_cast<TCharacter *>(apEnts[i]);
			// The switcher bounds check only matters on the client, where entity data
			// from the snapshot is not validated against the switch layer of the map
			if(pChr && (pThis->m_Layer != LAYER_SWITCH || (pThis->m_Layer == LAYER_SWITCH && pThis->m_Number > 0 && pThis->m_Number < (int)pThis->Switchers().size() && pThis->Switchers()[pThis->m_Number].m_aStatus[pChr->Team()])))
				pChr->Freeze();
		}
	}
};

#endif // GAME_PHYSICS_PROJECTILE_H
