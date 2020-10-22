#include "save.h"

#include <cstdio>
#include <new>

#include "gamemodes/DDRace.h"
#include "teams.h"
#include <engine/shared/config.h>

CSaveTee::CSaveTee()
{
}

CSaveTee::~CSaveTee()
{
}

void CSaveTee::save(CCharacter *pChr)
{
	m_ClientID = pChr->m_pPlayer->GetCID();
	str_copy(m_aName, pChr->m_pPlayer->Server()->ClientName(m_ClientID), sizeof(m_aName));

	m_Alive = pChr->m_Alive;
	m_Paused = abs(pChr->m_pPlayer->IsPaused());
	m_NeededFaketuning = pChr->m_NeededFaketuning;

	m_TeeFinished = pChr->Teams()->TeeFinished(m_ClientID);
	m_IsSolo = pChr->m_Solo;

	for(int i = 0; i < NUM_WEAPONS; i++)
	{
		m_aWeapons[i].m_AmmoRegenStart = pChr->m_aWeapons[i].m_AmmoRegenStart;
		m_aWeapons[i].m_Ammo = pChr->m_aWeapons[i].m_Ammo;
		m_aWeapons[i].m_Ammocost = pChr->m_aWeapons[i].m_Ammocost;
		m_aWeapons[i].m_Got = pChr->m_aWeapons[i].m_Got;
	}

	m_LastWeapon = pChr->m_LastWeapon;
	m_QueuedWeapon = pChr->m_QueuedWeapon;

	m_SuperJump = pChr->m_SuperJump;
	m_Jetpack = pChr->m_Jetpack;
	m_NinjaJetpack = pChr->m_NinjaJetpack;
	m_FreezeTime = pChr->m_FreezeTime;
	m_FreezeTick = pChr->Server()->Tick() - pChr->m_FreezeTick;

	m_DeepFreeze = pChr->m_DeepFreeze;
	m_EndlessHook = pChr->m_EndlessHook;
	m_DDRaceState = pChr->m_DDRaceState;

	m_Hit = pChr->m_Hit;
	m_TuneZone = pChr->m_TuneZone;
	m_TuneZoneOld = pChr->m_TuneZoneOld;

	if(pChr->m_StartTime)
		m_Time = pChr->Server()->Tick() - pChr->m_StartTime;
	else
		m_Time = 0;

	m_Pos = pChr->m_Pos;
	m_PrevPos = pChr->m_PrevPos;
	m_TeleCheckpoint = pChr->m_TeleCheckpoint;
	m_LastPenalty = pChr->m_LastPenalty;

	if(pChr->m_CpTick)
		m_CpTime = pChr->Server()->Tick() - pChr->m_CpTick;

	m_CpActive = pChr->m_CpActive;
	m_CpLastBroadcast = pChr->m_CpLastBroadcast;

	for(int i = 0; i < 25; i++)
		m_aCpCurrent[i] = pChr->m_CpCurrent[i];

	m_NotEligibleForFinish = pChr->m_pPlayer->m_NotEligibleForFinish;

	m_HasTelegunGun = pChr->m_Core.m_HasTelegunGun;
	m_HasTelegunGrenade = pChr->m_Core.m_HasTelegunGrenade;
	m_HasTelegunLaser = pChr->m_Core.m_HasTelegunLaser;

	// Core
	m_CorePos = pChr->m_Core.m_Pos;
	m_Vel = pChr->m_Core.m_Vel;
	m_Hook = pChr->m_Core.m_Hook;
	m_Collision = pChr->m_Core.m_Collision;
	m_ActiveWeapon = pChr->m_Core.m_ActiveWeapon;
	m_Jumped = pChr->m_Core.m_Jumped;
	m_JumpedTotal = pChr->m_Core.m_JumpedTotal;
	m_Jumps = pChr->m_Core.m_Jumps;
	m_HookPos = pChr->m_Core.m_HookPos;
	m_HookDir = pChr->m_Core.m_HookDir;
	m_HookTeleBase = pChr->m_Core.m_HookTeleBase;

	m_HookTick = pChr->m_Core.m_HookTick;

	m_HookState = pChr->m_Core.m_HookState;
	m_HookedPlayer = pChr->m_Core.m_HookedPlayer;
	m_NewHook = pChr->m_Core.m_NewHook != 0;

	m_InputDirection = pChr->m_SavedInput.m_Direction;
	m_InputJump = pChr->m_SavedInput.m_Jump;
	m_InputFire = pChr->m_SavedInput.m_Fire;
	m_InputHook = pChr->m_SavedInput.m_Hook;

	m_ReloadTimer = pChr->m_ReloadTimer;

	FormatUuid(pChr->GameServer()->GameUuid(), m_aGameUuid, sizeof(m_aGameUuid));
}

void CSaveTee::load(CCharacter *pChr, int Team)
{
	pChr->m_pPlayer->Pause(m_Paused, true);

	pChr->m_Alive = m_Alive;
	pChr->m_NeededFaketuning = m_NeededFaketuning;

	pChr->Teams()->SetForceCharacterTeam(pChr->m_pPlayer->GetCID(), Team);
	pChr->Teams()->SetFinished(pChr->m_pPlayer->GetCID(), m_TeeFinished);

	for(int i = 0; i < NUM_WEAPONS; i++)
	{
		pChr->m_aWeapons[i].m_AmmoRegenStart = m_aWeapons[i].m_AmmoRegenStart;
		// m_Ammo not used anymore for tracking freeze following https://github.com/ddnet/ddnet/pull/2086
		pChr->m_aWeapons[i].m_Ammo = -1;
		pChr->m_aWeapons[i].m_Ammocost = m_aWeapons[i].m_Ammocost;
		pChr->m_aWeapons[i].m_Got = m_aWeapons[i].m_Got;
	}

	pChr->m_LastWeapon = m_LastWeapon;
	pChr->m_QueuedWeapon = m_QueuedWeapon;

	pChr->m_SuperJump = m_SuperJump;
	pChr->m_Jetpack = m_Jetpack;
	pChr->m_NinjaJetpack = m_NinjaJetpack;
	pChr->m_FreezeTime = m_FreezeTime;
	pChr->m_FreezeTick = pChr->Server()->Tick() - m_FreezeTick;

	pChr->m_DeepFreeze = m_DeepFreeze;
	pChr->m_EndlessHook = m_EndlessHook;
	pChr->m_DDRaceState = m_DDRaceState;

	pChr->m_Hit = m_Hit;
	pChr->m_TuneZone = m_TuneZone;
	pChr->m_TuneZoneOld = m_TuneZoneOld;

	if(m_Time)
		pChr->m_StartTime = pChr->Server()->Tick() - m_Time;

	pChr->m_Pos = m_Pos;
	pChr->m_PrevPos = m_PrevPos;
	pChr->m_TeleCheckpoint = m_TeleCheckpoint;
	pChr->m_LastPenalty = m_LastPenalty;

	if(m_CpTime)
		pChr->m_CpTick = pChr->Server()->Tick() - m_CpTime;

	pChr->m_CpActive = m_CpActive;
	pChr->m_CpLastBroadcast = m_CpLastBroadcast;

	for(int i = 0; i < 25; i++)
		pChr->m_CpCurrent[i] = m_aCpCurrent[i];

	pChr->m_pPlayer->m_NotEligibleForFinish = pChr->m_pPlayer->m_NotEligibleForFinish || m_NotEligibleForFinish;

	pChr->m_Core.m_HasTelegunGun = m_HasTelegunGun;
	pChr->m_Core.m_HasTelegunLaser = m_HasTelegunLaser;
	pChr->m_Core.m_HasTelegunGrenade = m_HasTelegunGrenade;

	// Core
	pChr->m_Core.m_Pos = m_CorePos;
	pChr->m_Core.m_Vel = m_Vel;
	pChr->m_Core.m_Hook = m_Hook;
	pChr->m_Core.m_Collision = m_Collision;
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
		pChr->m_Core.m_HookedPlayer = -1;
		pChr->m_Core.m_HookState = HOOK_FLYING;
	}
	else
	{
		pChr->m_Core.m_HookedPlayer = m_HookedPlayer;
	}
	pChr->m_Core.m_NewHook = m_NewHook;
	pChr->m_SavedInput.m_Direction = m_InputDirection;
	pChr->m_SavedInput.m_Jump = m_InputJump;
	pChr->m_SavedInput.m_Fire = m_InputFire;
	pChr->m_SavedInput.m_Hook = m_InputHook;

	pChr->m_ReloadTimer = m_ReloadTimer;

	pChr->SetSolo(m_IsSolo);

	// Always create a rescue tee at the exact location we loaded from so that
	// the old one gets overwritten.
	pChr->SetRescue();
}

char *CSaveTee::GetString(const CSaveTeam *pTeam)
{
	// Add time penalty of 60 seconds (only to the database)
	int Time = m_Time + 60 * SERVER_TICK_SPEED;

	int HookedPlayer = -1;
	if(m_HookedPlayer != -1)
	{
		for(int n = 0; n < pTeam->GetMembersCount(); n++)
		{
			if(m_HookedPlayer == pTeam->m_pSavedTees[n].GetClientID())
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
		"%d\t%d\t%d\t%d\t%d\t%d\t%d\t" // m_SuperJump
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
		"%d\t"
		"%d\t%d\t%d\t"
		"%s\t"
		"%d\t%d\t"
		"%d\t%d\t%d\t%d\t"
		"%d",
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
		m_SuperJump, m_Jetpack, m_NinjaJetpack, m_FreezeTime, m_FreezeTick, m_DeepFreeze, m_EndlessHook,
		m_DDRaceState, m_Hit, m_Collision, m_TuneZone, m_TuneZoneOld, m_Hook, Time,
		(int)m_Pos.x, (int)m_Pos.y, (int)m_PrevPos.x, (int)m_PrevPos.y,
		m_TeleCheckpoint, m_LastPenalty,
		(int)m_CorePos.x, (int)m_CorePos.y, m_Vel.x, m_Vel.y,
		m_ActiveWeapon, m_Jumped, m_JumpedTotal, m_Jumps,
		(int)m_HookPos.x, (int)m_HookPos.y, m_HookDir.x, m_HookDir.y,
		(int)m_HookTeleBase.x, (int)m_HookTeleBase.y, m_HookTick, m_HookState,
		// time checkpoints
		m_CpTime, m_CpActive, m_CpLastBroadcast,
		m_aCpCurrent[0], m_aCpCurrent[1], m_aCpCurrent[2], m_aCpCurrent[3], m_aCpCurrent[4],
		m_aCpCurrent[5], m_aCpCurrent[6], m_aCpCurrent[7], m_aCpCurrent[8], m_aCpCurrent[9],
		m_aCpCurrent[10], m_aCpCurrent[11], m_aCpCurrent[12], m_aCpCurrent[13], m_aCpCurrent[14],
		m_aCpCurrent[15], m_aCpCurrent[16], m_aCpCurrent[17], m_aCpCurrent[18], m_aCpCurrent[19],
		m_aCpCurrent[20], m_aCpCurrent[21], m_aCpCurrent[22], m_aCpCurrent[23], m_aCpCurrent[24],
		m_NotEligibleForFinish,
		m_HasTelegunGun, m_HasTelegunLaser, m_HasTelegunGrenade,
		m_aGameUuid,
		HookedPlayer, m_NewHook,
		m_InputDirection, m_InputJump, m_InputFire, m_InputHook,
		m_ReloadTimer);
	return m_aString;
}

int CSaveTee::FromString(const char *String)
{
	int Num;
	Num = sscanf(String,
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
		"%d\t%d\t%d\t%d\t%d\t%d\t%d\t" // m_SuperJump
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
		"%d\t"
		"%d\t%d\t%d\t"
		"%36s\t"
		"%d\t%d"
		"%d\t%d\t%d\t%d\t"
		"%d",
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
		&m_SuperJump, &m_Jetpack, &m_NinjaJetpack, &m_FreezeTime, &m_FreezeTick, &m_DeepFreeze, &m_EndlessHook,
		&m_DDRaceState, &m_Hit, &m_Collision, &m_TuneZone, &m_TuneZoneOld, &m_Hook, &m_Time,
		&m_Pos.x, &m_Pos.y, &m_PrevPos.x, &m_PrevPos.y,
		&m_TeleCheckpoint, &m_LastPenalty,
		&m_CorePos.x, &m_CorePos.y, &m_Vel.x, &m_Vel.y,
		&m_ActiveWeapon, &m_Jumped, &m_JumpedTotal, &m_Jumps,
		&m_HookPos.x, &m_HookPos.y, &m_HookDir.x, &m_HookDir.y,
		&m_HookTeleBase.x, &m_HookTeleBase.y, &m_HookTick, &m_HookState,
		// time checkpoints
		&m_CpTime, &m_CpActive, &m_CpLastBroadcast,
		&m_aCpCurrent[0], &m_aCpCurrent[1], &m_aCpCurrent[2], &m_aCpCurrent[3], &m_aCpCurrent[4],
		&m_aCpCurrent[5], &m_aCpCurrent[6], &m_aCpCurrent[7], &m_aCpCurrent[8], &m_aCpCurrent[9],
		&m_aCpCurrent[10], &m_aCpCurrent[11], &m_aCpCurrent[12], &m_aCpCurrent[13], &m_aCpCurrent[14],
		&m_aCpCurrent[15], &m_aCpCurrent[16], &m_aCpCurrent[17], &m_aCpCurrent[18], &m_aCpCurrent[19],
		&m_aCpCurrent[20], &m_aCpCurrent[21], &m_aCpCurrent[22], &m_aCpCurrent[23], &m_aCpCurrent[24],
		&m_NotEligibleForFinish,
		&m_HasTelegunGun, &m_HasTelegunLaser, &m_HasTelegunGrenade,
		m_aGameUuid,
		&m_HookedPlayer, &m_NewHook,
		&m_InputDirection, &m_InputJump, &m_InputFire, &m_InputHook,
		&m_ReloadTimer);
	switch(Num) // Don't forget to update this when you save / load more / less.
	{
	case 96:
		m_NotEligibleForFinish = false;
		// fall through
	case 97:
		m_HasTelegunGrenade = 0;
		m_HasTelegunLaser = 0;
		m_HasTelegunGun = 0;
		// fall through
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
		// fall through
	case 108:
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
	m_HookedPlayer = pTeam->m_pSavedTees[m_HookedPlayer].GetClientID();
}

CSaveTeam::CSaveTeam(IGameController *Controller)
{
	m_pController = Controller;
	m_pSwitchers = 0;
	m_pSavedTees = 0;
}

CSaveTeam::~CSaveTeam()
{
	if(m_pSwitchers)
		delete[] m_pSwitchers;
	if(m_pSavedTees)
		delete[] m_pSavedTees;
}

int CSaveTeam::save(int Team)
{
	if(g_Config.m_SvTeam == 3 || (Team > 0 && Team < MAX_CLIENTS))
	{
		CGameTeams *Teams = &(((CGameControllerDDRace *)m_pController)->m_Teams);

		m_MembersCount = Teams->Count(Team);
		if(m_MembersCount <= 0)
		{
			return 2;
		}

		m_TeamState = Teams->GetTeamState(Team);

		if(m_TeamState != CGameTeams::TEAMSTATE_STARTED)
		{
			return 4;
		}

		m_NumSwitchers = m_pController->GameServer()->Collision()->m_NumSwitchers;
		m_TeamLocked = Teams->TeamLocked(Team);
		m_Practice = Teams->IsPractice(Team);

		m_pSavedTees = new CSaveTee[m_MembersCount];
		int j = 0;
		CCharacter *p = (CCharacter *)m_pController->GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
		for(; p; p = (CCharacter *)p->TypeNext())
		{
			if(Teams->m_Core.Team(p->GetPlayer()->GetCID()) != Team)
				continue;
			if(m_MembersCount == j)
				return 3;
			m_pSavedTees[j].save(p);
			j++;
		}
		if(m_MembersCount != j)
			return 3;

		if(m_pController->GameServer()->Collision()->m_NumSwitchers)
		{
			m_pSwitchers = new SSimpleSwitchers[m_pController->GameServer()->Collision()->m_NumSwitchers + 1];

			for(int i = 1; i < m_pController->GameServer()->Collision()->m_NumSwitchers + 1; i++)
			{
				m_pSwitchers[i].m_Status = m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Status[Team];
				if(m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team])
					m_pSwitchers[i].m_EndTime = m_pController->Server()->Tick() - m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team];
				else
					m_pSwitchers[i].m_EndTime = 0;
				m_pSwitchers[i].m_Type = m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Type[Team];
			}
		}
		return 0;
	}
	else
		return 1;
}

bool CSaveTeam::HandleSaveError(int Result, int ClientID, CGameContext *pGameContext)
{
	switch(Result)
	{
	case 0:
		return false;
	case 1:
		pGameContext->SendChatTarget(ClientID, "You have to be in a team (from 1-63)");
		break;
	case 2:
		pGameContext->SendChatTarget(ClientID, "Could not find your Team");
		break;
	case 3:
		pGameContext->SendChatTarget(ClientID, "Unable to find all Characters");
		break;
	case 4:
		pGameContext->SendChatTarget(ClientID, "Your team has not started yet");
		break;
	default: // this state should never be reached
		pGameContext->SendChatTarget(ClientID, "Unknown error while saving");
		break;
	}
	return true;
}

void CSaveTeam::load(int Team, bool KeepCurrentWeakStrong)
{
	CGameTeams *pTeams = &(((CGameControllerDDRace *)m_pController)->m_Teams);

	pTeams->ChangeTeamState(Team, m_TeamState);
	pTeams->SetTeamLock(Team, m_TeamLocked);
	if(m_Practice)
		pTeams->EnablePractice(Team);

	for(int i = m_MembersCount; i-- > 0;)
	{
		int ClientID = m_pSavedTees[i].GetClientID();
		if(m_pController->GameServer()->m_apPlayers[ClientID] && pTeams->m_Core.Team(ClientID) == Team)
		{
			CCharacter *pChr = MatchCharacter(m_pSavedTees[i].GetClientID(), i, KeepCurrentWeakStrong);
			m_pSavedTees[i].load(pChr, Team);
		}
	}

	if(m_pController->GameServer()->Collision()->m_NumSwitchers)
	{
		for(int i = 1; i < m_pController->GameServer()->Collision()->m_NumSwitchers + 1; i++)
		{
			m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Status[Team] = m_pSwitchers[i].m_Status;
			if(m_pSwitchers[i].m_EndTime)
				m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team] = m_pController->Server()->Tick() - m_pSwitchers[i].m_EndTime;
			m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Type[Team] = m_pSwitchers[i].m_Type;
		}
	}
}

CCharacter *CSaveTeam::MatchCharacter(int ClientID, int SaveID, bool KeepCurrentCharacter)
{
	if(KeepCurrentCharacter && m_pController->GameServer()->m_apPlayers[ClientID]->GetCharacter())
	{
		// keep old character to retain current weak/strong order
		return m_pController->GameServer()->m_apPlayers[ClientID]->GetCharacter();
	}
	m_pController->GameServer()->m_apPlayers[ClientID]->KillCharacter(WEAPON_GAME);
	return m_pController->GameServer()->m_apPlayers[ClientID]->ForceSpawn(m_pSavedTees[SaveID].GetPos());
}

char *CSaveTeam::GetString()
{
	str_format(m_aString, sizeof(m_aString), "%d\t%d\t%d\t%d\t%d", m_TeamState, m_MembersCount, m_NumSwitchers, m_TeamLocked, m_Practice);

	for(int i = 0; i < m_MembersCount; i++)
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "\n%s", m_pSavedTees[i].GetString(this));
		str_append(m_aString, aBuf, sizeof(m_aString));
	}

	if(m_pSwitchers && m_NumSwitchers)
	{
		for(int i = 1; i < m_NumSwitchers + 1; i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "\n%d\t%d\t%d", m_pSwitchers[i].m_Status, m_pSwitchers[i].m_EndTime, m_pSwitchers[i].m_Type);
			str_append(m_aString, aBuf, sizeof(m_aString));
		}
	}

	return m_aString;
}

int CSaveTeam::FromString(const char *String)
{
	char TeamStats[MAX_CLIENTS];
	char Switcher[64];
	char SaveTee[1024];

	char *CopyPos;
	unsigned int Pos = 0;
	unsigned int LastPos = 0;
	unsigned int StrSize;

	str_copy(m_aString, String, sizeof(m_aString));

	while(m_aString[Pos] != '\n' && Pos < sizeof(m_aString) && m_aString[Pos]) // find next \n or \0
		Pos++;

	CopyPos = m_aString + LastPos;
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

	if(StrSize < sizeof(TeamStats))
	{
		str_copy(TeamStats, CopyPos, StrSize);
		int Num = sscanf(TeamStats, "%d\t%d\t%d\t%d\t%d", &m_TeamState, &m_MembersCount, &m_NumSwitchers, &m_TeamLocked, &m_Practice);
		switch(Num) // Don't forget to update this when you save / load more / less.
		{
		case 4:
			m_Practice = false;
			// fallthrough
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

	if(m_MembersCount)
		m_pSavedTees = new CSaveTee[m_MembersCount];

	for(int n = 0; n < m_MembersCount; n++)
	{
		while(m_aString[Pos] != '\n' && Pos < sizeof(m_aString) && m_aString[Pos]) // find next \n or \0
			Pos++;

		CopyPos = m_aString + LastPos;
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

		if(StrSize < sizeof(SaveTee))
		{
			str_copy(SaveTee, CopyPos, StrSize);
			int Num = m_pSavedTees[n].FromString(SaveTee);
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

	if(m_NumSwitchers)
		m_pSwitchers = new SSimpleSwitchers[m_NumSwitchers + 1];

	for(int n = 1; n < m_NumSwitchers + 1; n++)
	{
		while(m_aString[Pos] != '\n' && Pos < sizeof(m_aString) && m_aString[Pos]) // find next \n or \0
			Pos++;

		CopyPos = m_aString + LastPos;
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

		if(StrSize < sizeof(Switcher))
		{
			str_copy(Switcher, CopyPos, StrSize);
			int Num = sscanf(Switcher, "%d\t%d\t%d", &(m_pSwitchers[n].m_Status), &(m_pSwitchers[n].m_EndTime), &(m_pSwitchers[n].m_Type));
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

bool CSaveTeam::MatchPlayers(const char (*paNames)[MAX_NAME_LENGTH], const int *pClientID, int NumPlayer, char *pMessage, int MessageLen)
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
				m_pSavedTees[i].SetClientID(pClientID[j]);
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
	// match hook to correct ClientID
	for(int n = 0; n < m_MembersCount; n++)
		m_pSavedTees[n].LoadHookedPlayer(this);
	return true;
}
