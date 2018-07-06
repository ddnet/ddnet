/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DRAGGER_H
#define GAME_SERVER_ENTITIES_DRAGGER_H

#include <game/server/entity.h>
class CCharacter;

class CDragger: public CEntity
{
	vec2 m_Core;
	float m_Strength;
	int m_EvalTick;
	void Move();
	void Drag();
	CCharacter * m_Target;
	bool m_NW;
	int m_CatchedTeam;

	CCharacter * m_SoloEnts[MAX_CLIENTS];
	int m_SoloIDs[MAX_CLIENTS];
public:

	CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool NW,
			int CatchedTeam, int Layer = 0, int Number = 0);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int snapping_client);
};

class CDraggerTeam
{
	CDragger * m_Draggers[MAX_CLIENTS];

public:

	CDraggerTeam(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool NW =
			false, int Layer = 0, int Number = 0);
	//~CDraggerTeam();
};

#endif // GAME_SERVER_ENTITIES_DRAGGER_H
