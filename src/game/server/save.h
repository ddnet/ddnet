#ifndef GAME_SERVER_SAVE_H
#define GAME_SERVER_SAVE_H

#include "./entities/character.h"
#include <game/server/gamecontroller.h>

class CSaveTee
{
public:
	CSaveTee();
	~CSaveTee();
	void save(CCharacter* pchr);
	void load(CCharacter* pchr, int Team);
	char* GetString();
	int LoadString(char* String);
	vec2 GetPos() { return m_Pos; }
	char* GetName() { return m_name; }

private:

	char m_String [2048];
	char m_name [16];

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
	float m_CpCurrent[25];

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
};

class CSaveTeam
{
public:
	CSaveTeam(IGameController* Controller);
	~CSaveTeam();
	char* GetString();
	int GetMembersCount() {return m_MembersCount;}
	int LoadString(const char* String);
	int save(int Team);
	int load(int Team);
	CSaveTee* SavedTees;

private:
	int MatchPlayer(char name[16]);
	CCharacter* MatchCharacter(char name[16], int SaveID);

	IGameController* m_pController;

	char m_String[65536];

	struct SSimpleSwitchers
	{
		int m_Status;
		int m_EndTime;
		int m_Type;
	};
	SSimpleSwitchers* m_Switchers;

	int m_TeamState;
	int m_MembersCount;
	int m_NumSwitchers;
	int m_TeamLocked;
};

#endif // GAME_SERVER_SAVE_H
