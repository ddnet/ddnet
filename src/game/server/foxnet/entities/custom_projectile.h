// made by fokkonaut
#ifndef GAME_SERVER_FOXNET_ENTITIES_CUSTOM_PROJECTILE_H
#define GAME_SERVER_FOXNET_ENTITIES_CUSTOM_PROJECTILE_H

#include <game/server/entity.h>
#include <game/server/entities/character.h>

#include <engine/shared/protocol.h>

enum
{
	NOT_COLLIDED = 0,
	COLLIDED_ONCE,
	COLLIDED_TWICE
};

class CCustomProjectile : public CEntity
{
public:
	CCustomProjectile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir,
		bool Explosive, bool Freeze, bool Unfreeze, int Type, bool IsNormalWeapon = false, float Lifetime = 6.0f, float Accel = 1.0f, float Speed = 10.0f);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;

private:
	vec2 m_Core;
	vec2 m_PrevPos;
	vec2 m_Direction;

	int m_EvalTick;
	int m_LifeTime;

	CClientMask m_TeamMask;
	CCharacter *m_pOwner;
	int m_Owner;

	int m_Freeze;
	int m_Unfreeze;
	bool m_Explosive;

	int m_TuneZone;
	bool m_Weapon;

	int m_Type;

	float m_Accel;

	int m_CollisionState;

	void HitCharacter();
	void Move();

	void FillInfo(CNetObj_Projectile *pProj);
	bool FillExtraInfoLegacy(CNetObj_DDRaceProjectile *pProj);
	void FillExtraInfo(CNetObj_DDNetProjectile *pProj);
};

#endif
