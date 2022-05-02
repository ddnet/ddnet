/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DRAGGER_H
#define GAME_SERVER_ENTITIES_DRAGGER_H

#include <game/server/entity.h>
class CDraggerBeam;

class CDragger : public CEntity
{
	// m_Core is the direction vector by which a dragger is shifted at each movement tick (every 150ms)
	vec2 m_Core;
	float m_Strength;
	int m_EvalTick;
	void LookForPlayersToDrag();
	bool m_IgnoreWalls;

	CDraggerBeam *m_apDraggerBeam[MAX_CLIENTS];

public:
	CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool IgnoreWalls, int Layer = 0, int Number = 0);

	void RemoveDraggerBeam(int ClientID);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_ENTITIES_DRAGGER_H
