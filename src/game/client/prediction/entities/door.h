/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_DOOR_H
#define GAME_CLIENT_PREDICTION_ENTITIES_DOOR_H

#include <game/client/prediction/entity.h>

class CLaserData;

class CDoor : public CEntity
{
	vec2 m_To;
	vec2 m_Direction;
	int m_Length;
	bool m_Active;

public:
	CDoor(CGameWorld *pGameWorld, int Id, const CLaserData *pData);
	void ResetCollision();
	bool Match(const CDoor *pDoor) const;
	void Read(const CLaserData *pData);

	void Destroy() override;
};

#endif // GAME_CLIENT_PREDICTION_ENTITIES_DOOR_H
