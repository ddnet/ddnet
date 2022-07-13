/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_H
#define GAME_SERVER_ENTITIES_LASER_H

#include <game/server/entity.h>

class CLaser : public CEntity
{
public:
	CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Type);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void TickPaused() override;
	virtual void Snap(int SnappingClient) override;
	virtual void SwapClients(int Client1, int Client2) override;

protected:
	bool HitCharacter(vec2 From, vec2 To);
	void DoBounce();

private:
	vec2 m_From;
	vec2 m_Dir;
	vec2 m_TelePos;
	bool m_WasTele = false;
	float m_Energy = 0;
	int m_Bounces = 0;
	int m_EvalTick = 0;
	int m_Owner = 0;
	int m_TeamMask = 0;
	bool m_ZeroEnergyBounceInLastTick = false;

	// DDRace

	vec2 m_PrevPos;
	int m_Type = 0;
	int m_TuneZone = 0;
	bool m_TeleportCancelled = false;
	bool m_IsBlueTeleport = false;
	bool m_BelongsToPracticeTeam = false;
};

#endif
