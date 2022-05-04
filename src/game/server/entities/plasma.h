/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_PLASMA_H
#define GAME_SERVER_ENTITIES_PLASMA_H

#include <game/server/entity.h>

class CPlasma : public CEntity
{
	vec2 m_Core;
	int m_Freeze;
	bool m_Explosive;
	int m_ForClientID;
	int m_EvalTick;
	int m_LifeTime;

	bool HitCharacter(CCharacter *pTarget);
	void Move();

public:
	CPlasma(CGameWorld *pGameWorld, vec2 Pos, vec2 Dir, bool Freeze,
		bool Explosive, int ForClientId);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_ENTITIES_PLASMA_H
