// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_COSMETICS_PICKUPPET_H
#define GAME_SERVER_FOXNET_COSMETICS_PICKUPPET_H

#include <game/server/gameworld.h>
#include <game/server/entity.h>

#include <base/vmath.h>

class CPickupPet : public CEntity
{
	enum PetMode
	{
		PET_MODE_AFK,
		PET_MODE_FOLLOW,
		PET_MODE_STATIC,
	};

	vec2 m_aPos;
	
	float m_aSpeed;
	int m_Owner;

	int m_CurType;
	int64_t m_SwitchDelay;

	int m_PetMode;

public:
	CPickupPet(CGameWorld *pGameWorld, int Owner, vec2 Pos);
	
	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;

	void SetPetMode(int Mode) { m_PetMode = Mode; }
	int GetPetMode() { return m_PetMode; }

	void PlayerAfkMode(CCharacter *pOwner);
	void StaticMode(CCharacter *pOwner);
	void FollowMode(CCharacter *pOwner);
};

#endif // GAME_SERVER_FOXNET_COSMETICS_PICKUPPET_H
