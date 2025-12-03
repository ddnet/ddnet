/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TAS_TAS_STATE_H
#define GAME_SERVER_TAS_TAS_STATE_H

#include <base/vmath.h>
#include <engine/shared/protocol.h>
#include <game/server/save.h>
#include <game/generated/protocol.h>
#include <game/teamscore.h>

#include <memory>
#include <vector>

class CGameContext;
class CCharacter;
class CProjectile;
class CLaser;

// Input state for a single tick
struct STasInput
{
	int m_Tick;
	int m_ClientId;
	int m_Direction;
	int m_TargetX;
	int m_TargetY;
	int m_Jump;
	int m_Fire;
	int m_Hook;
	int m_Weapon;

	bool operator<(const STasInput &Other) const
	{
		if(m_Tick != Other.m_Tick)
			return m_Tick < Other.m_Tick;
		return m_ClientId < Other.m_ClientId;
	}
};

// Complete snapshot of game state at a single tick
class CTasSnapshot
{
public:
	CTasSnapshot();
	~CTasSnapshot() = default;

	int m_Tick;
	bool m_IsKeyframe;
	uint32_t m_StateHash; // For quick comparison (idle detection)

	// Character states (sparse - only active characters)
	struct SCharacterState
	{
		int m_ClientId;
		CSaveTee m_SaveTee;
		bool m_Alive;
		vec2 m_Pos;
		vec2 m_Vel;

		// Additional state not in CSaveTee
		bool m_Super;
		bool m_Invincible;
	};
	std::vector<SCharacterState> m_vCharacters;

	// Projectile states
	struct SProjectileState
	{
		int m_Id;
		int m_Type;
		int m_Owner;
		vec2 m_Pos;
		vec2 m_Direction;
		int m_LifeSpan;
		int m_StartTick;
		bool m_Explosive;
		bool m_Freeze;
		int m_SoundImpact;
		int m_TuneZone;
	};
	std::vector<SProjectileState> m_vProjectiles;

	// Laser states
	struct SLaserState
	{
		int m_Id;
		int m_Owner;
		vec2 m_From;
		vec2 m_Pos;
		vec2 m_Dir;
		int m_EvalTick;
		float m_Energy;
		int m_Bounces;
		int m_Type;
		int m_TuneZone;
	};
	std::vector<SLaserState> m_vLasers;

	// Switcher states
	struct SSwitcherState
	{
		int m_Number;
		std::vector<bool> m_vStatus;         // Per team status [NUM_DDRACE_TEAMS]
		std::vector<int> m_vEndTick;         // Per team end tick [NUM_DDRACE_TEAMS]
		std::vector<int> m_vType;            // Per team type [NUM_DDRACE_TEAMS]
		std::vector<int> m_vLastUpdateTick;  // Per team last update tick [NUM_DDRACE_TEAMS]
	};
	std::vector<SSwitcherState> m_vSwitchers;

	// Team states
	struct STeamState
	{
		int m_Team;
		int m_TeamState;
		bool m_Practice;
		bool m_Locked;
	};
	std::vector<STeamState> m_vTeamStates;

	// World pause state
	bool m_WorldPaused;

	// Capture state from game context
	void Capture(CGameContext *pGameServer);

	// Restore state to game context
	bool Restore(CGameContext *pGameServer);

	// Calculate state hash for comparison
	uint32_t CalculateHash() const;

	// Memory size estimation
	size_t GetApproximateSize() const;

	// Serialization (binary)
	void SaveToBuffer(std::vector<uint8_t> &vBuffer) const;
	bool LoadFromBuffer(const uint8_t *pData, size_t Size);

	// Compare with another snapshot (for idle detection)
	bool IsEquivalent(const CTasSnapshot &Other) const;

private:
	void CaptureCharacter(CCharacter *pChr, SCharacterState &State);
	void CaptureProjectile(CProjectile *pProj, SProjectileState &State);
	void CaptureLaser(CLaser *pLaser, SLaserState &State);

	bool RestoreCharacter(CGameContext *pGameServer, const SCharacterState &State);
	void RestoreProjectile(CGameContext *pGameServer, const SProjectileState &State);
	void RestoreLaser(CGameContext *pGameServer, const SLaserState &State);
};

#endif // GAME_SERVER_TAS_TAS_STATE_H
