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

	friend class CSaveTee; // need to use core

public:
	CCharacter(CGameWorld *pWorld, CNetObj_PlayerInput LastInput);

	void Reset() override;
	void Destroy() override;
	void Tick() override;
	void TickDefered() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void SwapClients(int Client1, int Client2) override;

	bool CanSnapCharacter(int SnappingClient);

	bool IsGrounded();

	void SetWeapon(int W);
	void SetJetpack(bool Active);
	void SetSolo(bool Solo);
	void SetSuper(bool Super);
	void SetLiveFrozen(bool Active);
	void SetDeepFrozen(bool Active);
	void HandleWeaponSwitch();
	void DoWeaponSwitch();

	void HandleWeapons();
	void HandleNinja();
	void HandleJetpack();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetHook();
	void ResetInput();
	void FireWeapon();

	void Die(int Killer, int Weapon);
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

	int NeededFaketuning() { return m_NeededFaketuning; }
	bool IsAlive() const { return m_Alive; }
	bool IsPaused() const { return m_Paused; }
	class CPlayer *GetPlayer() { return m_pPlayer; }
	int64_t TeamMask();

private:
	// player controlling this character
	class CPlayer *m_pPlayer = nullptr;

	bool m_Alive = false;
	bool m_Paused = false;
	int m_NeededFaketuning = 0;

	// weapon info
	CEntity *m_apHitObjects[10] = {nullptr};
	int m_NumObjectsHit = 0;

	int m_LastWeapon = 0;
	int m_QueuedWeapon = 0;

	int m_ReloadTimer = 0;
	int m_AttackTick = 0;

	int m_DamageTaken = 0;

	int m_EmoteType = 0;
	int m_EmoteStop = 0;

	// last tick that the player took any action ie some input
	int m_LastAction = 0;
	int m_LastNoAmmoSound = 0;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevPrevInput;
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_PrevInput;
	CNetObj_PlayerInput m_Input;
	CNetObj_PlayerInput m_SavedInput;
	int m_NumInputs = 0;

	int m_DamageTakenTick = 0;

	int m_Health = 0;
	int m_Armor = 0;

	// the player core for the physics
	CCharacterCore m_Core;
	CGameTeams *m_pTeams = nullptr;

	std::map<int, std::vector<vec2>> *m_pTeleOuts = nullptr;
	std::map<int, std::vector<vec2>> *m_pTeleCheckOuts = nullptr;

	// info for dead reckoning
	int m_ReckoningTick = 0; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	// DDRace

	void SnapCharacter(int SnappingClient, int ID);
	static bool IsSwitchActiveCb(int Number, void *pUser);
	void SetTimeCheckpoint(int TimeCheckpoint);
	void HandleTiles(int Index);
	float m_Time = 0;
	int m_LastBroadcast = 0;
	void DDRaceInit();
	void HandleSkippableTiles(int Index);
	void SetRescue();
	void DDRaceTick();
	void DDRacePostCoreTick();
	void HandleBroadcast();
	void HandleTuneLayer();
	void SendZoneMsgs();
	IAntibot *Antibot();

	bool m_SetSavePos = false;
	CSaveTee m_RescueTee;

public:
	CGameTeams *Teams() { return m_pTeams; }
	void SetTeams(CGameTeams *pTeams);
	void SetTeleports(std::map<int, std::vector<vec2>> *pTeleOuts, std::map<int, std::vector<vec2>> *pTeleCheckOuts);

	void FillAntibot(CAntibotCharacterData *pData);
	void Pause(bool Pause);
	bool Freeze(int Seconds);
	bool Freeze();
	bool UnFreeze();
	void GiveAllWeapons();
	void ResetPickups();
	int m_DDRaceState = 0;
	int Team();
	bool CanCollide(int ClientID);
	bool SameTeam(int ClientID);
	bool m_NinjaJetpack = false;
	int m_TeamBeforeSuper = 0;
	int m_FreezeTime = 0;
	bool m_FrozenLastTick = false;
	bool m_FreezeHammer = false;
	int m_TuneZone = 0;
	int m_TuneZoneOld = 0;
	int m_PainSoundTimer = 0;
	int m_LastMove = 0;
	int m_StartTime = 0;
	vec2 m_PrevPos;
	int m_TeleCheckpoint = 0;

	int m_TimeCpBroadcastEndTick = 0;
	int m_LastTimeCp = 0;
	int m_LastTimeCpBroadcasted = 0;
	float m_aCurrentTimeCp[MAX_CHECKPOINTS] = {0};

	int m_TileIndex = 0;
	int m_TileFIndex = 0;

	int m_MoveRestrictions = 0;

	vec2 m_Intersection;
	int64_t m_LastStartWarning = 0;
	int64_t m_LastRescue = 0;
	bool m_LastRefillJumps = false;
	bool m_LastPenalty = false;
	bool m_LastBonus = false;
	vec2 m_TeleGunPos;
	bool m_TeleGunTeleport = false;
	bool m_IsBlueTeleGunTeleport = false;
	int m_StrongWeakID = 0;

	int m_SpawnTick = 0;
	int m_WeaponChangeTick = 0;

	// Setters/Getters because i don't want to modify vanilla vars access modifiers
	int GetLastWeapon() { return m_LastWeapon; }
	void SetLastWeapon(int LastWeap) { m_LastWeapon = LastWeap; }
	int GetActiveWeapon() { return m_Core.m_ActiveWeapon; }
	void SetActiveWeapon(int ActiveWeap) { m_Core.m_ActiveWeapon = ActiveWeap; }
	void SetLastAction(int LastAction) { m_LastAction = LastAction; }
	int GetArmor() { return m_Armor; }
	void SetArmor(int Armor) { m_Armor = Armor; }
	CCharacterCore GetCore() { return m_Core; }
	void SetCore(CCharacterCore Core) { m_Core = Core; }
	CCharacterCore *Core() { return &m_Core; }
	bool GetWeaponGot(int Type) { return m_Core.m_aWeapons[Type].m_Got; }
	void SetWeaponGot(int Type, bool Value) { m_Core.m_aWeapons[Type].m_Got = Value; }
	int GetWeaponAmmo(int Type) { return m_Core.m_aWeapons[Type].m_Ammo; }
	void SetWeaponAmmo(int Type, int Value) { m_Core.m_aWeapons[Type].m_Ammo = Value; }
	bool IsAlive() { return m_Alive; }
	void SetNinjaActivationDir(vec2 ActivationDir) { m_Core.m_Ninja.m_ActivationDir = ActivationDir; }
	void SetNinjaActivationTick(int ActivationTick) { m_Core.m_Ninja.m_ActivationTick = ActivationTick; }
	void SetNinjaCurrentMoveTime(int CurrentMoveTime) { m_Core.m_Ninja.m_CurrentMoveTime = CurrentMoveTime; }

	int GetLastAction() const { return m_LastAction; }

	bool HasTelegunGun() { return m_Core.m_HasTelegunGun; }
	bool HasTelegunGrenade() { return m_Core.m_HasTelegunGrenade; }
	bool HasTelegunLaser() { return m_Core.m_HasTelegunLaser; }

	bool HammerHitDisabled() { return m_Core.m_HammerHitDisabled; }
	bool ShotgunHitDisabled() { return m_Core.m_ShotgunHitDisabled; }
	bool LaserHitDisabled() { return m_Core.m_LaserHitDisabled; }
	bool GrenadeHitDisabled() { return m_Core.m_GrenadeHitDisabled; }

	bool IsSuper() { return m_Core.m_Super; }

	CSaveTee &GetRescueTeeRef() { return m_RescueTee; }
};

enum
{
	DDRACE_NONE = 0,
	DDRACE_STARTED,
	DDRACE_CHEAT, // no time and won't start again unless ordered by a mod or death
	DDRACE_FINISHED
};

#endif
