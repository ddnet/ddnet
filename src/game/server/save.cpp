#include <new>
#include <cstdio>

#include "save.h"
#include "teams.h"
#include <engine/server/server.h>
#include "./gamemodes/DDRace.h"

CSaveTee::CSaveTee()
{
	;
}

CSaveTee::~CSaveTee()
{
	;
}

void CSaveTee::save(CCharacter* pchr)
{
	str_copy(m_name, pchr->m_pPlayer->Server()->ClientName(pchr->m_pPlayer->GetCID()), sizeof(m_name));

	m_Alive = pchr->m_Alive;
	m_Paused = pchr->m_pPlayer->m_Paused;
	m_NeededFaketuning = pchr->m_NeededFaketuning;

	m_TeeFinished = pchr->Teams()->TeeFinished(pchr->m_pPlayer->GetCID());
	m_IsSolo = pchr->Teams()->m_Core.GetSolo(pchr->m_pPlayer->GetCID());

	for(int i = 0; i< NUM_WEAPONS; i++)
	{
		m_aWeapons[i].m_AmmoRegenStart = pchr->m_aWeapons[i].m_AmmoRegenStart;
		m_aWeapons[i].m_Ammo = pchr->m_aWeapons[i].m_Ammo;
		m_aWeapons[i].m_Ammocost = pchr->m_aWeapons[i].m_Ammocost;
		m_aWeapons[i].m_Got = pchr->m_aWeapons[i].m_Got;
	}

	m_LastWeapon = pchr->m_LastWeapon;
	m_QueuedWeapon = pchr->m_QueuedWeapon;

	m_SuperJump = pchr->m_SuperJump;
	m_Jetpack = pchr->m_Jetpack;
	m_NinjaJetpack = pchr->m_NinjaJetpack;
	m_FreezeTime = pchr->m_FreezeTime;

	if(pchr->m_FreezeTick)
		m_FreezeTick = pchr->Server()->Tick() - pchr->m_FreezeTick;

	m_DeepFreeze = pchr->m_DeepFreeze;
	m_EndlessHook = pchr->m_EndlessHook;
	m_DDRaceState = pchr->m_DDRaceState;

	m_Hit = pchr->m_Hit;
	m_Collision = pchr->m_Collision;
	m_TuneZone = pchr->m_TuneZone;
	m_TuneZoneOld = pchr->m_TuneZoneOld;
	m_Hook = pchr->m_Hook;

	if(pchr->m_StartTime)
		m_Time = pchr->Server()->Tick() - pchr->m_StartTime;

	m_Pos = pchr->m_Pos;
	m_PrevPos = pchr->m_PrevPos;
	m_TeleCheckpoint = pchr->m_TeleCheckpoint;
	m_LastPenalty = pchr->m_LastPenalty;

	if(pchr->m_CpTick)
		m_CpTime = pchr->Server()->Tick() - pchr->m_CpTick;

	m_CpActive = pchr->m_CpActive;
	m_CpLastBroadcast = pchr->m_CpLastBroadcast;

	for(int i = 0; i<=25; i++)
		m_CpCurrent[i] = pchr->m_CpCurrent[i];

	// Core
	m_CorePos = pchr->m_Core.m_Pos;
	m_Vel = pchr->m_Core.m_Vel;
	m_ActiveWeapon = pchr->m_Core.m_ActiveWeapon;
	m_Jumped = pchr->m_Core.m_Jumped;
	m_JumpedTotal = pchr->m_Core.m_JumpedTotal;
	m_Jumps = pchr->m_Core.m_Jumps;
	m_HookPos = pchr->m_Core.m_HookPos;
	m_HookDir = pchr->m_Core.m_HookDir;
	m_HookTeleBase = pchr->m_Core.m_HookTeleBase;

	m_HookTick = pchr->m_Core.m_HookTick;

	m_HookState = pchr->m_Core.m_HookState;
}

void CSaveTee::load(CCharacter* pchr, int Team)
{
	pchr->m_pPlayer->m_Paused = m_Paused;
	pchr->m_pPlayer->ProcessPause();

	pchr->m_Alive = m_Alive;
	pchr->m_NeededFaketuning = m_NeededFaketuning;

	pchr->Teams()->SetForceCharacterTeam(pchr->m_pPlayer->GetCID(), Team);
	pchr->Teams()->m_Core.SetSolo(pchr->m_pPlayer->GetCID(), m_IsSolo);
	pchr->Teams()->SetFinished(pchr->m_pPlayer->GetCID(), m_TeeFinished);

	for(int i = 0; i< NUM_WEAPONS; i++)
	{
		pchr->m_aWeapons[i].m_AmmoRegenStart = m_aWeapons[i].m_AmmoRegenStart;
		pchr->m_aWeapons[i].m_Ammo = m_aWeapons[i].m_Ammo;
		pchr->m_aWeapons[i].m_Ammocost = m_aWeapons[i].m_Ammocost;
		pchr->m_aWeapons[i].m_Got = m_aWeapons[i].m_Got;
	}

	pchr->m_LastWeapon = m_LastWeapon;
	pchr->m_QueuedWeapon = m_QueuedWeapon;

	pchr->m_SuperJump = m_SuperJump;
	pchr->m_Jetpack = m_Jetpack;
	pchr->m_NinjaJetpack = m_NinjaJetpack;
	pchr->m_FreezeTime = m_FreezeTime;

	if(m_FreezeTick)
		pchr->m_FreezeTick = pchr->Server()->Tick() - m_FreezeTick;

	pchr->m_DeepFreeze = m_DeepFreeze;
	pchr->m_EndlessHook = m_EndlessHook;
	pchr->m_DDRaceState = m_DDRaceState;

	pchr->m_Hit = m_Hit;
	pchr->m_Collision = m_Collision;
	pchr->m_TuneZone = m_TuneZone;
	pchr->m_TuneZoneOld = m_TuneZoneOld;
	pchr->m_Hook = m_Hook;

	if(m_Time)
		pchr->m_StartTime = pchr->Server()->Tick() - m_Time;

	pchr->m_Pos = m_Pos;
	pchr->m_PrevPos = m_PrevPos;
	pchr->m_TeleCheckpoint = m_TeleCheckpoint;
	pchr->m_LastPenalty = m_LastPenalty;

	if(m_CpTime)
		pchr->m_CpTick = pchr->Server()->Tick() - m_CpTime;

	pchr->m_CpActive  = m_CpActive;
	pchr->m_CpLastBroadcast = m_CpLastBroadcast;

	for(int i = 0; i<=25; i++)
		pchr->m_CpCurrent[i] =m_CpCurrent[i];

	// Core
	pchr->m_Core.m_Pos = m_CorePos;
	pchr->m_Core.m_Vel = m_Vel;
	pchr->m_Core.m_ActiveWeapon = m_ActiveWeapon;
	pchr->m_Core.m_Jumped = m_Jumped;
	pchr->m_Core.m_JumpedTotal = m_JumpedTotal;
	pchr->m_Core.m_Jumps = m_Jumps;
	pchr->m_Core.m_HookPos = m_HookPos;
	pchr->m_Core.m_HookDir = m_HookDir;
	pchr->m_Core.m_HookTeleBase = m_HookTeleBase;

	pchr->m_Core.m_HookTick = m_HookTick;

	if(m_HookState == HOOK_GRABBED)
	{
		pchr->m_Core.m_HookState = HOOK_FLYING;
		pchr->m_Core.m_HookedPlayer = -1;
	}
	else
	{
		pchr->m_Core.m_HookState = m_HookState;
	}
}

char* CSaveTee::GetString()
{
	str_format(m_String, sizeof(m_String), "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\t%f\t%d\t%d\t%d\t%d\t%d\t%d\t%f\t%f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", m_name, m_Alive, m_Paused, m_NeededFaketuning, m_TeeFinished, m_IsSolo, m_aWeapons[0].m_AmmoRegenStart, m_aWeapons[0].m_Ammo, m_aWeapons[0].m_Ammocost, m_aWeapons[0].m_Got, m_aWeapons[1].m_AmmoRegenStart, m_aWeapons[1].m_Ammo, m_aWeapons[1].m_Ammocost, m_aWeapons[1].m_Got, m_aWeapons[2].m_AmmoRegenStart, m_aWeapons[2].m_Ammo, m_aWeapons[2].m_Ammocost, m_aWeapons[2].m_Got, m_aWeapons[3].m_AmmoRegenStart, m_aWeapons[3].m_Ammo, m_aWeapons[3].m_Ammocost, m_aWeapons[3].m_Got, m_aWeapons[4].m_AmmoRegenStart, m_aWeapons[4].m_Ammo, m_aWeapons[4].m_Ammocost, m_aWeapons[4].m_Got, m_aWeapons[5].m_AmmoRegenStart, m_aWeapons[5].m_Ammo, m_aWeapons[5].m_Ammocost, m_aWeapons[5].m_Got, m_LastWeapon, m_QueuedWeapon, m_SuperJump, m_Jetpack, m_NinjaJetpack, m_FreezeTime, m_FreezeTick, m_DeepFreeze, m_EndlessHook, m_DDRaceState, m_Hit, m_Collision, m_TuneZone, m_TuneZoneOld, m_Hook, m_Time, (int)m_Pos.x, (int)m_Pos.y, (int)m_PrevPos.x, (int)m_PrevPos.y, m_TeleCheckpoint, m_LastPenalty, (int)m_CorePos.x, (int)m_CorePos.y, m_Vel.x, m_Vel.y, m_ActiveWeapon, m_Jumped, m_JumpedTotal, m_Jumps, (int)m_HookPos.x, (int)m_HookPos.y, m_HookDir.x, m_HookDir.y, (int)m_HookTeleBase.x, (int)m_HookTeleBase.y, m_HookTick, m_HookState, m_CpTime, m_CpActive, m_CpLastBroadcast, m_CpCurrent[0], m_CpCurrent[1], m_CpCurrent[2], m_CpCurrent[3], m_CpCurrent[4], m_CpCurrent[5], m_CpCurrent[6], m_CpCurrent[7], m_CpCurrent[8], m_CpCurrent[9], m_CpCurrent[10], m_CpCurrent[11], m_CpCurrent[12], m_CpCurrent[13], m_CpCurrent[14], m_CpCurrent[15], m_CpCurrent[16], m_CpCurrent[17], m_CpCurrent[18], m_CpCurrent[19], m_CpCurrent[20], m_CpCurrent[21], m_CpCurrent[22], m_CpCurrent[23], m_CpCurrent[24]);
	return m_String;
}

int CSaveTee::LoadString(char* String)
{
	int Num;
	Num = sscanf(String, "%[^\t]\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\t%f\t%f\t%f\t%d\t%d\t%f\t%f\t%f\t%f\t%d\t%d\t%d\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%d\t%d\t%d\t%d\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", m_name, &m_Alive, &m_Paused, &m_NeededFaketuning, &m_TeeFinished, &m_IsSolo, &m_aWeapons[0].m_AmmoRegenStart, &m_aWeapons[0].m_Ammo, &m_aWeapons[0].m_Ammocost, &m_aWeapons[0].m_Got, &m_aWeapons[1].m_AmmoRegenStart, &m_aWeapons[1].m_Ammo, &m_aWeapons[1].m_Ammocost, &m_aWeapons[1].m_Got, &m_aWeapons[2].m_AmmoRegenStart, &m_aWeapons[2].m_Ammo, &m_aWeapons[2].m_Ammocost, &m_aWeapons[2].m_Got, &m_aWeapons[3].m_AmmoRegenStart, &m_aWeapons[3].m_Ammo, &m_aWeapons[3].m_Ammocost, &m_aWeapons[3].m_Got, &m_aWeapons[4].m_AmmoRegenStart, &m_aWeapons[4].m_Ammo, &m_aWeapons[4].m_Ammocost, &m_aWeapons[4].m_Got, &m_aWeapons[5].m_AmmoRegenStart, &m_aWeapons[5].m_Ammo, &m_aWeapons[5].m_Ammocost, &m_aWeapons[5].m_Got, &m_LastWeapon, &m_QueuedWeapon, &m_SuperJump, &m_Jetpack, &m_NinjaJetpack, &m_FreezeTime, &m_FreezeTick, &m_DeepFreeze, &m_EndlessHook, &m_DDRaceState, &m_Hit, &m_Collision, &m_TuneZone, &m_TuneZoneOld, &m_Hook, &m_Time, &m_Pos.x, &m_Pos.y, &m_PrevPos.x, &m_PrevPos.y, &m_TeleCheckpoint, &m_LastPenalty, &m_CorePos.x, &m_CorePos.y, &m_Vel.x, &m_Vel.y, &m_ActiveWeapon, &m_Jumped, &m_JumpedTotal, &m_Jumps, &m_HookPos.x, &m_HookPos.y, &m_HookDir.x, &m_HookDir.y, &m_HookTeleBase.x, &m_HookTeleBase.y, &m_HookTick, &m_HookState, &m_CpTime, &m_CpActive, &m_CpLastBroadcast, &m_CpCurrent[0], &m_CpCurrent[1], &m_CpCurrent[2], &m_CpCurrent[3], &m_CpCurrent[4], &m_CpCurrent[5], &m_CpCurrent[6], &m_CpCurrent[7], &m_CpCurrent[8], &m_CpCurrent[9], &m_CpCurrent[10], &m_CpCurrent[11], &m_CpCurrent[12], &m_CpCurrent[13], &m_CpCurrent[14], &m_CpCurrent[15], &m_CpCurrent[16], &m_CpCurrent[17], &m_CpCurrent[18], &m_CpCurrent[19], &m_CpCurrent[20], &m_CpCurrent[21], &m_CpCurrent[22], &m_CpCurrent[23], &m_CpCurrent[24]);
	if (Num == 96) // Don't forget to update this when you save / load more / less.
		return 0;
	else
	{
		dbg_msg("Load", "failed to load Tee-string");
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "loaded %d vars", Num);
		dbg_msg("Load", aBuf);
		return Num+1; // never 0 here
	}
}

CSaveTeam::CSaveTeam(IGameController* Controller)
{
	m_pController = Controller;
	m_Switchers = 0;
	SavedTees = 0;
}

CSaveTeam::~CSaveTeam()
{
	if(m_Switchers)
		delete[] m_Switchers;
	if(SavedTees)
		delete[] SavedTees;
}

int CSaveTeam::save(int Team)
{
	if(Team > 0 && Team < 64)
	{
		CGameTeams* Teams = &(((CGameControllerDDRace*)m_pController)->m_Teams);
		
		if(Teams->Count(Team) <= 0)
		{
			return 2;
		}

		m_TeamState = Teams->GetTeamState(Team);
		m_MembersCount = Teams->Count(Team);
		m_NumSwitchers = m_pController->GameServer()->Collision()->m_NumSwitchers;
		m_TeamLocked = Teams->TeamLocked(Team);

		SavedTees = new CSaveTee[m_MembersCount];
		int j = 0;
		for (int i = 0; i<64; i++)
		{
			if(Teams->m_Core.Team(i) == Team)
			{
				if(m_pController->GameServer()->m_apPlayers[i]->GetCharacter())
					SavedTees[j].save(m_pController->GameServer()->m_apPlayers[i]->GetCharacter());
				else
					return 3;
				j++;
			}
		}

		if(m_pController->GameServer()->Collision()->m_NumSwitchers)
		{
			m_Switchers = new SSimpleSwitchers[m_pController->GameServer()->Collision()->m_NumSwitchers+1];

			for(int i=1; i < m_pController->GameServer()->Collision()->m_NumSwitchers+1; i++)
			{
				m_Switchers[i].m_Status = m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Status[Team];
				if(m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team])
					m_Switchers[i].m_EndTime = m_pController->Server()->Tick() - m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team];
				else
					m_Switchers[i].m_EndTime = 0;
				m_Switchers[i].m_Type = m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Type[Team];
			}
		}
		return 0;
	}
	else
	    return 1;
}

int CSaveTeam::load(int Team)
{
	if(Team > 0 && Team < 64)
	{
		CGameTeams* Teams = &(((CGameControllerDDRace*)m_pController)->m_Teams);

		Teams->ChangeTeamState(Team, m_TeamState);
		Teams->SetTeamLock(Team, m_TeamLocked);

		CCharacter* pchr;

		for (int i = 0; i<m_MembersCount; i++)
		{
			int ID = MatchPlayer(SavedTees[i].GetName());
			if(ID == -1) // first check if team can be loaded / do not load half teams
			{
				return i+10; // +10 to let space for other return-values
			}
			else if (m_pController->GameServer()->m_apPlayers[ID]->GetCharacter() && m_pController->GameServer()->m_apPlayers[ID]->GetCharacter()->m_DDRaceState)
			{
				return i+100; // +100 to let space for other return-values
			}
		}

		for (int i = 0; i<m_MembersCount; i++)
		{
			pchr = MatchCharacter(SavedTees[i].GetName(), i);
			if(pchr)
			{
				SavedTees[i].load(pchr, Team);
			}
		}

		if(m_pController->GameServer()->Collision()->m_NumSwitchers)
			for(int i=1; i < m_pController->GameServer()->Collision()->m_NumSwitchers+1; i++)
			{
				m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Status[Team] = m_Switchers[i].m_Status;
				if(m_Switchers[i].m_EndTime)
					m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team] = m_pController->Server()->Tick() - m_Switchers[i].m_EndTime;
				m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Type[Team] = m_Switchers[i].m_Type;
			}
		return 0;
	}
	return 1;
}

int CSaveTeam::MatchPlayer(char name[16])
{
	for (int i = 0; i<64; i++)
	{
		if(str_comp(m_pController->Server()->ClientName(i), name) == 0)
		{
			return i;
		}
	}
	return -1;
}

CCharacter* CSaveTeam::MatchCharacter(char name[16], int SaveID)
{
	int ID = MatchPlayer(name);
	if(ID >= 0)
	{
		if(m_pController->GameServer()->m_apPlayers[ID]->GetCharacter())
			return m_pController->GameServer()->m_apPlayers[ID]->GetCharacter();
		else
			return m_pController->GameServer()->m_apPlayers[ID]->ForceSpawn(SavedTees[SaveID].GetPos());
	}

	return 0;
}

char* CSaveTeam::GetString()
{
	str_format(m_String, sizeof(m_String), "%d\t%d\t%d\t%d", m_TeamState, m_MembersCount, m_NumSwitchers, m_TeamLocked);

	for (int i = 0; i<m_MembersCount; i++)
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "\n%s", SavedTees[i].GetString());
		str_append(m_String, aBuf, sizeof(m_String));
	}

	if(m_NumSwitchers)
		for(int i=1; i < m_NumSwitchers+1; i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "\n%d\t%d\t%d", m_Switchers[i].m_Status, m_Switchers[i].m_EndTime, m_Switchers[i].m_Type);
			str_append(m_String, aBuf, sizeof(m_String));
		}

	return m_String;
}

int CSaveTeam::LoadString(const char* String)
{
	char TeamStats[64];
	char Switcher[64];
	char SaveTee[1024];

	char* CopyPos;
	unsigned int Pos = 0;
	unsigned int LastPos = 0;
	unsigned int StrSize;

	str_copy(m_String, String, sizeof(m_String));

	while (m_String[Pos] != '\n' && Pos < sizeof(m_String) && m_String[Pos]) // find next \n or \0
		Pos++;

	CopyPos = m_String + LastPos;
	StrSize = Pos - LastPos + 1;
	if(m_String[Pos] == '\n')
	{
		Pos++; // skip \n
		LastPos = Pos;
	}

	if(StrSize <= 0)
	{
		dbg_msg("Load", "Savegame: wrong format (couldn't load TeamStats)");
		return 1;
	}

	if(StrSize < sizeof(TeamStats))
	{
		str_copy(TeamStats, CopyPos, StrSize);
		int Num = sscanf(TeamStats, "%d\t%d\t%d\t%d", &m_TeamState, &m_MembersCount, &m_NumSwitchers, &m_TeamLocked);
		if(Num != 4)
		{
			dbg_msg("Load", "failed to load Teamstats");
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "loaded %d vars", Num);
			dbg_msg("Load", aBuf);
		}
	}
	else
	{
		dbg_msg("Load", "Savegame: wrong format (couldn't load TeamStats, too big)");
		return 1;
	}

	if(SavedTees)
	{
		delete [] SavedTees;
		SavedTees = 0;
	}

	if(m_MembersCount)
		SavedTees = new CSaveTee[m_MembersCount];

	for (int n = 0; n < m_MembersCount; n++)
	{
		while (m_String[Pos] != '\n' && Pos < sizeof(m_String) && m_String[Pos]) // find next \n or \0
			Pos++;

		CopyPos = m_String + LastPos;
		StrSize = Pos - LastPos + 1;
		if(m_String[Pos] == '\n')
		{
			Pos++; // skip \n
			LastPos = Pos;
		}

		if(StrSize <= 0)
		{
			dbg_msg("Load", "Savegame: wrong format (couldn't load Tee)");
			return 1;
		}

		if(StrSize < sizeof(SaveTee))
		{
			str_copy(SaveTee, CopyPos, StrSize);
			int Num = SavedTees[n].LoadString(SaveTee);
			if(Num)
			{
				dbg_msg("Load", "failed to load Tee");
				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "loaded %d vars", Num-1);
				dbg_msg("Load", aBuf);
				return 1;
			}
		}
		else
		{
			dbg_msg("Load", "Savegame: wrong format (couldn't load Tee, too big)");
			return 1;
		}
	}

	if(m_Switchers)
	{
		delete [] m_Switchers;
		m_Switchers = 0;
	}

	if(m_NumSwitchers)
		m_Switchers = new SSimpleSwitchers[m_NumSwitchers+1];

	for (int n = 1; n < m_NumSwitchers+1; n++)
		{
			while (m_String[Pos] != '\n' && Pos < sizeof(m_String) && m_String[Pos]) // find next \n or \0
				Pos++;

			CopyPos = m_String + LastPos;
			StrSize = Pos - LastPos + 1;
			if(m_String[Pos] == '\n')
			{
				Pos++; // skip \n
				LastPos = Pos;
			}

			if(StrSize <= 0)
			{
				dbg_msg("Load", "Savegame: wrong format (couldn't load Switcher)");
				return 1;
			}

			if(StrSize < sizeof(Switcher))
			{
				str_copy(Switcher, CopyPos, StrSize);
				int Num = sscanf(Switcher, "%d\t%d\t%d", &(m_Switchers[n].m_Status), &(m_Switchers[n].m_EndTime), &(m_Switchers[n].m_Type));
				if(Num != 3)
				{
					dbg_msg("Load", "failed to load Switcher");
					char aBuf[32];
					str_format(aBuf, sizeof(aBuf), "loaded %d vars", Num-1);
					dbg_msg("Load", aBuf);
				}
			}
			else
			{
				dbg_msg("Load", "Savegame: wrong format (couldn't load Switcher, too big)");
				return 1;
			}
		}

	return 0;
}
