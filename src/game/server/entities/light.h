/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_LIGHT_H
#define GAME_SERVER_ENTITIES_LIGHT_H

#include <game/server/entity.h>

class CLight : public CEntity
{
	float m_Rotation = 0;
	vec2 m_To;
	vec2 m_Core;

	int m_EvalTick = 0;

	int m_Tick = 0;

	bool HitCharacter();
	void Move();
	void Step();

public:
	int m_CurveLength = 0;
	int m_LengthL = 0;
	float m_AngularSpeed = 0;
	int m_Speed = 0;
	int m_Length = 0;

	CLight(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
		int Layer = 0, int Number = 0);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_ENTITIES_LIGHT_H
