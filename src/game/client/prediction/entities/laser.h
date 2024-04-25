/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_LASER_H
#define GAME_CLIENT_PREDICTION_ENTITIES_LASER_H

#include <game/client/prediction/entity.h>

class CLaserData;

class CLaser : public CEntity
{
	friend class CGameWorld;

public:
	CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Type);

	void Tick() override;

	const vec2 &GetFrom() { return m_From; }
	const int &GetOwner() { return m_Owner; }
	const int &GetEvalTick() { return m_EvalTick; }
	CLaser(CGameWorld *pGameWorld, int Id, CLaserData *pLaser);
	bool Match(CLaser *pLaser);
	CLaserData GetData() const;

protected:
	bool HitCharacter(vec2 From, vec2 To);
	void DoBounce();

private:
	vec2 m_From;
	vec2 m_Dir;
	float m_Energy;
	int m_Bounces;
	int m_EvalTick;
	int m_Owner;
	bool m_ZeroEnergyBounceInLastTick;

	// DDRace

	vec2 m_PrevPos;
	int m_Type;
	int m_TuneZone;
};

#endif
