/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H

#include <game/server/entity.h>

class CProjectile : public CEntity
{
public:
	CProjectile(
		CGameWorld *pGameWorld,
		int Type,
		int Owner,
		vec2 Pos,
		vec2 Dir,
		int Span,
		bool Freeze,
		bool Explosive,
		int SoundImpact,
		vec2 InitDir,
		int Layer = 0,
		int Number = 0);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void SwapClients(int Client1, int Client2) override;

private:
	vec2 m_Direction;
	int m_LifeSpan;
	int m_Owner;
	int m_Type;
	//int m_Damage;
	int m_SoundImpact;
	int m_StartTick;
	bool m_Explosive;

	// DDRace

	int m_Bouncing;
	bool m_Freeze;
	int m_TuneZone;
	bool m_BelongsToPracticeTeam;
	int m_DDRaceTeam;
	bool m_IsSolo;
	vec2 m_InitDir;

	// for moving shotgun pellets
	int m_TargetSwitchCollisionCooldown;

public:
	void SetBouncing(int Value);
	bool FillExtraInfoLegacy(CNetObj_DDRaceProjectile *pProj);
	void FillExtraInfo(CNetObj_DDNetProjectile *pProj);

	bool CanCollide(int ClientId) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif
