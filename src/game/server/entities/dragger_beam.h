/* See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DRAGGER_BEAM_H
#define GAME_SERVER_ENTITIES_DRAGGER_BEAM_H

#include "dragger.h"
#include <game/server/entity.h>

class CDraggerBeam : public CEntity
{
	bool m_IgnoreWalls;
	float m_Strength;
	int m_ForClientID;
	int m_EvalTick;
	bool m_Active;
	CDragger *m_pDragger;

public:
	CDraggerBeam(CGameWorld *pGameWorld, CDragger *pDragger, vec2 Pos, float Strength, bool IgnoreWalls, int ForClientID);

	void SetPos(vec2 Pos);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_DRAGGER_BEAM_H
