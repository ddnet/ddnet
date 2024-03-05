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

	virtual int GetOwnerId() const override { return m_Owner; }

protected:
	bool HitCharacter(vec2 From, vec2 To);
	void DoBounce();

private:
	vec2 m_From;
	vec2 m_Dir;
	vec2 m_TelePos;
	bool m_WasTele;
	float m_Energy;
	int m_Bounces;
	int m_EvalTick;
	int m_Owner;
	CClientMask m_TeamMask;
	bool m_ZeroEnergyBounceInLastTick;

	// DDRace

	vec2 m_PrevPos;
	int m_Type;
	int m_TuneZone;
	bool m_TeleportCancelled;
	bool m_IsBlueTeleport;
	bool m_BelongsToPracticeTeam;
};

#endif
