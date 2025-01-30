/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_CHARACTER_H
#define GAME_CLIENT_PREDICTION_ENTITIES_CHARACTER_H

#include <game/client/prediction/entity.h>

#include <game/gamecore.h>
#include <game/generated/protocol.h>

enum
{
	FAKETUNE_FREEZE = 1,
	FAKETUNE_SOLO = 2,
	FAKETUNE_NOJUMP = 4,
	FAKETUNE_NOCOLL = 8,
	FAKETUNE_NOHOOK = 16,
	FAKETUNE_JETPACK = 32,
	FAKETUNE_NOHAMMER = 64,

};

class CCharacter : public CEntity
{
	friend class CGameWorld;

public:
	~CCharacter();

	void PreTick() override;
	void Tick() override;
	void TickDeferred() override;

	bool IsGrounded();

	void SetWeapon(int W);
	void SetSolo(bool Solo);
	void SetSuper(bool Super);
	void HandleWeaponSwitch();
	void DoWeaponSwitch();

	void HandleWeapons();
	void HandleNinja();
	void HandleJetpack();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ReleaseHook();
	void ResetHook();
	void ResetInput();
	void FireWeapon();

	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon);

	void GiveWeapon(int Weapon, bool Remove = false);
	void GiveNinja();
	void RemoveNinja();

	void ResetVelocity();
	void SetVelocity(vec2 NewVelocity);
	void SetRawVelocity(vec2 NewVelocity);
	void AddVelocity(vec2 Addition);
	void ApplyMoveRestrictions();

	bool m_IsLocal;

	CTeamsCore *TeamsCore();
	bool Freeze(int Seconds);
	bool Freeze();
	bool UnFreeze();
	void GiveAllWeapons();
	int Team();
	bool CanCollide(int ClientId);
	bool SameTeam(int ClientId);
	bool m_NinjaJetpack;
	int m_FreezeTime;
	bool m_FrozenLastTick;
	vec2 m_PrevPos;
	vec2 m_PrevPrevPos;
	int m_TeleCheckpoint;

	int m_TileIndex;
	int m_TileFIndex;

	bool m_LastRefillJumps;

	// Setters/Getters because i don't want to modify vanilla vars access modifiers
	int GetLastWeapon() { return m_LastWeapon; }
	void SetLastWeapon(int LastWeap) { m_LastWeapon = LastWeap; }
	int GetActiveWeapon() { return m_Core.m_ActiveWeapon; }
	void SetActiveWeapon(int ActiveWeap);
	CCharacterCore GetCore() { return m_Core; }
	void SetCore(CCharacterCore Core) { m_Core = Core; }
	const CCharacterCore *Core() const { return &m_Core; }
	bool GetWeaponGot(int Type) { return m_Core.m_aWeapons[Type].m_Got; }
	void SetWeaponGot(int Type, bool Value) { m_Core.m_aWeapons[Type].m_Got = Value; }
	int GetWeaponAmmo(int Type) { return m_Core.m_aWeapons[Type].m_Ammo; }
	void SetWeaponAmmo(int Type, int Value) { m_Core.m_aWeapons[Type].m_Ammo = Value; }
	void SetNinjaActivationDir(vec2 ActivationDir) { m_Core.m_Ninja.m_ActivationDir = ActivationDir; }
	void SetNinjaActivationTick(int ActivationTick) { m_Core.m_Ninja.m_ActivationTick = ActivationTick; }
	void SetNinjaCurrentMoveTime(int CurrentMoveTime) { m_Core.m_Ninja.m_CurrentMoveTime = CurrentMoveTime; }
	int GetCid() { return m_Id; }
	void SetInput(const CNetObj_PlayerInput *pNewInput)
	{
		m_LatestInput = m_Input = *pNewInput;
		// it is not allowed to aim in the center
		if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		{
			m_Input.m_TargetY = m_LatestInput.m_TargetY = -1;
		}
	};
	int GetJumped() { return m_Core.m_Jumped; }
	int GetAttackTick() { return m_AttackTick; }
	int GetStrongWeakId() { return m_StrongWeakId; }

	CCharacter(CGameWorld *pGameWorld, int Id, CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended = nullptr);
	void Read(CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended, bool IsLocal);
	void SetCoreWorld(CGameWorld *pGameWorld);

	int m_LastSnapWeapon;
	int m_LastJetpackStrength;
	bool m_KeepHooked;
	int m_GameTeam;
	bool m_CanMoveInFreeze;

	bool Match(CCharacter *pChar) const;
	void ResetPrediction();
	void SetTuneZone(int Zone);
	int GetOverriddenTuneZone() const;
	int GetPureTuneZone() const;

	bool HammerHitDisabled() { return m_Core.m_HammerHitDisabled; }
	bool ShotgunHitDisabled() { return m_Core.m_ShotgunHitDisabled; }
	bool LaserHitDisabled() { return m_Core.m_LaserHitDisabled; }
	bool GrenadeHitDisabled() { return m_Core.m_GrenadeHitDisabled; }

	bool IsSuper() { return m_Core.m_Super; }

private:
	// weapon info
	int m_aHitObjects[10];
	int m_NumObjectsHit;

	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_ReloadTimer;
	int m_AttackTick;

	int m_MoveRestrictions;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_PrevInput;
	CNetObj_PlayerInput m_Input;
	CNetObj_PlayerInput m_SavedInput;

	int m_NumInputs;

	// tune
	int m_TuneZone;
	int m_TuneZoneOverride;

	// the player core for the physics
	CCharacterCore m_Core;

	// DDRace

	static bool IsSwitchActiveCb(int Number, void *pUser);
	void HandleTiles(int Index);
	void HandleSkippableTiles(int Index);
	void DDRaceTick();
	void DDRacePostCoreTick();
	void HandleTuneLayer();

	CTuningParams *CharacterTuning();

	int m_StrongWeakId;

	int m_LastWeaponSwitchTick;
	int m_LastTuneZoneTick;
};

enum
{
	DDRACE_NONE = 0,
	DDRACE_STARTED,
	DDRACE_CHEAT, // no time and won't start again unless ordered by a mod or death
	DDRACE_FINISHED
};

#endif
