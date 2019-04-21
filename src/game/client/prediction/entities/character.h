/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_CHARACTER_H
#define GAME_CLIENT_PREDICTION_ENTITIES_CHARACTER_H

#include <game/client/prediction/entity.h>
#include "projectile.h"

#include <game/gamecore.h>
#include <game/generated/client_data.h>

enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

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
	//character's size
	static const int ms_PhysSize = 28;

	virtual void Tick();
	virtual void TickDefered();

	bool IsGrounded();

	void SetWeapon(int W);
	void SetSolo(bool Solo);
	void HandleWeaponSwitch();
	void DoWeaponSwitch();

	void HandleWeapons();
	void HandleNinja();
	void HandleJetpack();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void FireWeapon();

	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon);

	bool GiveWeapon(int Weapon, int Ammo);
	void GiveNinja();
	void RemoveNinja();

	bool IsAlive() { return m_Alive; }

	bool m_Alive;

	CTeamsCore* TeamsCore();
	bool Freeze(int Time);
	bool Freeze();
	bool UnFreeze();
	void GiveAllWeapons();
	int Team();
	bool CanCollide(int ClientID);
	bool SameTeam(int ClientID);
	bool m_Super;
	bool m_SuperJump;
	bool m_Jetpack;
	int m_FreezeTime;
	int m_FreezeTick;
	bool m_DeepFreeze;
	bool m_EndlessHook;
	enum
	{
		HIT_ALL=0,
		DISABLE_HIT_HAMMER=1,
		DISABLE_HIT_SHOTGUN=2,
		DISABLE_HIT_GRENADE=4,
		DISABLE_HIT_RIFLE=8
	};
	int m_Hit;
	vec2 m_PrevPos;

	int m_TileIndex;
	int m_TileFlags;
	int m_TileFIndex;
	int m_TileFFlags;
	int m_TileSIndex;
	int m_TileSFlags;
	int m_TileIndexL;
	int m_TileFlagsL;
	int m_TileFIndexL;
	int m_TileFFlagsL;
	int m_TileSIndexL;
	int m_TileSFlagsL;
	int m_TileIndexR;
	int m_TileFlagsR;
	int m_TileFIndexR;
	int m_TileFFlagsR;
	int m_TileSIndexR;
	int m_TileSFlagsR;
	int m_TileIndexT;
	int m_TileFlagsT;
	int m_TileFIndexT;
	int m_TileFFlagsT;
	int m_TileSIndexT;
	int m_TileSFlagsT;
	int m_TileIndexB;
	int m_TileFlagsB;
	int m_TileFIndexB;
	int m_TileFFlagsB;
	int m_TileSIndexB;
	int m_TileSFlagsB;
	bool m_LastRefillJumps;

	// Setters/Getters because i don't want to modify vanilla vars access modifiers
	int GetLastWeapon() { return m_LastWeapon; };
	void SetLastWeapon(int LastWeap) {m_LastWeapon = LastWeap; };
	int GetActiveWeapon() { return m_Core.m_ActiveWeapon; };
	void SetActiveWeapon(int ActiveWeap) {m_Core.m_ActiveWeapon = ActiveWeap; };
	CCharacterCore GetCore() { return m_Core; };
	void SetCore(CCharacterCore Core) {m_Core = Core; };
	CCharacterCore* Core() { return &m_Core; };
	bool GetWeaponGot(int Type) { return m_aWeapons[Type].m_Got; };
	void SetWeaponGot(int Type, bool Value) { m_aWeapons[Type].m_Got = Value; };
	int GetWeaponAmmo(int Type) { return m_aWeapons[Type].m_Ammo; };
	void SetWeaponAmmo(int Type, int Value) { m_aWeapons[Type].m_Ammo = Value; };
	void SetNinjaActivationDir(vec2 ActivationDir) { m_Ninja.m_ActivationDir = ActivationDir; };
	void SetNinjaActivationTick(int ActivationTick) { m_Ninja.m_ActivationTick = ActivationTick; };
	void SetNinjaCurrentMoveTime(int CurrentMoveTime) { m_Ninja.m_CurrentMoveTime = CurrentMoveTime; };
	int GetCID() { return m_ID; }
	void SetInput(CNetObj_PlayerInput *pNewInput) { m_LatestInput = m_Input = *pNewInput; };
	int GetJumped() { return m_Core.m_Jumped; }

	CCharacter(CGameWorld *pGameWorld, int ID, CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended = 0);
	void Read(CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended, bool IsLocal);
	void SetCoreWorld(CGameWorld *pGameWorld);

	int m_LastSnapWeapon;
	int m_LastJetpackStrength;
	bool m_KeepHooked;
	int m_GameTeam;

	bool Match(CCharacter *pChar);
	CCharacter() { m_Alive = false; }

private:
	// weapon info
	int m_aHitObjects[10];
	int m_NumObjectsHit;

	struct WeaponStat
	{
		int m_AmmoRegenStart;
		int m_Ammo;
		int m_Ammocost;
		bool m_Got;
	} m_aWeapons[NUM_WEAPONS];

	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_ReloadTimer;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_PrevInput;
	CNetObj_PlayerInput m_Input;
	CNetObj_PlayerInput m_SavedInput;

	int m_NumInputs;

	// ninja
	struct NinjaStat
	{
		vec2 m_ActivationDir;
		int m_ActivationTick;
		int m_CurrentMoveTime;
		int m_OldVelAmount;
	} m_Ninja;

	// the player core for the physics
	CCharacterCore m_Core;

	// DDRace

	void HandleTiles(int Index);
	void HandleSkippableTiles(int Index);
	void DDRaceTick();
	void DDRacePostCoreTick();
};

enum
{
	DDRACE_NONE = 0,
	DDRACE_STARTED,
	DDRACE_CHEAT, // no time and won't start again unless ordered by a mod or death
	DDRACE_FINISHED
};

#endif
