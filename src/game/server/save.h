#ifndef GAME_SERVER_SAVE_H
#define GAME_SERVER_SAVE_H

#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontroller.h>

class IGameController;
class CGameContext;
class CCharacter;
class CSaveTeam;

class CSaveTee
{
public:
	CSaveTee();
	~CSaveTee();
	void save(CCharacter *pchr);
	void load(CCharacter *pchr, int Team);
	char *GetString(const CSaveTeam *pTeam);
	int FromString(const char *String);
	void LoadHookedPlayer(const CSaveTeam *pTeam);
	bool IsHooking() const;
	vec2 GetPos() const { return m_Pos; }
	const char *GetName() const { return m_aName; }
	int GetClientID() const { return m_ClientID; }
	void SetClientID(int ClientID) { m_ClientID = ClientID; };

private:
	int m_ClientID;

	char m_aString[2048];
	char m_aName[16];

	int m_Alive;
	int m_Paused;
	int m_NeededFaketuning;

	// Teamstuff
	int m_TeeFinished;
	int m_IsSolo;

	struct WeaponStat
	{
		int m_AmmoRegenStart;
		int m_Ammo;
		int m_Ammocost;
		int m_Got;
	} m_aWeapons[NUM_WEAPONS];

	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_SuperJump;
	int m_Jetpack;
	int m_NinjaJetpack;
	int m_FreezeTime;
	int m_FreezeTick;
	int m_DeepFreeze;
	int m_EndlessHook;
	int m_DDRaceState;

	int m_Hit;
	int m_Collision;
	int m_TuneZone;
	int m_TuneZoneOld;
	int m_Hook;
	int m_Time;
	vec2 m_Pos;
	vec2 m_PrevPos;
	int m_TeleCheckpoint;
	int m_LastPenalty;

	int m_CpTime;
	int m_CpActive;
	int m_CpLastBroadcast;
	float m_aCpCurrent[25];

	int m_NotEligibleForFinish;

	int m_HasTelegunGun;
	int m_HasTelegunGrenade;
	int m_HasTelegunLaser;

	// Core
	vec2 m_CorePos;
	vec2 m_Vel;
	int m_ActiveWeapon;
	int m_Jumped;
	int m_JumpedTotal;
	int m_Jumps;
	vec2 m_HookPos;
	vec2 m_HookDir;
	vec2 m_HookTeleBase;
	int m_HookTick;
	int m_HookState;
	int m_HookedPlayer;
	int m_NewHook;

	// player input
	int m_InputDirection;
	int m_InputJump;
	int m_InputFire;
	int m_InputHook;

	int m_ReloadTimer;

	char m_aGameUuid[UUID_MAXSTRSIZE];
};

class CSaveTeam
{
public:
	CSaveTeam(IGameController *Controller);
	~CSaveTeam();
	char *GetString();
	int GetMembersCount() const { return m_MembersCount; }
	// MatchPlayers has to be called afterwards
	int FromString(const char *String);
	// returns true if a team can load, otherwise writes a nice error Message in pMessage
	bool MatchPlayers(const char (*paNames)[MAX_NAME_LENGTH], const int *pClientID, int NumPlayer, char *pMessage, int MessageLen);
	int save(int Team);
	void load(int Team, bool KeepCurrentWeakStrong);
	CSaveTee *m_pSavedTees;

	// returns true if an error occured
	static bool HandleSaveError(int Result, int ClientID, CGameContext *pGameContext);

private:
	CCharacter *MatchCharacter(int ClientID, int SaveID, bool KeepCurrentWeakStrong);

	IGameController *m_pController;

	char m_aString[65536];

	struct SSimpleSwitchers
	{
		int m_Status;
		int m_EndTime;
		int m_Type;
	};
	SSimpleSwitchers *m_pSwitchers;

	int m_TeamState;
	int m_MembersCount;
	int m_NumSwitchers;
	int m_TeamLocked;
	int m_Practice;
};

#endif // GAME_SERVER_SAVE_H
