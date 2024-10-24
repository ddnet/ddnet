/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H

#include <game/server/entity.h>
#include <game/server/save.h>

class CGameTeams;
class CGameWorld;
class IAntibot;
struct CAntibotCharacterData;

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
	MACRO_ALLOC_POOL_ID()

	// need to use core
	friend class CSaveTee;
	friend class CSaveHotReloadTee;

public:
	CCharacter(CGameWorld *pWorld, CNetObj_PlayerInput LastInput);

	void Reset() override;
	void Destroy() override;
	void PreTick();
	void Tick() override;
	void TickDeferred() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void PostSnap() override;
	void SwapClients(int Client1, int Client2) override;

	bool CanSnapCharacter(int SnappingClient);
	bool IsSnappingCharacterInView(int SnappingClientId);

	bool IsGrounded();

	void SetWeapon(int W);
	void SetJetpack(bool Active);
	void SetEndlessJump(bool Active);
	void SetJumps(int Jumps);
	void SetSolo(bool Solo);
	void SetSuper(bool Super);
	void SetInvincible(bool Invincible);
	void SetLiveFrozen(bool Active);
	void SetDeepFrozen(bool Active);
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

	void Die(int Killer, int Weapon, bool SendKillMsg = true);
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon);

	bool Spawn(class CPlayer *pPlayer, vec2 Pos);
	bool Remove();

	bool IncreaseHealth(int Amount);
	bool IncreaseArmor(int Amount);

	void GiveWeapon(int Weapon, bool Remove = false);
	void GiveNinja();
	void RemoveNinja();
	void SetEndlessHook(bool Enable);

	void SetEmote(int Emote, int Tick);

	void Rescue();

	int NeededFaketuning() const { return m_NeededFaketuning; }
	bool IsAlive() const { return m_Alive; }
	bool IsPaused() const { return m_Paused; }
	class CPlayer *GetPlayer() { return m_pPlayer; }
	CClientMask TeamMask();

	void SetPosition(const vec2 &Position);
	void Move(vec2 RelPos);

	void ResetVelocity();
	void SetVelocity(vec2 NewVelocity);
	void SetRawVelocity(vec2 NewVelocity);
	void AddVelocity(vec2 Addition);
	void ApplyMoveRestrictions();

private:
	// player controlling this character
	class CPlayer *m_pPlayer;

	bool m_Alive;
	bool m_Paused;
	int m_PausedTick;
	int m_NeededFaketuning;

	// weapon info
	CEntity *m_apHitObjects[10];
	int m_NumObjectsHit;

	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_ReloadTimer;
	int m_AttackTick;

	int m_MoveRestrictions;

	int m_DamageTaken;

	int m_EmoteType;
	int m_EmoteStop;

	// last tick that the player took any action ie some input
	int m_LastAction;
	int m_LastNoAmmoSound;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevPrevInput;
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_PrevInput;
	CNetObj_PlayerInput m_Input;
	CNetObj_PlayerInput m_SavedInput;
	int m_NumInputs;

	int m_DamageTakenTick;

	int m_Health;
	int m_Armor;

	int m_TriggeredEvents7;

	// the player core for the physics
	CCharacterCore m_Core;
	CGameTeams *m_pTeams = nullptr;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	// DDRace

	void SnapCharacter(int SnappingClient, int Id);
	static bool IsSwitchActiveCb(int Number, void *pUser);
	void SetTimeCheckpoint(int TimeCheckpoint);
	void HandleTiles(int Index);
	float m_Time;
	int m_LastBroadcast;
	void DDRaceInit();
	void HandleSkippableTiles(int Index);
	void ForceSetRescue(int RescueMode);
	void DDRaceTick();
	void DDRacePostCoreTick();
	void HandleBroadcast();
	void HandleTuneLayer();
	void SendZoneMsgs();
	IAntibot *Antibot();

	bool m_SetSavePos[NUM_RESCUEMODES];
	CSaveTee m_RescueTee[NUM_RESCUEMODES];

public:
	CGameTeams *Teams() { return m_pTeams; }
	void SetTeams(CGameTeams *pTeams);
	bool TrySetRescue(int RescueMode);

	void FillAntibot(CAntibotCharacterData *pData);
	void Pause(bool Pause);
	bool Freeze(int Seconds);
	bool Freeze();
	bool UnFreeze();
	void GiveAllWeapons();
	void ResetPickups();
	void ResetJumps();
	int m_DDRaceState;
	int Team();
	bool CanCollide(int ClientId);
	bool SameTeam(int ClientId);
	void StopRecording();
	bool m_NinjaJetpack;
	int m_TeamBeforeSuper;
	int m_FreezeTime;
	bool m_FrozenLastTick;
	bool m_FreezeHammer;
	int m_TuneZone;
	int m_TuneZoneOld;
	int m_PainSoundTimer;
	int m_LastMove;
	int m_StartTime;
	vec2 m_PrevPos;
	int m_TeleCheckpoint;

	int m_TimeCpBroadcastEndTick;
	int m_LastTimeCp;
	int m_LastTimeCpBroadcasted;
	float m_aCurrentTimeCp[MAX_CHECKPOINTS];

	int m_TileIndex;
	int m_TileFIndex;

	int64_t m_LastStartWarning;
	int64_t m_LastRescue;
	bool m_LastRefillJumps;
	bool m_LastPenalty;
	bool m_LastBonus;
	vec2 m_TeleGunPos;
	bool m_TeleGunTeleport;
	bool m_IsBlueTeleGunTeleport;
	int m_StrongWeakId;

	int m_SpawnTick;
	int m_WeaponChangeTick;

	// Setters/Getters because i don't want to modify vanilla vars access modifiers
	int GetLastWeapon() const { return m_LastWeapon; }
	void SetLastWeapon(int LastWeap) { m_LastWeapon = LastWeap; }
	int GetActiveWeapon() const { return m_Core.m_ActiveWeapon; }
	void SetActiveWeapon(int ActiveWeap) { m_Core.m_ActiveWeapon = ActiveWeap; }
	void SetLastAction(int LastAction) { m_LastAction = LastAction; }
	int GetArmor() const { return m_Armor; }
	void SetArmor(int Armor) { m_Armor = Armor; }
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

	int GetLastAction() const { return m_LastAction; }

	bool HasTelegunGun() const { return m_Core.m_HasTelegunGun; }
	bool HasTelegunGrenade() const { return m_Core.m_HasTelegunGrenade; }
	bool HasTelegunLaser() const { return m_Core.m_HasTelegunLaser; }

	bool HammerHitDisabled() const { return m_Core.m_HammerHitDisabled; }
	bool ShotgunHitDisabled() const { return m_Core.m_ShotgunHitDisabled; }
	bool LaserHitDisabled() const { return m_Core.m_LaserHitDisabled; }
	bool GrenadeHitDisabled() const { return m_Core.m_GrenadeHitDisabled; }

	bool IsSuper() const { return m_Core.m_Super; }

	CSaveTee &GetLastRescueTeeRef(int Mode = RESCUEMODE_AUTO) { return m_RescueTee[Mode]; }
	CTuningParams *GetTuning(int Zone) { return Zone ? &TuningList()[Zone] : Tuning(); }
};

enum
{
	DDRACE_NONE = 0,
	DDRACE_STARTED,
	DDRACE_CHEAT, // no time and won't start again unless ordered by a mod or death
	DDRACE_FINISHED
};

#endif
