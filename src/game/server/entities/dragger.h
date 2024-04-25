/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DRAGGER_H
#define GAME_SERVER_ENTITIES_DRAGGER_H

#include <game/server/entity.h>
class CDraggerBeam;

/**
 * Draggers generate dragger beams which pull players towards their center similar to a tractor beam
 *
 * A dragger will only generate one dragger beam per team for the closest player for whom the following criteria are met:
 * - The player is within the dragger range (sv_dragger_range).
 * - The player is not a super player
 * - The dragger is activated
 * - The dragger beam to be generated is not blocked by laser stoppers (or solid blocks if IgnoreWalls is set to false)
 * With the exception of solo players, for whom a dragger beam is always generated, regardless of the rest of the team,
 * if the above criteria are met. Solo players have no influence on the generation of the dragger beam for the rest
 * of the team.
 * A created dragger beam remains for the selected player until one of the criteria is no longer fulfilled. Only then
 * can a new dragger beam be created for that team, which may drag another team partner.
 */
class CDragger : public CEntity
{
	// m_Core is the direction vector by which a dragger is shifted at each movement tick (every 150ms)
	vec2 m_Core;
	float m_Strength;
	bool m_IgnoreWalls;
	int m_EvalTick;

	int m_aTargetIdInTeam[MAX_CLIENTS];
	CDraggerBeam *m_apDraggerBeam[MAX_CLIENTS];

	void LookForPlayersToDrag();

public:
	CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool IgnoreWalls, int Layer = 0, int Number = 0);

	void RemoveDraggerBeam(int ClientId);
	bool WillDraggerBeamUseDraggerId(int TargetClientId, int SnappingClientId);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	void SwapClients(int Client1, int Client2) override;
};

#endif // GAME_SERVER_ENTITIES_DRAGGER_H
