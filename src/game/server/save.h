#ifndef GAME_SERVER_SAVE_H
#define GAME_SERVER_SAVE_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>

class IGameController;
class CGameContext;
class CCharacter;
class CSaveTeam;

class CSaveTee
{
public:
	CSaveTee();
	~CSaveTee() = default;
	void Save(CCharacter *pchr);
	void Load(CCharacter *pchr, int Team, bool IsSwap = false);
	char *GetString(const CSaveTeam *pTeam);
	int FromString(const char *pString);
	void LoadHookedPlayer(const CSaveTeam *pTeam);
	bool IsHooking() const;
	vec2 GetPos() const { return m_Pos; }
	const char *GetName() const { return m_aName; }
	int GetClientID() const { return m_ClientID; }
	void SetClientID(int ClientID) { m_ClientID = ClientID; }

	enum
	{
		HIT_ALL = 0,
		HAMMER_HIT_DISABLED = 1,
		SHOTGUN_HIT_DISABLED = 2,
		GRENADE_HIT_DISABLED = 4,
		LASER_HIT_DISABLED = 8
	};

private:
	int m_ClientID = 0;

	char m_aString[2048] = {0};
	char m_aName[16] = {0};

	int m_Alive = 0;
	int m_Paused = 0;
	int m_NeededFaketuning = 0;

	// Teamstuff
	int m_TeeStarted = 0;
	int m_TeeFinished = 0;
	int m_IsSolo = 0;

	struct WeaponStat
	{
		int m_AmmoRegenStart = 0;
		int m_Ammo = 0;
		int m_Ammocost = 0;
		int m_Got = 0;
	} m_aWeapons[NUM_WEAPONS];

	int m_LastWeapon = 0;
	int m_QueuedWeapon = 0;

	int m_EndlessJump = 0;
	int m_Jetpack = 0;
	int m_NinjaJetpack = 0;
	int m_FreezeTime = 0;
	int m_FreezeStart = 0;
	int m_DeepFrozen = 0;
	int m_LiveFrozen = 0;
	int m_EndlessHook = 0;
	int m_DDRaceState = 0;

	int m_HitDisabledFlags = 0;
	int m_CollisionEnabled = 0;
	int m_TuneZone = 0;
	int m_TuneZoneOld = 0;
	int m_HookHitEnabled = 0;
	int m_Time = 0;
	vec2 m_Pos;
	vec2 m_PrevPos;
	int m_TeleCheckpoint = 0;
	int m_LastPenalty = 0;

	int m_TimeCpBroadcastEndTime = 0;
	int m_LastTimeCp = 0;
	int m_LastTimeCpBroadcasted = 0;
	float m_aCurrentTimeCp[MAX_CHECKPOINTS] = {0};

	int m_NotEligibleForFinish = 0;

	int m_HasTelegunGun = 0;
	int m_HasTelegunGrenade = 0;
	int m_HasTelegunLaser = 0;

	// Core
	vec2 m_CorePos;
	vec2 m_Vel;
	int m_ActiveWeapon = 0;
	int m_Jumped = 0;
	int m_JumpedTotal = 0;
	int m_Jumps = 0;
	vec2 m_HookPos;
	vec2 m_HookDir;
	vec2 m_HookTeleBase;
	int m_HookTick = 0;
	int m_HookState = 0;
	int m_HookedPlayer = 0;
	int m_NewHook = 0;

	// player input
	int m_InputDirection = 0;
	int m_InputJump = 0;
	int m_InputFire = 0;
	int m_InputHook = 0;

	int m_ReloadTimer = 0;

	char m_aGameUuid[UUID_MAXSTRSIZE] = {0};
};

class CSaveTeam
{
public:
	CSaveTeam(IGameController *pController);
	~CSaveTeam();
	char *GetString();
	int GetMembersCount() const { return m_MembersCount; }
	// MatchPlayers has to be called afterwards
	int FromString(const char *pString);
	// returns true if a team can load, otherwise writes a nice error Message in pMessage
	bool MatchPlayers(const char (*paNames)[MAX_NAME_LENGTH], const int *pClientID, int NumPlayer, char *pMessage, int MessageLen);
	int Save(int Team);
	void Load(int Team, bool KeepCurrentWeakStrong);
	CSaveTee *m_pSavedTees = nullptr;

	// returns true if an error occured
	static bool HandleSaveError(int Result, int ClientID, CGameContext *pGameContext);

private:
	CCharacter *MatchCharacter(int ClientID, int SaveID, bool KeepCurrentCharacter);

	IGameController *m_pController = nullptr;

	char m_aString[65536] = {0};

	struct SSimpleSwitchers
	{
		int m_Status = 0;
		int m_EndTime = 0;
		int m_Type = 0;
	};
	SSimpleSwitchers *m_pSwitchers = nullptr;

	int m_TeamState = 0;
	int m_MembersCount = 0;
	int m_HighestSwitchNumber = 0;
	int m_TeamLocked = 0;
	int m_Practice = 0;
};

#endif // GAME_SERVER_SAVE_H
