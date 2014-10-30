/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_AUTOFIRE_H
#define GAME_SERVER_ENTITIES_AUTOFIRE_H

class CAutofire : public CEntity
{
public:
	CAutofire
	(
		CGameWorld *pGameWorld,
		int Weapon,
		vec2 Position,
		vec2 Direction,
		int Delay
	);

	virtual void Tick();

private:
	int m_Weapon;
	vec2 m_Position;
	vec2 m_Direction;
	int m_Delay;

	int m_Time;
	int m_TuneZone;
};

#endif
