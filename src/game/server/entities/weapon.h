#ifndef GAME_SERVER_ENTITIES_WEAPON_H
#define GAME_SERVER_ENTITIES_WEAPON_H

#include <game/server/entity.h>

class CWeapon : public CEntity
{
public:
	CWeapon(CGameWorld *pGameWorld, int Owner, int Weapon, int Direction, bool Jetpack);

	void Reset(bool Erase, bool Picked);
	virtual void Reset() { Reset(true, false); };
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	static int const ms_PhysSize = 14;

private:
	int IsCharacterNear();
	void IsShieldNear();
	void Pickup();
	bool IsGrounded(bool SetVel = false);
	void HandleDropped();

	vec2 m_Vel;

	int64_t m_TeamMask;
	CCharacter* m_pOwner;
	int m_Owner;

	int m_Weapon;
	int m_Lifetime;
	int m_PickupDelay;
	bool m_Jetpack;
	vec2 m_PrevPos;

	int m_ID2;

	int m_TuneZone;
	int m_TeleCheckpoint;

	void HandleTiles(int Index);
	int m_TileIndex;
	int m_TileFlags;
	int m_TileFIndex;
	int m_TileFFlags;
	int m_TileSIndex;
	int m_TileSFlags;
	int m_TileIndexL;
	int m_TileFlagsL;
	int m_TileFIndexL;
	int m_TileFFlagsL;
	int m_TileSIndexL;
	int m_TileSFlagsL;
	int m_TileIndexR;
	int m_TileFlagsR;
	int m_TileFIndexR;
	int m_TileFFlagsR;
	int m_TileSIndexR;
	int m_TileSFlagsR;
	int m_TileIndexT;
	int m_TileFlagsT;
	int m_TileFIndexT;
	int m_TileFFlagsT;
	int m_TileSIndexT;
	int m_TileSFlagsT;
	int m_TileIndexB;
	int m_TileFlagsB;
	int m_TileFIndexB;
	int m_TileFFlagsB;
	int m_TileSIndexB;
	int m_TileSFlagsB;
};

#endif
