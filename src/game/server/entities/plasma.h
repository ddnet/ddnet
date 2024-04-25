/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_PLASMA_H
#define GAME_SERVER_ENTITIES_PLASMA_H

#include <game/server/entity.h>

/**
 * Plasma Bullets are projectiles fired from turrets at a specific target
 * 
 * When hitting a tee, plasma bullets can either freeze or unfreeze the player
 * Also, plasma projectiles can explode on impact. However, the player affected by the explosion is not necessarily the
 * one the plasma collided with, but if the affected player is not a solo player, then the team-mate with the lowest
 * ClientId within the explosion range. Furthermore, the affected player does not feel the explosion at the point of
 * impact but at the last position of the plasma bullet. The same applies if a plasma bullet explodes due to a collision
 * with a laser stopper or a solid block
 * Plasma bullets move every tick in the assigned direction and then accelerate by the factor PLASMA_ACCEL
 * Plasma bullets can explode twice if they would hit both a player and an obstacle in the next movement step
 * Plasma bullets will stop existing as soon as:
 * - The player they were created for do no longer exist
 * - They have had a collision with a player, a solid block or a laser stopper
 * - Their life time of 1.5 seconds has expired
 */
class CPlasma : public CEntity
{
	vec2 m_Core;
	int m_Freeze;
	bool m_Explosive;
	int m_ForClientId;
	int m_EvalTick;
	int m_LifeTime;

	void Move();
	bool HitCharacter(CCharacter *pTarget);
	bool HitObstacle(CCharacter *pTarget);

public:
	CPlasma(CGameWorld *pGameWorld, vec2 Pos, vec2 Dir, bool Freeze,
		bool Explosive, int ForClientId);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	void SwapClients(int Client1, int Client2) override;
};

#endif // GAME_SERVER_ENTITIES_PLASMA_H
