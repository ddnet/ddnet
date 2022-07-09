/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GAME_SERVER_ENTITIES_GUN_H
#define GAME_SERVER_ENTITIES_GUN_H

#include <game/server/entity.h>

/**
 * Turrets (also referred to as Gun) fire plasma bullets at the nearest player
 *
 * A turret fires plasma bullets with a certain firing rate (sv_plasma_per_sec) at the closest player of a team for whom
 * the following criteria are met:
 * - The player is within the turret range (sv_plasma_range)
 * - The player is not a super player
 * - The turret is activated
 * - The initial trajectory of the plasma bullet to be generated would not be stopped by any solid block
 * With the exception of solo players, for whom plasma bullets will always be fired, regardless of the rest of the team,
 * if the above criteria are met. Solo players do not affect the generation of plasma bullets for the rest of the team.
 * The shooting rate of sv_plasma_per_sec is independent for each team and solo player and starts with the first tick
 * when a target player is selected.
 */
class CGun : public CEntity
{
	vec2 m_Core;
	bool m_Freeze;
	bool m_Explosive;
	int m_EvalTick;
	int m_aLastFireTeam[MAX_CLIENTS];
	int m_aLastFireSolo[MAX_CLIENTS];

	void Fire();

public:
	CGun(CGameWorld *pGameWorld, vec2 Pos, bool Freeze, bool Explosive, int Layer = 0, int Number = 0);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_ENTITIES_GUN_H
