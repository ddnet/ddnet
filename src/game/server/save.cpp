#include "save.h"

#include <cstdio> // sscanf

#include "entities/character.h"
#include "gamemodes/DDRace.h"
#include "player.h"
#include "teams.h"
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

CSaveTee::CSaveTee() = default;

void CSaveTee::Save(CCharacter *pChr, bool AddPenalty)
{
	m_ClientId = pChr->m_pPlayer->GetCid();
	str_copy(m_aName, pChr->Server()->ClientName(m_ClientId), sizeof(m_aName));

	m_Alive = pChr->m_Alive;

	// This is extremely suspect code, probably interacts badly with force pause
	m_Paused = absolute(pChr->m_pPlayer->IsPaused());
	if(m_Paused == CPlayer::PAUSE_SPEC && !pChr->m_Paused)
	{
		m_Paused = CPlayer::PAUSE_NONE;
	}

	m_NeededFaketuning = pChr->m_NeededFaketuning;

	m_TeeStarted = pChr->Teams()->TeeStarted(m_ClientId);
	m_TeeFinished = pChr->Teams()->TeeFinished(m_ClientId);
	m_IsSolo = pChr->m_Core.m_Solo;

	for(int i = 0; i < NUM_WEAPONS; i++)
	{
		m_aWeapons[i].m_AmmoRegenStart = pChr->m_Core.m_aWeapons[i].m_AmmoRegenStart;
		m_aWeapons[i].m_Ammo = pChr->m_Core.m_aWeapons[i].m_Ammo;
		m_aWeapons[i].m_Ammocost = pChr->m_Core.m_aWeapons[i].m_Ammocost;
		m_aWeapons[i].m_Got = pChr->m_Core.m_aWeapons[i].m_Got;
	}

	m_Ninja.m_ActivationDir = pChr->m_Core.m_Ninja.m_ActivationDir;
	if(pChr->m_Core.m_Ninja.m_ActivationTick)
		m_Ninja.m_ActivationTick = pChr->Server()->Tick() - pChr->m_Core.m_Ninja.m_ActivationTick;
	else
		m_Ninja.m_ActivationTick = 0;
	m_Ninja.m_CurrentMoveTime = pChr->m_Core.m_Ninja.m_CurrentMoveTime;
	m_Ninja.m_OldVelAmount = pChr->m_Core.m_Ninja.m_OldVelAmount;

	m_LastWeapon = pChr->m_LastWeapon;
	m_QueuedWeapon = pChr->m_QueuedWeapon;

	m_EndlessJump = pChr->m_Core.m_EndlessJump;
	m_Jetpack = pChr->m_Core.m_Jetpack;
	m_NinjaJetpack = pChr->m_NinjaJetpack;
	m_FreezeTime = pChr->m_FreezeTime;
	m_FreezeStart = pChr->Server()->Tick() - pChr->m_Core.m_FreezeStart;

	m_DeepFrozen = pChr->m_Core.m_DeepFrozen;
	m_LiveFrozen = pChr->m_Core.m_LiveFrozen;
	m_EndlessHook = pChr->m_Core.m_EndlessHook;
	m_DDRaceState = pChr->m_DDRaceState;

	m_HitDisabledFlags = 0;
	if(pChr->m_Core.m_HammerHitDisabled)
		m_HitDisabledFlags |= CSaveTee::HAMMER_HIT_DISABLED;
	if(pChr->m_Core.m_ShotgunHitDisabled)
		m_HitDisabledFlags |= CSaveTee::SHOTGUN_HIT_DISABLED;
	if(pChr->m_Core.m_GrenadeHitDisabled)
		m_HitDisabledFlags |= CSaveTee::GRENADE_HIT_DISABLED;
	if(pChr->m_Core.m_LaserHitDisabled)
		m_HitDisabledFlags |= CSaveTee::LASER_HIT_DISABLED;

	m_TuneZone = pChr->m_TuneZone;
	m_TuneZoneOld = pChr->m_TuneZoneOld;

	if(pChr->m_StartTime)
		m_Time = pChr->Server()->Tick() - pChr->m_StartTime;
	else
		m_Time = 0;

	if(AddPenalty && pChr->m_StartTime)
		m_Time += g_Config.m_SvSaveSwapGamesPenalty * pChr->Server()->TickSpeed();

	m_Pos = pChr->m_Pos;
	m_PrevPos = pChr->m_PrevPos;
	m_TeleCheckpoint = pChr->m_TeleCheckpoint;
	m_LastPenalty = pChr->m_LastPenalty;

	if(pChr->m_TimeCpBroadcastEndTick)
		m_TimeCpBroadcastEndTime = pChr->Server()->Tick() - pChr->m_TimeCpBroadcastEndTick;

	m_LastTimeCp = pChr->m_LastTimeCp;
	m_LastTimeCpBroadcasted = pChr->m_LastTimeCpBroadcasted;

	for(int i = 0; i < MAX_CHECKPOINTS; i++)
		m_aCurrentTimeCp[i] = pChr->m_aCurrentTimeCp[i];

	m_NotEligibleForFinish = pChr->m_pPlayer->m_NotEligibleForFinish;

	m_HasTelegunGun = pChr->m_Core.m_HasTelegunGun;
	m_HasTelegunGrenade = pChr->m_Core.m_HasTelegunGrenade;
	m_HasTelegunLaser = pChr->m_Core.m_HasTelegunLaser;

	// Core
	m_CorePos = pChr->m_Core.m_Pos;
	m_Vel = pChr->m_Core.m_Vel;
	m_HookHitEnabled = !pChr->m_Core.m_HookHitDisabled;
	m_CollisionEnabled = !pChr->m_Core.m_CollisionDisabled;
	m_ActiveWeapon = pChr->m_Core.m_ActiveWeapon;
	m_Jumped = pChr->m_Core.m_Jumped;
	m_JumpedTotal = pChr->m_Core.m_JumpedTotal;
	m_Jumps = pChr->m_Core.m_Jumps;
	m_HookPos = pChr->m_Core.m_HookPos;
	m_HookDir = pChr->m_Core.m_HookDir;
	m_HookTeleBase = pChr->m_Core.m_HookTeleBase;

	m_HookTick = pChr->m_Core.m_HookTick;

	m_HookState = pChr->m_Core.m_HookState;
	m_HookedPlayer = pChr->m_Core.HookedPlayer();
	m_NewHook = pChr->m_Core.m_NewHook != 0;

	m_InputDirection = pChr->m_SavedInput.m_Direction;
	m_InputJump = pChr->m_SavedInput.m_Jump;
	m_InputFire = pChr->m_SavedInput.m_Fire;
	m_InputHook = pChr->m_SavedInput.m_Hook;

	m_ReloadTimer = pChr->m_ReloadTimer;

	FormatUuid(pChr->GameServer()->GameUuid(), m_aGameUuid, sizeof(m_aGameUuid));
}

bool CSaveTee::Load(CCharacter *pChr, int Team, bool IsSwap)
{
	bool Valid = true;

	pChr->m_pPlayer->Pause(m_Paused, true);

	pChr->m_Alive = m_Alive;
	pChr->m_NeededFaketuning = m_NeededFaketuning;

	if(!IsSwap)
	{
		pChr->Teams()->SetForceCharacterTeam(pChr->m_pPlayer->GetCid(), Team);
		pChr->Teams()->SetStarted(pChr->m_pPlayer->GetCid(), m_TeeStarted);
		pChr->Teams()->SetFinished(pChr->m_pPlayer->GetCid(), m_TeeFinished);
	}

	for(int i = 0; i < NUM_WEAPONS; i++)
	{
		pChr->m_Core.m_aWeapons[i].m_AmmoRegenStart = m_aWeapons[i].m_AmmoRegenStart;
		// m_Ammo not used anymore for tracking freeze following https://github.com/ddnet/ddnet/pull/2086
		pChr->m_Core.m_aWeapons[i].m_Ammo = -1;
		pChr->m_Core.m_aWeapons[i].m_Ammocost = m_aWeapons[i].m_Ammocost;
		pChr->m_Core.m_aWeapons[i].m_Got = m_aWeapons[i].m_Got;
	}

	pChr->m_Core.m_Ninja.m_ActivationDir = m_Ninja.m_ActivationDir;
	if(m_Ninja.m_ActivationTick)
		pChr->m_Core.m_Ninja.m_ActivationTick = pChr->Server()->Tick() - m_Ninja.m_ActivationTick;
	else
		pChr->m_Core.m_Ninja.m_ActivationTick = 0;
	pChr->m_Core.m_Ninja.m_CurrentMoveTime = m_Ninja.m_CurrentMoveTime;
	pChr->m_Core.m_Ninja.m_OldVelAmount = m_Ninja.m_OldVelAmount;

	pChr->m_LastWeapon = m_LastWeapon;
	pChr->m_QueuedWeapon = m_QueuedWeapon;

	pChr->m_Core.m_EndlessJump = m_EndlessJump;
	pChr->m_Core.m_Jetpack = m_Jetpack;
	pChr->m_NinjaJetpack = m_NinjaJetpack;
	pChr->m_FreezeTime = m_FreezeTime;
	pChr->m_Core.m_FreezeStart = pChr->Server()->Tick() - m_FreezeStart;

	pChr->m_Core.m_DeepFrozen = m_DeepFrozen;
	pChr->m_Core.m_LiveFrozen = m_LiveFrozen;
	pChr->m_Core.m_EndlessHook = m_EndlessHook;
	pChr->m_DDRaceState = m_DDRaceState;

	pChr->m_Core.m_HammerHitDisabled = m_HitDisabledFlags & CSaveTee::HAMMER_HIT_DISABLED;
	pChr->m_Core.m_ShotgunHitDisabled = m_HitDisabledFlags & CSaveTee::SHOTGUN_HIT_DISABLED;
	pChr->m_Core.m_GrenadeHitDisabled = m_HitDisabledFlags & CSaveTee::GRENADE_HIT_DISABLED;
	pChr->m_Core.m_LaserHitDisabled = m_HitDisabledFlags & CSaveTee::LASER_HIT_DISABLED;

	pChr->m_TuneZone = m_TuneZone;
	pChr->m_TuneZoneOld = m_TuneZoneOld;

	if(m_Time)
		pChr->m_StartTime = pChr->Server()->Tick() - m_Time;

	pChr->m_Pos = m_Pos;
	pChr->m_PrevPos = m_PrevPos;
	pChr->m_TeleCheckpoint = m_TeleCheckpoint;
	pChr->m_LastPenalty = m_LastPenalty;

	if(m_TimeCpBroadcastEndTime)
		pChr->m_TimeCpBroadcastEndTick = pChr->Server()->Tick() - m_TimeCpBroadcastEndTime;

	pChr->m_LastTimeCp = m_LastTimeCp;
	pChr->m_LastTimeCpBroadcasted = m_LastTimeCpBroadcasted;

	for(int i = 0; i < MAX_CHECKPOINTS; i++)
		pChr->m_aCurrentTimeCp[i] = m_aCurrentTimeCp[i];

	pChr->m_pPlayer->m_NotEligibleForFinish = pChr->m_pPlayer->m_NotEligibleForFinish || m_NotEligibleForFinish;

	pChr->m_Core.m_HasTelegunGun = m_HasTelegunGun;
	pChr->m_Core.m_HasTelegunLaser = m_HasTelegunLaser;
	pChr->m_Core.m_HasTelegunGrenade = m_HasTelegunGrenade;

	// Core
	pChr->m_Core.m_Pos = m_CorePos;
	pChr->m_Core.m_Vel = m_Vel;
	pChr->m_Core.m_HookHitDisabled = !m_HookHitEnabled;
	pChr->m_Core.m_CollisionDisabled = !m_CollisionEnabled;
	pChr->m_Core.m_ActiveWeapon = m_ActiveWeapon;
	pChr->m_Core.m_Jumped = m_Jumped;
	pChr->m_Core.m_JumpedTotal = m_JumpedTotal;
	pChr->m_Core.m_Jumps = m_Jumps;
	pChr->m_Core.m_HookPos = m_HookPos;
	pChr->m_Core.m_HookDir = m_HookDir;
	pChr->m_Core.m_HookTeleBase = m_HookTeleBase;

	pChr->m_Core.m_HookTick = m_HookTick;

	pChr->m_Core.m_HookState = m_HookState;
	if(m_HookedPlayer != -1 && pChr->Teams()->m_Core.Team(m_HookedPlayer) != Team)
	{
		pChr->m_Core.SetHookedPlayer(-1);
		pChr->m_Core.m_HookState = HOOK_FLYING;
	}
	else
	{
		pChr->m_Core.SetHookedPlayer(m_HookedPlayer);
	}
	pChr->m_Core.m_NewHook = m_NewHook;
	pChr->m_SavedInput.m_Direction = m_InputDirection;
	pChr->m_SavedInput.m_Jump = m_InputJump;
	pChr->m_SavedInput.m_Fire = m_InputFire;
	pChr->m_SavedInput.m_Hook = m_InputHook;

	pChr->m_ReloadTimer = m_ReloadTimer;

	pChr->SetSolo(m_IsSolo);

	if(!IsSwap)
	{
		// Always create a rescue tee at the exact location we loaded from so that
		// the old one gets overwritten.
		pChr->ForceSetRescue(RESCUEMODE_AUTO);
		pChr->ForceSetRescue(RESCUEMODE_MANUAL);
	}

	if(pChr->m_pPlayer->IsPaused() == -1 * CPlayer::PAUSE_SPEC && !pChr->m_pPlayer->CanSpec())
	{
		Valid = false;
	}

	return Valid;
}

char *CSaveTee::GetString(const CSaveTeam *pTeam)
{
	int HookedPlayer = -1;
	if(m_HookedPlayer != -1)
	{
		for(int n = 0; n < pTeam->GetMembersCount(); n++)
		{
			if(m_HookedPlayer == pTeam->m_pSavedTees[n].GetClientId())
			{
				HookedPlayer = n;
				break;
			}
		}
	}

	str_format(m_aString, sizeof(m_aString),
		"%s\t%d\t%d\t%d\t%d\t%d\t"
		// weapons
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t"
		// tee stats
		"%d\t%d\t%d\t%d\t%d\t%d\t%d\t" // m_EndlessJump
		"%d\t%d\t%d\t%d\t%d\t%d\t%d\t" // m_DDRaceState
		"%d\t%d\t%d\t%d\t" // m_Pos.x
		"%d\t%d\t" // m_TeleCheckpoint
		"%d\t%d\t%f\t%f\t" // m_CorePos.x
		"%d\t%d\t%d\t%d\t" // m_ActiveWeapon
		"%d\t%d\t%f\t%f\t" // m_HookPos.x
		"%d\t%d\t%d\t%d\t" // m_HookTeleBase.x
		// time checkpoints
		"%d\t%d\t%d\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%d\t" // m_NotEligibleForFinish
		"%d\t%d\t%d\t" // tele weapons
		"%s\t" // m_aGameUuid
		"%d\t%d\t" // m_HookedPlayer, m_NewHook
		"%d\t%d\t%d\t%d\t" // input stuff
		"%d\t" // m_ReloadTimer
		"%d\t" // m_TeeStarted
		"%d\t" //m_LiveFreeze
		"%f\t%f\t%d\t%d\t%d", // m_Ninja
		m_aName, m_Alive, m_Paused, m_NeededFaketuning, m_TeeFinished, m_IsSolo,
		// weapons
		m_aWeapons[0].m_AmmoRegenStart, m_aWeapons[0].m_Ammo, m_aWeapons[0].m_Ammocost, m_aWeapons[0].m_Got,
		m_aWeapons[1].m_AmmoRegenStart, m_aWeapons[1].m_Ammo, m_aWeapons[1].m_Ammocost, m_aWeapons[1].m_Got,
		m_aWeapons[2].m_AmmoRegenStart, m_aWeapons[2].m_Ammo, m_aWeapons[2].m_Ammocost, m_aWeapons[2].m_Got,
		m_aWeapons[3].m_AmmoRegenStart, m_aWeapons[3].m_Ammo, m_aWeapons[3].m_Ammocost, m_aWeapons[3].m_Got,
		m_aWeapons[4].m_AmmoRegenStart, m_aWeapons[4].m_Ammo, m_aWeapons[4].m_Ammocost, m_aWeapons[4].m_Got,
		m_aWeapons[5].m_AmmoRegenStart, m_aWeapons[5].m_Ammo, m_aWeapons[5].m_Ammocost, m_aWeapons[5].m_Got,
		m_LastWeapon, m_QueuedWeapon,
		// tee states
		m_EndlessJump, m_Jetpack, m_NinjaJetpack, m_FreezeTime, m_FreezeStart, m_DeepFrozen, m_EndlessHook,
		m_DDRaceState, m_HitDisabledFlags, m_CollisionEnabled, m_TuneZone, m_TuneZoneOld, m_HookHitEnabled, m_Time,
		(int)m_Pos.x, (int)m_Pos.y, (int)m_PrevPos.x, (int)m_PrevPos.y,
		m_TeleCheckpoint, m_LastPenalty,
		(int)m_CorePos.x, (int)m_CorePos.y, m_Vel.x, m_Vel.y,
		m_ActiveWeapon, m_Jumped, m_JumpedTotal, m_Jumps,
		(int)m_HookPos.x, (int)m_HookPos.y, m_HookDir.x, m_HookDir.y,
		(int)m_HookTeleBase.x, (int)m_HookTeleBase.y, m_HookTick, m_HookState,
		// time checkpoints
		m_TimeCpBroadcastEndTime, m_LastTimeCp, m_LastTimeCpBroadcasted,
		m_aCurrentTimeCp[0], m_aCurrentTimeCp[1], m_aCurrentTimeCp[2], m_aCurrentTimeCp[3], m_aCurrentTimeCp[4],
		m_aCurrentTimeCp[5], m_aCurrentTimeCp[6], m_aCurrentTimeCp[7], m_aCurrentTimeCp[8], m_aCurrentTimeCp[9],
		m_aCurrentTimeCp[10], m_aCurrentTimeCp[11], m_aCurrentTimeCp[12], m_aCurrentTimeCp[13], m_aCurrentTimeCp[14],
		m_aCurrentTimeCp[15], m_aCurrentTimeCp[16], m_aCurrentTimeCp[17], m_aCurrentTimeCp[18], m_aCurrentTimeCp[19],
		m_aCurrentTimeCp[20], m_aCurrentTimeCp[21], m_aCurrentTimeCp[22], m_aCurrentTimeCp[23], m_aCurrentTimeCp[24],
		m_NotEligibleForFinish,
		m_HasTelegunGun, m_HasTelegunLaser, m_HasTelegunGrenade,
		m_aGameUuid,
		HookedPlayer, m_NewHook,
		m_InputDirection, m_InputJump, m_InputFire, m_InputHook,
		m_ReloadTimer,
		m_TeeStarted,
		m_LiveFrozen,
		m_Ninja.m_ActivationDir.x, m_Ninja.m_ActivationDir.y, m_Ninja.m_ActivationTick, m_Ninja.m_CurrentMoveTime, m_Ninja.m_OldVelAmount);
	return m_aString;
}

int CSaveTee::FromString(const char *pString)
{
	int Num;
	Num = sscanf(pString,
		"%[^\t]\t%d\t%d\t%d\t%d\t%d\t"
		// weapons
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d\t%d\t"
		// tee states
		"%d\t%d\t%d\t%d\t%d\t%d\t%d\t" // m_EndlessJump
		"%d\t%d\t%d\t%d\t%d\t%d\t%d\t" // m_DDRaceState
		"%f\t%f\t%f\t%f\t" // m_Pos.x
		"%d\t%d\t" // m_TeleCheckpoint
		"%f\t%f\t%f\t%f\t" // m_CorePos.x
		"%d\t%d\t%d\t%d\t" // m_ActiveWeapon
		"%f\t%f\t%f\t%f\t" // m_HookPos.x
		"%f\t%f\t%d\t%d\t" // m_HookTeleBase.x
		// time checkpoints
		"%d\t%d\t%d\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%f\t%f\t%f\t%f\t%f\t"
		"%d\t" // m_NotEligibleForFinish
		"%d\t%d\t%d\t" // tele weapons
		"%36s\t" // m_aGameUuid
		"%d\t%d\t" // m_HookedPlayer, m_NewHook
		"%d\t%d\t%d\t%d\t" // input stuff
		"%d\t" // m_ReloadTimer
		"%d\t" // m_TeeStarted
		"%d\t" // m_LiveFreeze
		"%f\t%f\t%d\t%d\t%d", // m_Ninja
		m_aName, &m_Alive, &m_Paused, &m_NeededFaketuning, &m_TeeFinished, &m_IsSolo,
		// weapons
		&m_aWeapons[0].m_AmmoRegenStart, &m_aWeapons[0].m_Ammo, &m_aWeapons[0].m_Ammocost, &m_aWeapons[0].m_Got,
		&m_aWeapons[1].m_AmmoRegenStart, &m_aWeapons[1].m_Ammo, &m_aWeapons[1].m_Ammocost, &m_aWeapons[1].m_Got,
		&m_aWeapons[2].m_AmmoRegenStart, &m_aWeapons[2].m_Ammo, &m_aWeapons[2].m_Ammocost, &m_aWeapons[2].m_Got,
		&m_aWeapons[3].m_AmmoRegenStart, &m_aWeapons[3].m_Ammo, &m_aWeapons[3].m_Ammocost, &m_aWeapons[3].m_Got,
		&m_aWeapons[4].m_AmmoRegenStart, &m_aWeapons[4].m_Ammo, &m_aWeapons[4].m_Ammocost, &m_aWeapons[4].m_Got,
		&m_aWeapons[5].m_AmmoRegenStart, &m_aWeapons[5].m_Ammo, &m_aWeapons[5].m_Ammocost, &m_aWeapons[5].m_Got,
		&m_LastWeapon, &m_QueuedWeapon,
		// tee states
		&m_EndlessJump, &m_Jetpack, &m_NinjaJetpack, &m_FreezeTime, &m_FreezeStart, &m_DeepFrozen, &m_EndlessHook,
		&m_DDRaceState, &m_HitDisabledFlags, &m_CollisionEnabled, &m_TuneZone, &m_TuneZoneOld, &m_HookHitEnabled, &m_Time,
		&m_Pos.x, &m_Pos.y, &m_PrevPos.x, &m_PrevPos.y,
		&m_TeleCheckpoint, &m_LastPenalty,
		&m_CorePos.x, &m_CorePos.y, &m_Vel.x, &m_Vel.y,
		&m_ActiveWeapon, &m_Jumped, &m_JumpedTotal, &m_Jumps,
		&m_HookPos.x, &m_HookPos.y, &m_HookDir.x, &m_HookDir.y,
		&m_HookTeleBase.x, &m_HookTeleBase.y, &m_HookTick, &m_HookState,
		// time checkpoints
		&m_TimeCpBroadcastEndTime, &m_LastTimeCp, &m_LastTimeCpBroadcasted,
		&m_aCurrentTimeCp[0], &m_aCurrentTimeCp[1], &m_aCurrentTimeCp[2], &m_aCurrentTimeCp[3], &m_aCurrentTimeCp[4],
		&m_aCurrentTimeCp[5], &m_aCurrentTimeCp[6], &m_aCurrentTimeCp[7], &m_aCurrentTimeCp[8], &m_aCurrentTimeCp[9],
		&m_aCurrentTimeCp[10], &m_aCurrentTimeCp[11], &m_aCurrentTimeCp[12], &m_aCurrentTimeCp[13], &m_aCurrentTimeCp[14],
		&m_aCurrentTimeCp[15], &m_aCurrentTimeCp[16], &m_aCurrentTimeCp[17], &m_aCurrentTimeCp[18], &m_aCurrentTimeCp[19],
		&m_aCurrentTimeCp[20], &m_aCurrentTimeCp[21], &m_aCurrentTimeCp[22], &m_aCurrentTimeCp[23], &m_aCurrentTimeCp[24],
		&m_NotEligibleForFinish,
		&m_HasTelegunGun, &m_HasTelegunLaser, &m_HasTelegunGrenade,
		m_aGameUuid,
		&m_HookedPlayer, &m_NewHook,
		&m_InputDirection, &m_InputJump, &m_InputFire, &m_InputHook,
		&m_ReloadTimer,
		&m_TeeStarted,
		&m_LiveFrozen,
		&m_Ninja.m_ActivationDir.x, &m_Ninja.m_ActivationDir.y, &m_Ninja.m_ActivationTick, &m_Ninja.m_CurrentMoveTime, &m_Ninja.m_OldVelAmount);
	switch(Num) // Don't forget to update this when you save / load more / less.
	{
	case 96:
		m_NotEligibleForFinish = false;
		[[fallthrough]];
	case 97:
		m_HasTelegunGrenade = 0;
		m_HasTelegunLaser = 0;
		m_HasTelegunGun = 0;
		FormatUuid(CalculateUuid("game-uuid-nonexistent@ddnet.tw"), m_aGameUuid, sizeof(m_aGameUuid));
		[[fallthrough]];
	case 101:
		m_HookedPlayer = -1;
		m_NewHook = false;
		if(m_HookState == HOOK_GRABBED)
			m_HookState = HOOK_FLYING;
		m_InputDirection = 0;
		m_InputJump = 0;
		m_InputFire = 0;
		m_InputHook = 0;
		m_ReloadTimer = 0;
		[[fallthrough]];
	case 108:
		m_TeeStarted = true;
		[[fallthrough]];
	case 109:
		m_LiveFrozen = false;
		[[fallthrough]];
	case 110:
		if(m_aWeapons[WEAPON_NINJA].m_Got)
		{
			// remove ninja
			m_aWeapons[WEAPON_NINJA].m_Got = false;
			m_aWeapons[WEAPON_NINJA].m_Ammo = 0;
			m_ActiveWeapon = m_LastWeapon;
		}
		m_Ninja.m_ActivationDir.x = 0.0;
		m_Ninja.m_ActivationDir.y = 0.0;
		m_Ninja.m_ActivationTick = 0;
		m_Ninja.m_CurrentMoveTime = 0;
		m_Ninja.m_OldVelAmount = 0;
		[[fallthrough]];
	case 115:
		return 0;
	default:
		dbg_msg("load", "failed to load tee-string");
		dbg_msg("load", "loaded %d vars", Num);
		return Num + 1; // never 0 here
	}
}

void CSaveTee::LoadHookedPlayer(const CSaveTeam *pTeam)
{
	if(m_HookedPlayer == -1)
		return;
	m_HookedPlayer = pTeam->m_pSavedTees[m_HookedPlayer].GetClientId();
}

bool CSaveTee::IsHooking() const
{
	return m_HookState == HOOK_GRABBED || m_HookState == HOOK_FLYING;
}

CSaveTeam::CSaveTeam()
{
	m_aString[0] = '\0';
}

CSaveTeam::~CSaveTeam()
{
	delete[] m_pSwitchers;
	delete[] m_pSavedTees;
}

ESaveResult CSaveTeam::Save(CGameContext *pGameServer, int Team, bool Dry, bool Force)
{
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && (Team <= 0 || MAX_CLIENTS <= Team) && !Force)
		return ESaveResult::TEAM_FLOCK;

	IGameController *pController = pGameServer->m_pController;
	CGameTeams *pTeams = &pController->Teams();

	if(pTeams->TeamFlock(Team) && !Force)
	{
		return ESaveResult::TEAM_0_MODE;
	}

	m_MembersCount = pTeams->Count(Team);
	if(m_MembersCount <= 0 && !Force)
	{
		return ESaveResult::TEAM_NOT_FOUND;
	}

	m_TeamState = pTeams->GetTeamState(Team);

	if(m_TeamState != CGameTeams::TEAMSTATE_STARTED && !Force)
	{
		return ESaveResult::NOT_STARTED;
	}

	m_HighestSwitchNumber = pGameServer->Collision()->m_HighestSwitchNumber;
	m_TeamLocked = pTeams->TeamLocked(Team);
	m_Practice = pTeams->IsPractice(Team);

	m_pSavedTees = new CSaveTee[m_MembersCount];
	int aPlayerCids[MAX_CLIENTS];
	int j = 0;
	CCharacter *p = (CCharacter *)pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(pTeams->m_Core.Team(p->GetPlayer()->GetCid()) != Team && !Force)
			continue;
		if(m_MembersCount == j && !Force)
			return ESaveResult::CHAR_NOT_FOUND;
		ESaveResult Result = pGameServer->m_World.BlocksSave(p->GetPlayer()->GetCid());
		if(Result != ESaveResult::SUCCESS && !Force)
			return Result;
		m_pSavedTees[j].Save(p);
		aPlayerCids[j] = p->GetPlayer()->GetCid();
		j++;
	}
	if(m_MembersCount != j && !Force)
		return ESaveResult::CHAR_NOT_FOUND;

	if(pGameServer->Collision()->m_HighestSwitchNumber)
	{
		m_pSwitchers = new SSimpleSwitchers[pGameServer->Collision()->m_HighestSwitchNumber + 1];

		for(int i = 1; i < pGameServer->Collision()->m_HighestSwitchNumber + 1; i++)
		{
			m_pSwitchers[i].m_Status = pGameServer->Switchers()[i].m_aStatus[Team];
			if(pGameServer->Switchers()[i].m_aEndTick[Team])
				m_pSwitchers[i].m_EndTime = pController->Server()->Tick() - pGameServer->Switchers()[i].m_aEndTick[Team];
			else
				m_pSwitchers[i].m_EndTime = 0;
			m_pSwitchers[i].m_Type = pGameServer->Switchers()[i].m_aType[Team];
		}
	}
	if(!Dry)
	{
		pGameServer->m_World.RemoveEntitiesFromPlayers(aPlayerCids, m_MembersCount);
	}
	return ESaveResult::SUCCESS;
}

bool CSaveTeam::HandleSaveError(ESaveResult Result, int ClientId, CGameContext *pGameContext)
{
	switch(Result)
	{
	case ESaveResult::SUCCESS:
		return false;
	case ESaveResult::TEAM_FLOCK:
		pGameContext->SendChatTarget(ClientId, "You have to be in a team (from 1-63)");
		break;
	case ESaveResult::TEAM_NOT_FOUND:
		pGameContext->SendChatTarget(ClientId, "Could not find your Team");
		break;
	case ESaveResult::CHAR_NOT_FOUND:
		pGameContext->SendChatTarget(ClientId, "To save all players in your team have to be alive and not in '/spec'");
		break;
	case ESaveResult::NOT_STARTED:
		pGameContext->SendChatTarget(ClientId, "Your team has not started yet");
		break;
	case ESaveResult::TEAM_0_MODE:
		pGameContext->SendChatTarget(ClientId, "Team can't be saved while in team 0 mode");
		break;
	case ESaveResult::DRAGGER_ACTIVE:
		pGameContext->SendChatTarget(ClientId, "Team can't be saved while a dragger is active");
		break;
	}
	return true;
}

bool CSaveTeam::Load(CGameContext *pGameServer, int Team, bool KeepCurrentWeakStrong, bool IgnorePlayers)
{
	IGameController *pController = pGameServer->m_pController;
	CGameTeams *pTeams = &pController->Teams();

	pTeams->ChangeTeamState(Team, m_TeamState);
	pTeams->SetTeamLock(Team, m_TeamLocked);
	pTeams->SetPractice(Team, m_Practice);

	bool ContainsInvalidPlayer = false;
	int aPlayerCids[MAX_CLIENTS];

	if(!IgnorePlayers)
	{
		for(int i = m_MembersCount; i-- > 0;)
		{
			int ClientId = m_pSavedTees[i].GetClientId();
			aPlayerCids[i] = ClientId;
			if(pGameServer->m_apPlayers[ClientId] && pTeams->m_Core.Team(ClientId) == Team)
			{
				CCharacter *pChr = MatchCharacter(pGameServer, m_pSavedTees[i].GetClientId(), i, KeepCurrentWeakStrong);
				ContainsInvalidPlayer |= !m_pSavedTees[i].Load(pChr, Team);
			}
		}
	}

	if(pGameServer->Collision()->m_HighestSwitchNumber)
	{
		for(int i = 1; i < minimum(m_HighestSwitchNumber, pGameServer->Collision()->m_HighestSwitchNumber) + 1; i++)
		{
			pGameServer->Switchers()[i].m_aStatus[Team] = m_pSwitchers[i].m_Status;
			if(m_pSwitchers[i].m_EndTime)
				pGameServer->Switchers()[i].m_aEndTick[Team] = pController->Server()->Tick() - m_pSwitchers[i].m_EndTime;
			pGameServer->Switchers()[i].m_aType[Team] = m_pSwitchers[i].m_Type;
		}
	}
	// remove projectiles and laser
	if(!IgnorePlayers)
		pGameServer->m_World.RemoveEntitiesFromPlayers(aPlayerCids, m_MembersCount);

	return !ContainsInvalidPlayer;
}

CCharacter *CSaveTeam::MatchCharacter(CGameContext *pGameServer, int ClientId, int SaveId, bool KeepCurrentCharacter) const
{
	if(KeepCurrentCharacter && pGameServer->m_apPlayers[ClientId]->GetCharacter())
	{
		// keep old character to retain current weak/strong order
		return pGameServer->m_apPlayers[ClientId]->GetCharacter();
	}
	pGameServer->m_apPlayers[ClientId]->KillCharacter(WEAPON_GAME);
	return pGameServer->m_apPlayers[ClientId]->ForceSpawn(m_pSavedTees[SaveId].GetPos());
}

char *CSaveTeam::GetString()
{
	str_format(m_aString, sizeof(m_aString), "%d\t%d\t%d\t%d\t%d", m_TeamState, m_MembersCount, m_HighestSwitchNumber, m_TeamLocked, m_Practice);

	for(int i = 0; i < m_MembersCount; i++)
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "\n%s", m_pSavedTees[i].GetString(this));
		str_append(m_aString, aBuf);
	}

	if(m_pSwitchers && m_HighestSwitchNumber)
	{
		for(int i = 1; i < m_HighestSwitchNumber + 1; i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "\n%d\t%d\t%d", m_pSwitchers[i].m_Status, m_pSwitchers[i].m_EndTime, m_pSwitchers[i].m_Type);
			str_append(m_aString, aBuf);
		}
	}

	return m_aString;
}

int CSaveTeam::FromString(const char *pString)
{
	char aTeamStats[MAX_CLIENTS];
	char aSwitcher[64];
	char aSaveTee[1024];

	char *pCopyPos;
	unsigned int Pos = 0;
	unsigned int LastPos = 0;
	unsigned int StrSize;

	str_copy(m_aString, pString, sizeof(m_aString));

	while(m_aString[Pos] != '\n' && Pos < sizeof(m_aString) && m_aString[Pos]) // find next \n or \0
		Pos++;

	pCopyPos = m_aString + LastPos;
	StrSize = Pos - LastPos + 1;
	if(m_aString[Pos] == '\n')
	{
		Pos++; // skip \n
		LastPos = Pos;
	}

	if(StrSize <= 0)
	{
		dbg_msg("load", "savegame: wrong format (couldn't load teamstats)");
		return 1;
	}

	if(StrSize < sizeof(aTeamStats))
	{
		str_copy(aTeamStats, pCopyPos, StrSize);
		int Num = sscanf(aTeamStats, "%d\t%d\t%d\t%d\t%d", &m_TeamState, &m_MembersCount, &m_HighestSwitchNumber, &m_TeamLocked, &m_Practice);
		switch(Num) // Don't forget to update this when you save / load more / less.
		{
		case 4:
			m_Practice = false;
			[[fallthrough]];
		case 5:
			break;
		default:
			dbg_msg("load", "failed to load teamstats");
			dbg_msg("load", "loaded %d vars", Num);
			return Num + 1; // never 0 here
		}
	}
	else
	{
		dbg_msg("load", "savegame: wrong format (couldn't load teamstats, too big)");
		return 1;
	}

	if(m_pSavedTees)
	{
		delete[] m_pSavedTees;
		m_pSavedTees = 0;
	}

	if(m_MembersCount > 64)
	{
		dbg_msg("load", "savegame: team has too many players");
		return 1;
	}
	else if(m_MembersCount)
	{
		m_pSavedTees = new CSaveTee[m_MembersCount];
	}

	for(int n = 0; n < m_MembersCount; n++)
	{
		while(m_aString[Pos] != '\n' && Pos < sizeof(m_aString) && m_aString[Pos]) // find next \n or \0
			Pos++;

		pCopyPos = m_aString + LastPos;
		StrSize = Pos - LastPos + 1;
		if(m_aString[Pos] == '\n')
		{
			Pos++; // skip \n
			LastPos = Pos;
		}

		if(StrSize <= 0)
		{
			dbg_msg("load", "savegame: wrong format (couldn't load tee)");
			return 1;
		}

		if(StrSize < sizeof(aSaveTee))
		{
			str_copy(aSaveTee, pCopyPos, StrSize);
			int Num = m_pSavedTees[n].FromString(aSaveTee);
			if(Num)
			{
				dbg_msg("load", "failed to load tee");
				dbg_msg("load", "loaded %d vars", Num - 1);
				return 1;
			}
		}
		else
		{
			dbg_msg("load", "savegame: wrong format (couldn't load tee, too big)");
			return 1;
		}
	}

	if(m_pSwitchers)
	{
		delete[] m_pSwitchers;
		m_pSwitchers = 0;
	}

	if(m_HighestSwitchNumber)
		m_pSwitchers = new SSimpleSwitchers[m_HighestSwitchNumber + 1];

	for(int n = 1; n < m_HighestSwitchNumber + 1; n++)
	{
		while(m_aString[Pos] != '\n' && Pos < sizeof(m_aString) && m_aString[Pos]) // find next \n or \0
			Pos++;

		pCopyPos = m_aString + LastPos;
		StrSize = Pos - LastPos + 1;
		if(m_aString[Pos] == '\n')
		{
			Pos++; // skip \n
			LastPos = Pos;
		}

		if(StrSize <= 0)
		{
			dbg_msg("load", "savegame: wrong format (couldn't load switcher)");
			return 1;
		}

		if(StrSize < sizeof(aSwitcher))
		{
			str_copy(aSwitcher, pCopyPos, StrSize);
			int Num = sscanf(aSwitcher, "%d\t%d\t%d", &(m_pSwitchers[n].m_Status), &(m_pSwitchers[n].m_EndTime), &(m_pSwitchers[n].m_Type));
			if(Num != 3)
			{
				dbg_msg("load", "failed to load switcher");
				dbg_msg("load", "loaded %d vars", Num - 1);
			}
		}
		else
		{
			dbg_msg("load", "savegame: wrong format (couldn't load switcher, too big)");
			return 1;
		}
	}

	return 0;
}

bool CSaveTeam::MatchPlayers(const char (*paNames)[MAX_NAME_LENGTH], const int *pClientId, int NumPlayer, char *pMessage, int MessageLen) const
{
	if(NumPlayer > m_MembersCount)
	{
		str_format(pMessage, MessageLen, "Too many players in this team, should be %d", m_MembersCount);
		return false;
	}
	// check for wrong players
	for(int i = 0; i < NumPlayer; i++)
	{
		int Found = false;
		for(int j = 0; j < m_MembersCount; j++)
		{
			if(str_comp(paNames[i], m_pSavedTees[j].GetName()) == 0)
			{
				Found = true;
			}
		}
		if(!Found)
		{
			str_format(pMessage, MessageLen, "'%s' doesn't belong to this team", paNames[i]);
			return false;
		}
	}
	// check for missing players
	for(int i = 0; i < m_MembersCount; i++)
	{
		int Found = false;
		for(int j = 0; j < NumPlayer; j++)
		{
			if(str_comp(m_pSavedTees[i].GetName(), paNames[j]) == 0)
			{
				m_pSavedTees[i].SetClientId(pClientId[j]);
				Found = true;
				break;
			}
		}
		if(!Found)
		{
			str_format(pMessage, MessageLen, "'%s' has to be in this team", m_pSavedTees[i].GetName());
			return false;
		}
	}
	// match hook to correct ClientId
	for(int n = 0; n < m_MembersCount; n++)
		m_pSavedTees[n].LoadHookedPlayer(this);
	return true;
}
