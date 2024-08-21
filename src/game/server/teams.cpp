/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include "entities/character.h"
#include "gamecontroller.h"
#include "player.h"
#include "score.h"
#include "teehistorian.h"
#include <base/system.h>

#include <engine/shared/config.h>

#include <game/mapitems.h>

CGameTeams::CGameTeams(CGameContext *pGameContext) :
	m_pGameContext(pGameContext)
{
	Reset();
}

void CGameTeams::Reset()
{
	m_Core.Reset();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aTeeStarted[i] = false;
		m_aTeeFinished[i] = false;
		m_aLastChat[i] = 0;
		SendTeamsState(i);
	}

	for(int i = 0; i < NUM_DDRACE_TEAMS; ++i)
	{
		m_aTeamState[i] = TEAMSTATE_EMPTY;
		m_aTeamLocked[i] = false;
		m_aTeamFlock[i] = false;
		m_apSaveTeamResult[i] = nullptr;
		m_aTeamSentStartWarning[i] = false;
		ResetRoundState(i);
	}
}

void CGameTeams::ResetRoundState(int Team)
{
	ResetInvited(Team);
	if(Team != TEAM_SUPER)
		ResetSwitchers(Team);

	m_aPractice[Team] = false;
	m_aTeamUnfinishableKillTick[Team] = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_VotedForPractice = false;
			GameServer()->m_apPlayers[i]->m_SwapTargetsClientId = -1;
			m_aLastSwap[i] = 0;
		}
	}
}

void CGameTeams::ResetSwitchers(int Team)
{
	for(auto &Switcher : GameServer()->Switchers())
	{
		Switcher.m_aStatus[Team] = Switcher.m_Initial;
		Switcher.m_aEndTick[Team] = 0;
		Switcher.m_aType[Team] = TILE_SWITCHOPEN;
	}
}

void CGameTeams::OnCharacterStart(int ClientId)
{
	int Tick = Server()->Tick();
	CCharacter *pStartingChar = Character(ClientId);
	if(!pStartingChar)
		return;
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO && pStartingChar->m_DDRaceState == DDRACE_STARTED)
		return;
	if((g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO || (m_Core.Team(ClientId) != TEAM_FLOCK && !m_aTeamFlock[m_Core.Team(ClientId)])) && pStartingChar->m_DDRaceState == DDRACE_FINISHED)
		return;
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO &&
		(m_Core.Team(ClientId) == TEAM_FLOCK || TeamFlock(m_Core.Team(ClientId)) || m_Core.Team(ClientId) == TEAM_SUPER))
	{
		if(TeamFlock(m_Core.Team(ClientId)) && (m_aTeamState[m_Core.Team(ClientId)] < TEAMSTATE_STARTED))
			ChangeTeamState(m_Core.Team(ClientId), TEAMSTATE_STARTED);

		m_aTeeStarted[ClientId] = true;
		pStartingChar->m_DDRaceState = DDRACE_STARTED;
		pStartingChar->m_StartTime = Tick;
		return;
	}
	bool Waiting = false;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_Core.Team(ClientId) != m_Core.Team(i))
			continue;
		CPlayer *pPlayer = GetPlayer(i);
		if(!pPlayer || !pPlayer->IsPlaying())
			continue;
		if(GetDDRaceState(pPlayer) != DDRACE_FINISHED)
			continue;

		Waiting = true;
		pStartingChar->m_DDRaceState = DDRACE_NONE;

		if(m_aLastChat[ClientId] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s has finished and didn't go through start yet, wait for him or join another team.",
				Server()->ClientName(i));
			GameServer()->SendChatTarget(ClientId, aBuf);
			m_aLastChat[ClientId] = Tick;
		}
		if(m_aLastChat[i] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s wants to start a new round, kill or walk to start.",
				Server()->ClientName(ClientId));
			GameServer()->SendChatTarget(i, aBuf);
			m_aLastChat[i] = Tick;
		}
	}

	if(!Waiting)
	{
		m_aTeeStarted[ClientId] = true;
	}

	if(m_aTeamState[m_Core.Team(ClientId)] < TEAMSTATE_STARTED && !Waiting)
	{
		ChangeTeamState(m_Core.Team(ClientId), TEAMSTATE_STARTED);
		m_aTeamSentStartWarning[m_Core.Team(ClientId)] = false;
		m_aTeamUnfinishableKillTick[m_Core.Team(ClientId)] = -1;

		int NumPlayers = Count(m_Core.Team(ClientId));

		char aBuf[512];
		str_format(
			aBuf,
			sizeof(aBuf),
			"Team %d started with %d player%s: ",
			m_Core.Team(ClientId),
			NumPlayers,
			NumPlayers == 1 ? "" : "s");

		bool First = true;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_Core.Team(ClientId) == m_Core.Team(i))
			{
				CPlayer *pPlayer = GetPlayer(i);
				// TODO: THE PROBLEM IS THAT THERE IS NO CHARACTER SO START TIME CAN'T BE SET!
				if(pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientId))))
				{
					SetDDRaceState(pPlayer, DDRACE_STARTED);
					SetStartTime(pPlayer, Tick);

					if(First)
						First = false;
					else
						str_append(aBuf, ", ");

					str_append(aBuf, GameServer()->Server()->ClientName(i));
				}
			}
		}

		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && g_Config.m_SvMaxTeamSize != 2 && g_Config.m_SvPauseable)
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				CPlayer *pPlayer = GetPlayer(i);
				if(m_Core.Team(ClientId) == m_Core.Team(i) && pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientId))))
				{
					GameServer()->SendChatTarget(i, aBuf);
				}
			}
		}
	}
}

void CGameTeams::OnCharacterFinish(int ClientId)
{
	if(((m_Core.Team(ClientId) == TEAM_FLOCK || m_aTeamFlock[m_Core.Team(ClientId)]) && g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO) || m_Core.Team(ClientId) == TEAM_SUPER)
	{
		CPlayer *pPlayer = GetPlayer(ClientId);
		if(pPlayer && pPlayer->IsPlaying())
		{
			int TimeTicks = Server()->Tick() - GetStartTime(pPlayer);
			if(TimeTicks <= 0)
				return;
			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE); // 2019-04-02 19:41:58

			OnFinish(pPlayer, TimeTicks, aTimestamp);
		}
	}
	else
	{
		if(m_aTeeStarted[ClientId])
		{
			m_aTeeFinished[ClientId] = true;
		}
		CheckTeamFinished(m_Core.Team(ClientId));
	}
}

void CGameTeams::Tick()
{
	int Now = Server()->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayerData *pData = GameServer()->Score()->PlayerData(i);
		if(!Server()->IsRecording(i))
			continue;

		if(Now >= pData->m_RecordStopTick && pData->m_RecordStopTick != -1)
		{
			Server()->SaveDemo(i, pData->m_RecordFinishTime);
			pData->m_RecordStopTick = -1;
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamUnfinishableKillTick[i] == -1 || m_aTeamState[i] != TEAMSTATE_STARTED_UNFINISHABLE)
		{
			continue;
		}
		if(Now >= m_aTeamUnfinishableKillTick[i])
		{
			if(m_aPractice[i])
			{
				m_aTeamUnfinishableKillTick[i] = -1;
				continue;
			}
			GameServer()->SendChatTeam(i, "Your team was killed because it couldn't finish anymore and hasn't entered /practice mode");
			KillTeam(i, -1);
		}
	}

	int Frequency = Server()->TickSpeed() * 60;
	int Remainder = Server()->TickSpeed() * 30;
	uint64_t TeamHasWantedStartTime = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChar = GameServer()->m_apPlayers[i] ? GameServer()->m_apPlayers[i]->GetCharacter() : nullptr;
		int Team = m_Core.Team(i);
		if(!pChar || m_aTeamState[Team] != TEAMSTATE_STARTED || m_aTeamFlock[Team] || m_aTeeStarted[i] || m_aPractice[m_Core.Team(i)])
		{
			continue;
		}
		if((Now - pChar->m_StartTime) % Frequency == Remainder)
		{
			TeamHasWantedStartTime |= ((uint64_t)1) << m_Core.Team(i);
		}
	}
	TeamHasWantedStartTime &= ~(uint64_t)1;
	if(!TeamHasWantedStartTime)
	{
		return;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(((TeamHasWantedStartTime >> i) & 1) == 0)
		{
			continue;
		}
		if(Count(i) <= 1)
		{
			continue;
		}
		bool TeamHasCheatCharacter = false;
		int NumPlayersNotStarted = 0;
		char aPlayerNames[256];
		aPlayerNames[0] = 0;
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			if(Character(j) && Character(j)->m_DDRaceState == DDRACE_CHEAT)
				TeamHasCheatCharacter = true;
			if(m_Core.Team(j) == i && !m_aTeeStarted[j])
			{
				if(aPlayerNames[0])
				{
					str_append(aPlayerNames, ", ");
				}
				str_append(aPlayerNames, Server()->ClientName(j));
				NumPlayersNotStarted += 1;
			}
		}
		if(!aPlayerNames[0] || TeamHasCheatCharacter)
		{
			continue;
		}
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"Your team has %d %s not started yet, they need "
			"to touch the start before this team can finish: %s",
			NumPlayersNotStarted,
			NumPlayersNotStarted == 1 ? "player that has" : "players that have",
			aPlayerNames);
		GameServer()->SendChatTeam(i, aBuf);
	}
}

void CGameTeams::CheckTeamFinished(int Team)
{
	if(TeamFinished(Team))
	{
		CPlayer *apTeamPlayers[MAX_CLIENTS];
		unsigned int PlayersCount = 0;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(Team == m_Core.Team(i))
			{
				CPlayer *pPlayer = GetPlayer(i);
				if(pPlayer && pPlayer->IsPlaying())
				{
					m_aTeeStarted[i] = false;
					m_aTeeFinished[i] = false;

					apTeamPlayers[PlayersCount++] = pPlayer;
				}
			}
		}

		if(PlayersCount > 0)
		{
			int TimeTicks = Server()->Tick() - GetStartTime(apTeamPlayers[0]);
			float Time = (float)TimeTicks / (float)Server()->TickSpeed();
			if(TimeTicks <= 0)
			{
				return;
			}

			if(m_aPractice[Team])
			{
				ChangeTeamState(Team, TEAMSTATE_FINISHED);

				int min = (int)Time / 60;
				float sec = Time - (min * 60.0f);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf),
					"Your team would've finished in: %d minute(s) %5.2f second(s). Since you had practice mode enabled your rank doesn't count.",
					min, sec);
				GameServer()->SendChatTeam(Team, aBuf);

				for(unsigned int i = 0; i < PlayersCount; ++i)
				{
					SetDDRaceState(apTeamPlayers[i], DDRACE_FINISHED);
				}

				return;
			}

			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE); // 2019-04-02 19:41:58

			for(unsigned int i = 0; i < PlayersCount; ++i)
				OnFinish(apTeamPlayers[i], TimeTicks, aTimestamp);
			ChangeTeamState(Team, TEAMSTATE_FINISHED); // TODO: Make it better
			OnTeamFinish(Team, apTeamPlayers, PlayersCount, TimeTicks, aTimestamp);
		}
	}
}

const char *CGameTeams::SetCharacterTeam(int ClientId, int Team)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return "Invalid client ID";
	if(Team < 0 || Team >= MAX_CLIENTS + 1)
		return "Invalid team number";
	if(Team != TEAM_SUPER && m_aTeamState[Team] > TEAMSTATE_OPEN && !m_aPractice[Team] && !m_aTeamFlock[Team])
		return "This team started already";
	if(m_Core.Team(ClientId) == Team)
		return "You are in this team already";
	if(!Character(ClientId))
		return "Your character is not valid";
	if(Team == TEAM_SUPER && !Character(ClientId)->IsSuper())
		return "You can't join super team if you don't have super rights";
	if(Team != TEAM_SUPER && Character(ClientId)->m_DDRaceState != DDRACE_NONE)
		return "You have started racing already";
	// No cheating through noob filter with practice and then leaving team
	if(m_aPractice[m_Core.Team(ClientId)])
		return "You have used practice mode already";

	// you can not join a team which is currently in the process of saving,
	// because the save-process can fail and then the team is reset into the game
	if(Team != TEAM_SUPER && GetSaving(Team))
		return "Your team is currently saving";
	if(m_Core.Team(ClientId) != TEAM_SUPER && GetSaving(m_Core.Team(ClientId)))
		return "This team is currently saving";

	SetForceCharacterTeam(ClientId, Team);
	return nullptr;
}

void CGameTeams::SetForceCharacterTeam(int ClientId, int Team)
{
	m_aTeeStarted[ClientId] = false;
	m_aTeeFinished[ClientId] = false;
	int OldTeam = m_Core.Team(ClientId);

	if(Team != OldTeam && (OldTeam != TEAM_FLOCK || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO) && OldTeam != TEAM_SUPER && m_aTeamState[OldTeam] != TEAMSTATE_EMPTY)
	{
		bool NoElseInOldTeam = Count(OldTeam) <= 1;
		if(NoElseInOldTeam)
		{
			m_aTeamState[OldTeam] = TEAMSTATE_EMPTY;

			// unlock team when last player leaves
			SetTeamLock(OldTeam, false);
			SetTeamFlock(OldTeam, false);
			ResetRoundState(OldTeam);
			// do not reset SaveTeamResult, because it should be logged into teehistorian even if the team leaves
		}
	}

	m_Core.Team(ClientId, Team);

	if(OldTeam != Team)
	{
		for(int LoopClientId = 0; LoopClientId < MAX_CLIENTS; ++LoopClientId)
			if(GetPlayer(LoopClientId))
				SendTeamsState(LoopClientId);

		if(GetPlayer(ClientId))
		{
			GetPlayer(ClientId)->m_VotedForPractice = false;
			GetPlayer(ClientId)->m_SwapTargetsClientId = -1;
		}
		m_pGameContext->m_World.RemoveEntitiesFromPlayer(ClientId);
	}

	if(Team != TEAM_SUPER && (m_aTeamState[Team] == TEAMSTATE_EMPTY || (m_aTeamLocked[Team] && !m_aTeamFlock[Team])))
	{
		if(!m_aTeamLocked[Team])
			ChangeTeamState(Team, TEAMSTATE_OPEN);

		ResetSwitchers(Team);
	}
}

int CGameTeams::Count(int Team) const
{
	if(Team == TEAM_SUPER)
		return -1;

	int Count = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team)
			Count++;

	return Count;
}

void CGameTeams::ChangeTeamState(int Team, int State)
{
	m_aTeamState[Team] = State;
}

void CGameTeams::KillTeam(int Team, int NewStrongId, int ExceptId)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_VotedForPractice = false;
			if(i != ExceptId)
			{
				GameServer()->m_apPlayers[i]->KillCharacter(WEAPON_SELF, false);
				if(NewStrongId != -1 && i != NewStrongId)
				{
					GameServer()->m_apPlayers[i]->Respawn(true); // spawn the rest of team with weak hook on the killer
				}
			}
		}
	}

	// send the team kill message
	CNetMsg_Sv_KillMsgTeam Msg;
	Msg.m_Team = Team;
	Msg.m_First = NewStrongId;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

bool CGameTeams::TeamFinished(int Team)
{
	if(m_aTeamState[Team] != TEAMSTATE_STARTED)
	{
		return false;
	}
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team && !m_aTeeFinished[i])
			return false;
	return true;
}

CClientMask CGameTeams::TeamMask(int Team, int ExceptId, int Asker, int VersionFlags)
{
	if(Team == TEAM_SUPER)
	{
		if(ExceptId == -1)
			return CClientMask().set();
		return CClientMask().set().reset(ExceptId);
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == ExceptId)
			continue; // Explicitly excluded
		if(!GetPlayer(i))
			continue; // Player doesn't exist
		if(!((Server()->IsSixup(i) && (VersionFlags & CGameContext::FLAG_SIXUP)) ||
			   (!Server()->IsSixup(i) && (VersionFlags & CGameContext::FLAG_SIX))))
			continue;

		if(!(GetPlayer(i)->GetTeam() == TEAM_SPECTATORS || GetPlayer(i)->IsPaused()))
		{ // Not spectator
			if(i != Asker)
			{ // Actions of other players
				if(!Character(i))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if(m_Core.GetSolo(i))
						continue; // When in solo part don't show others
					if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of yourself
		}
		else if(GetPlayer(i)->m_SpectatorId != SPEC_FREEVIEW)
		{ // Spectating specific player
			if(GetPlayer(i)->m_SpectatorId != Asker)
			{ // Actions of other players
				if(!Character(GetPlayer(i)->m_SpectatorId))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(m_Core.Team(GetPlayer(i)->m_SpectatorId) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorId) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if(m_Core.GetSolo(GetPlayer(i)->m_SpectatorId))
						continue; // When in solo part don't show others
					if(m_Core.Team(GetPlayer(i)->m_SpectatorId) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorId) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of player you're spectating
		}
		else
		{ // Freeview
			if(GetPlayer(i)->m_SpecTeam)
			{ // Show only players in own team when spectating
				if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
					continue; // in different teams
			}
		}

		Mask.set(i);
	}
	return Mask;
}

void CGameTeams::SendTeamsState(int ClientId)
{
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
		return;

	if(!m_pGameContext->m_apPlayers[ClientId])
		return;

	CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);
	CMsgPacker MsgLegacy(NETMSGTYPE_SV_TEAMSSTATELEGACY);

	for(unsigned i = 0; i < MAX_CLIENTS; i++)
	{
		Msg.AddInt(m_Core.Team(i));
		MsgLegacy.AddInt(m_Core.Team(i));
	}

	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
	int ClientVersion = m_pGameContext->m_apPlayers[ClientId]->GetClientVersion();
	if(!Server()->IsSixup(ClientId) && VERSION_DDRACE < ClientVersion && ClientVersion < VERSION_DDNET_MSG_LEGACY)
	{
		Server()->SendMsg(&MsgLegacy, MSGFLAG_VITAL, ClientId);
	}
}

int CGameTeams::GetDDRaceState(CPlayer *Player)
{
	if(!Player)
		return DDRACE_NONE;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_DDRaceState;
	return DDRACE_NONE;
}

void CGameTeams::SetDDRaceState(CPlayer *Player, int DDRaceState)
{
	if(!Player)
		return;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		pChar->m_DDRaceState = DDRaceState;
}

int CGameTeams::GetStartTime(CPlayer *Player)
{
	if(!Player)
		return 0;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_StartTime;
	return 0;
}

void CGameTeams::SetStartTime(CPlayer *Player, int StartTime)
{
	if(!Player)
		return;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		pChar->m_StartTime = StartTime;
}

void CGameTeams::SetLastTimeCp(CPlayer *Player, int LastTimeCp)
{
	if(!Player)
		return;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		pChar->m_LastTimeCp = LastTimeCp;
}

float *CGameTeams::GetCurrentTimeCp(CPlayer *Player)
{
	if(!Player)
		return NULL;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_aCurrentTimeCp;
	return NULL;
}

void CGameTeams::OnTeamFinish(int Team, CPlayer **Players, unsigned int Size, int TimeTicks, const char *pTimestamp)
{
	int aPlayerCids[MAX_CLIENTS];

	for(unsigned int i = 0; i < Size; i++)
	{
		aPlayerCids[i] = Players[i]->GetCid();

		if(g_Config.m_SvRejoinTeam0 && g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && (m_Core.Team(Players[i]->GetCid()) >= TEAM_SUPER || !m_aTeamLocked[m_Core.Team(Players[i]->GetCid())]))
		{
			SetForceCharacterTeam(Players[i]->GetCid(), TEAM_FLOCK);
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "'%s' joined team 0",
				GameServer()->Server()->ClientName(Players[i]->GetCid()));
			GameServer()->SendChat(-1, TEAM_ALL, aBuf);
		}
	}

	if(Size >= (unsigned int)g_Config.m_SvMinTeamSize)
		GameServer()->Score()->SaveTeamScore(Team, aPlayerCids, Size, TimeTicks, pTimestamp);
}

void CGameTeams::OnFinish(CPlayer *Player, int TimeTicks, const char *pTimestamp)
{
	if(!Player || !Player->IsPlaying())
		return;

	float Time = TimeTicks / (float)Server()->TickSpeed();

	// TODO:DDRace:btd: this ugly
	const int ClientId = Player->GetCid();
	CPlayerData *pData = GameServer()->Score()->PlayerData(ClientId);

	char aBuf[128];
	SetLastTimeCp(Player, -1);
	// Note that the "finished in" message is parsed by the client
	str_format(aBuf, sizeof(aBuf),
		"%s finished in: %d minute(s) %5.2f second(s)",
		Server()->ClientName(ClientId), (int)Time / 60,
		Time - ((int)Time / 60 * 60));
	if(g_Config.m_SvHideScore || !g_Config.m_SvSaveWorseScores)
		GameServer()->SendChatTarget(ClientId, aBuf, CGameContext::FLAG_SIX);
	else
		GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1., CGameContext::FLAG_SIX);

	float Diff = absolute(Time - pData->m_BestTime);

	if(Time - pData->m_BestTime < 0)
	{
		// new record \o/
		pData->m_RecordStopTick = Server()->Tick() + Server()->TickSpeed();
		pData->m_RecordFinishTime = Time;

		if(Diff >= 60)
			str_format(aBuf, sizeof(aBuf), "New record: %d minute(s) %5.2f second(s) better.",
				(int)Diff / 60, Diff - ((int)Diff / 60 * 60));
		else
			str_format(aBuf, sizeof(aBuf), "New record: %5.2f second(s) better.",
				Diff);
		if(g_Config.m_SvHideScore || !g_Config.m_SvSaveWorseScores)
			GameServer()->SendChatTarget(ClientId, aBuf, CGameContext::FLAG_SIX);
		else
			GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1, CGameContext::FLAG_SIX);
	}
	else if(pData->m_BestTime != 0) // tee has already finished?
	{
		Server()->StopRecord(ClientId);

		if(Diff <= 0.005f)
		{
			GameServer()->SendChatTarget(ClientId,
				"You finished with your best time.");
		}
		else
		{
			if(Diff >= 60)
				str_format(aBuf, sizeof(aBuf), "%d minute(s) %5.2f second(s) worse, better luck next time.",
					(int)Diff / 60, Diff - ((int)Diff / 60 * 60));
			else
				str_format(aBuf, sizeof(aBuf),
					"%5.2f second(s) worse, better luck next time.",
					Diff);
			GameServer()->SendChatTarget(ClientId, aBuf, CGameContext::FLAG_SIX); // this is private, sent only to the tee
		}
	}
	else
	{
		pData->m_RecordStopTick = Server()->Tick() + Server()->TickSpeed();
		pData->m_RecordFinishTime = Time;
	}

	if(!Server()->IsSixup(ClientId))
	{
		CNetMsg_Sv_DDRaceTime Msg;
		CNetMsg_Sv_DDRaceTimeLegacy MsgLegacy;
		MsgLegacy.m_Time = Msg.m_Time = (int)(Time * 100.0f);
		MsgLegacy.m_Check = Msg.m_Check = 0;
		MsgLegacy.m_Finish = Msg.m_Finish = 1;

		if(pData->m_BestTime)
		{
			float Diff100 = (Time - pData->m_BestTime) * 100;
			MsgLegacy.m_Check = Msg.m_Check = (int)Diff100;
		}
		if(VERSION_DDRACE <= Player->GetClientVersion())
		{
			if(Player->GetClientVersion() < VERSION_DDNET_MSG_LEGACY)
			{
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
			}
			else
			{
				Server()->SendPackMsg(&MsgLegacy, MSGFLAG_VITAL, ClientId);
			}
		}
	}

	CNetMsg_Sv_RaceFinish RaceFinishMsg;
	RaceFinishMsg.m_ClientId = ClientId;
	RaceFinishMsg.m_Time = Time * 1000;
	RaceFinishMsg.m_Diff = 0;
	if(pData->m_BestTime)
	{
		RaceFinishMsg.m_Diff = Diff * 1000 * (Time < pData->m_BestTime ? -1 : 1);
	}
	RaceFinishMsg.m_RecordPersonal = (Time < pData->m_BestTime || !pData->m_BestTime);
	RaceFinishMsg.m_RecordServer = Time < GameServer()->m_pController->m_CurrentRecord;
	Server()->SendPackMsg(&RaceFinishMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

	bool CallSaveScore = g_Config.m_SvSaveWorseScores;
	bool NeedToSendNewPersonalRecord = false;
	if(!pData->m_BestTime || Time < pData->m_BestTime)
	{
		// update the score
		pData->Set(Time, GetCurrentTimeCp(Player));
		CallSaveScore = true;
		NeedToSendNewPersonalRecord = true;
	}

	if(CallSaveScore)
		if(g_Config.m_SvNamelessScore || !str_startswith(Server()->ClientName(ClientId), "nameless tee"))
			GameServer()->Score()->SaveScore(ClientId, TimeTicks, pTimestamp,
				GetCurrentTimeCp(Player), Player->m_NotEligibleForFinish);

	bool NeedToSendNewServerRecord = false;
	// update server best time
	if(GameServer()->m_pController->m_CurrentRecord == 0)
	{
		GameServer()->Score()->LoadBestTime();
	}
	else if(Time < GameServer()->m_pController->m_CurrentRecord)
	{
		// check for nameless
		if(g_Config.m_SvNamelessScore || !str_startswith(Server()->ClientName(ClientId), "nameless tee"))
		{
			GameServer()->m_pController->m_CurrentRecord = Time;
			NeedToSendNewServerRecord = true;
		}
	}

	SetDDRaceState(Player, DDRACE_FINISHED);
	if(NeedToSendNewServerRecord)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetClientVersion() >= VERSION_DDRACE)
			{
				GameServer()->SendRecord(i);
			}
		}
	}
	if(!NeedToSendNewServerRecord && NeedToSendNewPersonalRecord && Player->GetClientVersion() >= VERSION_DDRACE)
	{
		GameServer()->SendRecord(ClientId);
	}

	int TTime = (int)Time;
	if(!Player->m_Score.has_value() || TTime < Player->m_Score.value())
	{
		Player->m_Score = TTime;
	}

	// Confetti
	CCharacter *pChar = Player->GetCharacter();
	m_pGameContext->CreateFinishEffect(pChar->m_Pos, pChar->TeamMask());
}

void CGameTeams::RequestTeamSwap(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team)
{
	if(!pPlayer || !pTargetPlayer)
		return;

	char aBuf[512];
	if(pPlayer->m_SwapTargetsClientId == pTargetPlayer->GetCid())
	{
		str_format(aBuf, sizeof(aBuf),
			"You have already requested to swap with %s.", Server()->ClientName(pTargetPlayer->GetCid()));

		GameServer()->SendChatTarget(pPlayer->GetCid(), aBuf);
		return;
	}

	// Notification for the swap initiator
	str_format(aBuf, sizeof(aBuf),
		"You have requested to swap with %s.",
		Server()->ClientName(pTargetPlayer->GetCid()));
	GameServer()->SendChatTarget(pPlayer->GetCid(), aBuf);

	// Notification to the target swap player
	str_format(aBuf, sizeof(aBuf),
		"%s has requested to swap with you. To complete the swap process please wait %d seconds and then type /swap %s.",
		Server()->ClientName(pPlayer->GetCid()), g_Config.m_SvSaveSwapGamesDelay, Server()->ClientName(pPlayer->GetCid()));
	GameServer()->SendChatTarget(pTargetPlayer->GetCid(), aBuf);

	// Notification for the remaining team
	str_format(aBuf, sizeof(aBuf),
		"%s has requested to swap with %s.",
		Server()->ClientName(pPlayer->GetCid()), Server()->ClientName(pTargetPlayer->GetCid()));
	// Do not send the team notification for team 0
	if(Team != 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Core.Team(i) == Team && i != pTargetPlayer->GetCid() && i != pPlayer->GetCid())
			{
				GameServer()->SendChatTarget(i, aBuf);
			}
		}
	}

	pPlayer->m_SwapTargetsClientId = pTargetPlayer->GetCid();
	m_aLastSwap[pPlayer->GetCid()] = Server()->Tick();
}

void CGameTeams::SwapTeamCharacters(CPlayer *pPrimaryPlayer, CPlayer *pTargetPlayer, int Team)
{
	if(!pPrimaryPlayer || !pTargetPlayer)
		return;

	char aBuf[128];

	int Since = (Server()->Tick() - m_aLastSwap[pTargetPlayer->GetCid()]) / Server()->TickSpeed();
	if(Since < g_Config.m_SvSaveSwapGamesDelay)
	{
		str_format(aBuf, sizeof(aBuf),
			"You have to wait %d seconds until you can swap.",
			g_Config.m_SvSaveSwapGamesDelay - Since);

		GameServer()->SendChatTarget(pPrimaryPlayer->GetCid(), aBuf);

		return;
	}

	pPrimaryPlayer->m_SwapTargetsClientId = -1;
	pTargetPlayer->m_SwapTargetsClientId = -1;

	int TimeoutAfterDelay = g_Config.m_SvSaveSwapGamesDelay + g_Config.m_SvSwapTimeout;
	if(Since >= TimeoutAfterDelay)
	{
		str_format(aBuf, sizeof(aBuf),
			"Your swap request timed out %d seconds ago. Use /swap again to re-initiate it.",
			Since - g_Config.m_SvSwapTimeout);

		GameServer()->SendChatTarget(pPrimaryPlayer->GetCid(), aBuf);

		return;
	}

	CSaveTee PrimarySavedTee;
	PrimarySavedTee.Save(pPrimaryPlayer->GetCharacter());

	CSaveTee SecondarySavedTee;
	SecondarySavedTee.Save(pTargetPlayer->GetCharacter());

	PrimarySavedTee.Load(pTargetPlayer->GetCharacter(), Team, true);
	SecondarySavedTee.Load(pPrimaryPlayer->GetCharacter(), Team, true);

	if(Team >= 1 && !m_aTeamFlock[Team])
	{
		for(const auto &pPlayer : GameServer()->m_apPlayers)
		{
			CCharacter *pChar = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(pChar && pChar->Team() == Team && pChar != pPrimaryPlayer->GetCharacter() && pChar != pTargetPlayer->GetCharacter())
				pChar->m_StartTime = pPrimaryPlayer->GetCharacter()->m_StartTime;
		}
	}
	std::swap(m_aTeeStarted[pPrimaryPlayer->GetCid()], m_aTeeStarted[pTargetPlayer->GetCid()]);
	std::swap(m_aTeeFinished[pPrimaryPlayer->GetCid()], m_aTeeFinished[pTargetPlayer->GetCid()]);
	std::swap(pPrimaryPlayer->GetCharacter()->GetLastRescueTeeRef(RESCUEMODE_AUTO), pTargetPlayer->GetCharacter()->GetLastRescueTeeRef(RESCUEMODE_AUTO));
	std::swap(pPrimaryPlayer->GetCharacter()->GetLastRescueTeeRef(RESCUEMODE_MANUAL), pTargetPlayer->GetCharacter()->GetLastRescueTeeRef(RESCUEMODE_MANUAL));

	GameServer()->m_World.SwapClients(pPrimaryPlayer->GetCid(), pTargetPlayer->GetCid());

	if(GameServer()->TeeHistorianActive())
	{
		GameServer()->TeeHistorian()->RecordPlayerSwap(pPrimaryPlayer->GetCid(), pTargetPlayer->GetCid());
	}

	str_format(aBuf, sizeof(aBuf),
		"%s has swapped with %s.",
		Server()->ClientName(pPrimaryPlayer->GetCid()), Server()->ClientName(pTargetPlayer->GetCid()));

	GameServer()->SendChatTeam(Team, aBuf);
}

void CGameTeams::ProcessSaveTeam()
{
	for(int Team = 0; Team < NUM_DDRACE_TEAMS; Team++)
	{
		if(m_apSaveTeamResult[Team] == nullptr || !m_apSaveTeamResult[Team]->m_Completed)
			continue;
		if(m_apSaveTeamResult[Team]->m_aBroadcast[0] != '\0')
			GameServer()->SendBroadcast(m_apSaveTeamResult[Team]->m_aBroadcast, -1);
		if(m_apSaveTeamResult[Team]->m_aMessage[0] != '\0' && m_apSaveTeamResult[Team]->m_Status != CScoreSaveResult::LOAD_FAILED)
			GameServer()->SendChatTeam(Team, m_apSaveTeamResult[Team]->m_aMessage);
		switch(m_apSaveTeamResult[Team]->m_Status)
		{
		case CScoreSaveResult::SAVE_SUCCESS:
		{
			if(GameServer()->TeeHistorianActive())
			{
				GameServer()->TeeHistorian()->RecordTeamSaveSuccess(
					Team,
					m_apSaveTeamResult[Team]->m_SaveId,
					m_apSaveTeamResult[Team]->m_SavedTeam.GetString());
			}
			for(int i = 0; i < m_apSaveTeamResult[Team]->m_SavedTeam.GetMembersCount(); i++)
			{
				if(m_apSaveTeamResult[Team]->m_SavedTeam.m_pSavedTees->IsHooking())
				{
					int ClientId = m_apSaveTeamResult[Team]->m_SavedTeam.m_pSavedTees->GetClientId();
					if(GameServer()->m_apPlayers[ClientId] != nullptr)
						GameServer()->SendChatTarget(ClientId, "Start holding the hook before loading the savegame to keep the hook");
				}
			}
			ResetSavedTeam(m_apSaveTeamResult[Team]->m_RequestingPlayer, Team);
			char aSaveId[UUID_MAXSTRSIZE];
			FormatUuid(m_apSaveTeamResult[Team]->m_SaveId, aSaveId, UUID_MAXSTRSIZE);
			dbg_msg("save", "Save successful: %s", aSaveId);
			break;
		}
		case CScoreSaveResult::SAVE_FAILED:
			if(GameServer()->TeeHistorianActive())
				GameServer()->TeeHistorian()->RecordTeamSaveFailure(Team);
			if(Count(Team) > 0)
			{
				// load weak/strong order to prevent switching weak/strong while saving
				m_apSaveTeamResult[Team]->m_SavedTeam.Load(GameServer(), Team, false);
			}
			break;
		case CScoreSaveResult::LOAD_SUCCESS:
		{
			if(GameServer()->TeeHistorianActive())
			{
				GameServer()->TeeHistorian()->RecordTeamLoadSuccess(
					Team,
					m_apSaveTeamResult[Team]->m_SaveId,
					m_apSaveTeamResult[Team]->m_SavedTeam.GetString());
			}

			bool TeamValid = false;
			if(Count(Team) > 0)
			{
				// keep current weak/strong order as on some maps there is no other way of switching
				TeamValid = m_apSaveTeamResult[Team]->m_SavedTeam.Load(GameServer(), Team, true);
			}

			if(!TeamValid)
			{
				GameServer()->SendChatTeam(Team, "Your team has been killed because it contains an invalid tee state");
				KillTeam(Team, -1, -1);
			}

			char aSaveId[UUID_MAXSTRSIZE];
			FormatUuid(m_apSaveTeamResult[Team]->m_SaveId, aSaveId, UUID_MAXSTRSIZE);
			dbg_msg("save", "Load successful: %s", aSaveId);
			break;
		}
		case CScoreSaveResult::LOAD_FAILED:
			if(GameServer()->TeeHistorianActive())
				GameServer()->TeeHistorian()->RecordTeamLoadFailure(Team);
			if(m_apSaveTeamResult[Team]->m_aMessage[0] != '\0')
				GameServer()->SendChatTarget(m_apSaveTeamResult[Team]->m_RequestingPlayer, m_apSaveTeamResult[Team]->m_aMessage);
			break;
		}
		m_apSaveTeamResult[Team] = nullptr;
	}
}

void CGameTeams::OnCharacterSpawn(int ClientId)
{
	m_Core.SetSolo(ClientId, false);
	int Team = m_Core.Team(ClientId);

	if(GetSaving(Team))
		return;

	if(m_Core.Team(ClientId) >= TEAM_SUPER || !m_aTeamLocked[Team])
	{
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO)
			SetForceCharacterTeam(ClientId, TEAM_FLOCK);
		else
			SetForceCharacterTeam(ClientId, ClientId); // initialize team
		if(!m_aTeamFlock[Team])
			CheckTeamFinished(Team);
	}
}

void CGameTeams::OnCharacterDeath(int ClientId, int Weapon)
{
	m_Core.SetSolo(ClientId, false);

	int Team = m_Core.Team(ClientId);
	if(GetSaving(Team))
		return;
	bool Locked = TeamLocked(Team) && Weapon != WEAPON_GAME;

	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO && Team != TEAM_SUPER)
	{
		ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);
		if(m_aPractice[Team])
		{
			if(Weapon != WEAPON_WORLD)
			{
				ResetRoundState(Team);
			}
			else
			{
				GameServer()->SendChatTeam(Team, "You died, but will stay in practice until you use kill.");
			}
		}
		else
		{
			ResetRoundState(Team);
		}
	}
	else if(Locked)
	{
		SetForceCharacterTeam(ClientId, Team);

		if(GetTeamState(Team) != TEAMSTATE_OPEN && !m_aTeamFlock[m_Core.Team(ClientId)])
		{
			ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);

			m_aPractice[Team] = false;

			if(Count(Team) > 1)
			{
				// Disband team if the team has more players than allowed.
				if(Count(Team) > g_Config.m_SvMaxTeamSize)
				{
					GameServer()->SendChatTeam(Team, "This team was disbanded because there are more players than allowed in the team.");
					SetTeamLock(Team, false);
					KillTeam(Team, Weapon == WEAPON_SELF ? ClientId : -1, ClientId);
					return;
				}

				KillTeam(Team, Weapon == WEAPON_SELF ? ClientId : -1, ClientId);

				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "Everyone in your locked team was killed because '%s' %s.", Server()->ClientName(ClientId), Weapon == WEAPON_SELF ? "killed" : "died");

				GameServer()->SendChatTeam(Team, aBuf);
			}
		}
	}
	else
	{
		if(m_aTeamState[m_Core.Team(ClientId)] == CGameTeams::TEAMSTATE_STARTED && !m_aTeeStarted[ClientId] && !m_aTeamFlock[m_Core.Team(ClientId)])
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "This team cannot finish anymore because '%s' left the team before hitting the start", Server()->ClientName(ClientId));
			GameServer()->SendChatTeam(Team, aBuf);
			GameServer()->SendChatTeam(Team, "Enter /practice mode or restart to avoid the entire team being killed in 60 seconds");

			m_aTeamUnfinishableKillTick[Team] = Server()->Tick() + 60 * Server()->TickSpeed();
			ChangeTeamState(Team, CGameTeams::TEAMSTATE_STARTED_UNFINISHABLE);
		}
		SetForceCharacterTeam(ClientId, TEAM_FLOCK);
		if(!m_aTeamFlock[m_Core.Team(ClientId)])
			CheckTeamFinished(Team);
	}
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
		m_aTeamLocked[Team] = Lock;
}

void CGameTeams::SetTeamFlock(int Team, bool Mode)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
		m_aTeamFlock[Team] = Mode;
}

void CGameTeams::ResetInvited(int Team)
{
	m_aInvited[Team].reset();
}

void CGameTeams::SetClientInvited(int Team, int ClientId, bool Invited)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		if(Invited)
			m_aInvited[Team].set(ClientId);
		else
			m_aInvited[Team].reset(ClientId);
	}
}

void CGameTeams::KillSavedTeam(int ClientId, int Team)
{
	if(g_Config.m_SvSoloServer || !g_Config.m_SvTeam)
	{
		GameServer()->m_apPlayers[ClientId]->KillCharacter(WEAPON_SELF, true);
	}
	else
	{
		KillTeam(Team, -1);
	}
}

void CGameTeams::ResetSavedTeam(int ClientId, int Team)
{
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);
		ResetRoundState(Team);
	}
	else
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
			{
				SetForceCharacterTeam(i, TEAM_FLOCK);
			}
		}
	}
}

int CGameTeams::GetFirstEmptyTeam() const
{
	for(int i = 1; i < MAX_CLIENTS; i++)
		if(m_aTeamState[i] == TEAMSTATE_EMPTY)
			return i;
	return -1;
}
