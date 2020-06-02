#ifndef GAME_SERVER_SAVE_H
#define GAME_SERVER_SAVE_H

#include "entities/character.h"
class IGameController;
class CGameContext;

class CSaveTee
{
public:
	CSaveTee();
	~CSaveTee();
	void save(CCharacter* pchr);
	void load(CCharacter* pchr, int Team);
	char* GetString();
	int LoadString(char* String);
	vec2 GetPos() const { return m_Pos; }
	const char* GetName() const { return m_aName; }

private:

	char m_aString [2048];
	char m_aName [16];

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

	char m_aGameUuid[UUID_MAXSTRSIZE];
};

class CSaveTeam
{
public:
	CSaveTeam(IGameController* Controller);
	~CSaveTeam();
	char* GetString();
	int GetMembersCount() const { return m_MembersCount; }
	int LoadString(const char* String);
	int save(int Team);
	int load(int Team);
	CSaveTee* m_pSavedTees;

	// returns true if an error occured
	static bool HandleSaveError(int Result, int ClientID, CGameContext *pGameContext);
	static void HandleLoadError(int Result, int ClientID, const CSaveTeam &SavedTeam, CGameContext *pGameContext);
private:
	int MatchPlayer(const char name[16]);
	CCharacter* MatchCharacter(const char name[16], int SaveID);

	IGameController* m_pController;

	char m_aString[65536];

	struct SSimpleSwitchers
	{
		int m_Status;
		int m_EndTime;
		int m_Type;
	};
	SSimpleSwitchers* m_pSwitchers;

	int m_TeamState;
	int m_MembersCount;
	int m_NumSwitchers;
	int m_TeamLocked;
	int m_Practice;
};

#endif // GAME_SERVER_SAVE_H
