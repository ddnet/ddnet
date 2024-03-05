/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_DRAGGER_H
#define GAME_CLIENT_PREDICTION_ENTITIES_DRAGGER_H

#include <game/client/prediction/entity.h>

class CLaserData;

class CDragger : public CEntity
{
	vec2 m_Core;
	float m_Strength;
	bool m_IgnoreWalls;
	int m_TargetId;

	void LookForPlayersToDrag();
	void DraggerBeamTick();
	void DraggerBeamReset();

public:
	CDragger(CGameWorld *pGameWorld, int Id, const CLaserData *pData);
	bool Match(CDragger *pDragger);
	void Read(const CLaserData *pData);
	float GetStrength() { return m_Strength; }

	void Tick() override;
};

#endif // GAME_CLIENT_PREDICTION_ENTITIES_DRAGGER_H
