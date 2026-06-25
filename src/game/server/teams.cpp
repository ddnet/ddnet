/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"

#include "gamecontroller.h"
#include "player.h"
#include "score.h"
#include "teehistorian.h"

#include <base/dbg.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/interactions.h>
#include <game/team_state.h>

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
		m_aTeamState[i] = ETeamState::EMPTY;
		m_aTeamLocked[i] = false;
		m_aTeamFlock[i] = false;
		m_apSaveTeamResult[i] = nullptr;
		m_aTeamSentStartWarning[i] = false;
		if(m_pGameContext->PracticeByDefault())
			m_aPractice[i] = true;
		ResetRoundState(i);
		ResetRelayState(i);
		// yirou: initialize relay duration with default 5 seconds
		m_aTeamRelayDurationTicks[i] = 5 * Server()->TickSpeed();
	}
}

void CGameTeams::ResetRoundState(int Team)
{
	ResetInvited(Team);
	if(Team != TEAM_SUPER)
		ResetSwitchers(Team);

	if(!m_pGameContext->PracticeByDefault())
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
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO && pStartingChar->m_DDRaceState == ERaceState::STARTED)
		return;
	if((g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO || (m_Core.Team(ClientId) != TEAM_FLOCK && !m_aTeamFlock[m_Core.Team(ClientId)])) && pStartingChar->m_DDRaceState == ERaceState::FINISHED)
		return;
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO &&
		(m_Core.Team(ClientId) == TEAM_FLOCK || TeamFlock(m_Core.Team(ClientId)) || m_Core.Team(ClientId) == TEAM_SUPER))
	{
		if(TeamFlock(m_Core.Team(ClientId)) && (m_aTeamState[m_Core.Team(ClientId)] < ETeamState::STARTED))
			ChangeTeamState(m_Core.Team(ClientId), ETeamState::STARTED);

		m_aTeeStarted[ClientId] = true;
		pStartingChar->m_DDRaceState = ERaceState::STARTED;
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
		if(GetDDRaceState(pPlayer) != ERaceState::FINISHED)
			continue;

		Waiting = true;
		pStartingChar->m_DDRaceState = ERaceState::NONE;

		if(m_aLastChat[ClientId] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s has finished and didn't go through start yet, wait for them or join another team.",
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

	if(m_aTeamState[m_Core.Team(ClientId)] < ETeamState::STARTED && !Waiting)
	{
		ChangeTeamState(m_Core.Team(ClientId), ETeamState::STARTED);
		m_aTeamSentStartWarning[m_Core.Team(ClientId)] = false;
		m_aTeamUnfinishableKillTick[m_Core.Team(ClientId)] = -1;

		int NumPlayers = TeamSize(m_Core.Team(ClientId));

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
					SetDDRaceState(pPlayer, ERaceState::STARTED);
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
	int Team = m_Core.Team(ClientId);
	if(IsRelayActive(Team))
	{
		FinishRelayTeam(Team, ClientId);
		return;
	}

	if(((Team == TEAM_FLOCK || m_aTeamFlock[Team]) && g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO) || Team == TEAM_SUPER)
	{
		CPlayer *pPlayer = GetPlayer(ClientId);
		if(pPlayer && pPlayer->IsPlaying())
		{
			int TimeTicks = Server()->Tick() - GetStartTime(pPlayer);
			if(TimeTicks <= 0)
				return;
			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE); // 2019-04-02 19:41:58

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

	for(int i = 0; i < TEAM_SUPER; i++)
	{
		if(m_aTeamUnfinishableKillTick[i] == -1 || m_aTeamState[i] != ETeamState::STARTED_UNFINISHABLE)
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
		if(!pChar || m_aTeamState[Team] != ETeamState::STARTED || m_aTeamFlock[Team] || m_aTeeStarted[i] || m_aPractice[m_Core.Team(i)])
		{
			continue;
		}
		if((Now - pChar->m_StartTime) % Frequency == Remainder)
		{
			TeamHasWantedStartTime |= ((uint64_t)1) << m_Core.Team(i);
		}
	}
	TeamHasWantedStartTime &= ~(uint64_t)1;

	for(int i = 0; i < NUM_DDRACE_TEAMS; i++)
		OnRelayTick(i);

	if(!TeamHasWantedStartTime)
	{
		return;
	}
	//for(int i = 0; i < MAX_CLIENTS; i++)
	//{
	//	if(((TeamHasWantedStartTime >> i) & 1) == 0)
	//	{
	//		continue;
	//	}
	//	if(TeamSize(i) <= 1)
	//	{
	//		continue;
	//	}
	//	bool TeamHasCheatCharacter = false;
	//	int NumPlayersNotStarted = 0;
	//	char aPlayerNames[256];
	//	aPlayerNames[0] = 0;
	//	for(int j = 0; j < MAX_CLIENTS; j++)
	//	{
	//		if(Character(j) && Character(j)->m_DDRaceState == ERaceState::CHEATED)
	//			TeamHasCheatCharacter = true;
	//		if(m_Core.Team(j) == i && !m_aTeeStarted[j])
	//		{
	//			if(aPlayerNames[0])
	//			{
	//				str_append(aPlayerNames, ", ");
	//			}
	//			str_append(aPlayerNames, Server()->ClientName(j));
	//			NumPlayersNotStarted += 1;
	//		}
	//	}
	//	if(!aPlayerNames[0] || TeamHasCheatCharacter)
	//	{
	//		continue;
	//	}
	//	char aBuf[512];
	//	str_format(aBuf, sizeof(aBuf),
	//		"Your team has %d %s not started yet, they need "
	//		"to touch the start before this team can finish: %s",
	//		NumPlayersNotStarted,
	//		NumPlayersNotStarted == 1 ? "player that has" : "players that have",
	//		aPlayerNames);
	//	GameServer()->SendChatTeam(i, aBuf);
	//}
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
				ChangeTeamState(Team, ETeamState::FINISHED);

				const int Minutes = (int)Time / 60;
				const float Seconds = Time - (Minutes * 60.0f);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf),
					"Your team would've finished in: %d minute(s) %5.2f second(s). Since you had practice mode enabled your rank doesn't count.",
					Minutes, Seconds);
				GameServer()->SendChatTeam(Team, aBuf);

				for(unsigned int i = 0; i < PlayersCount; ++i)
				{
					SetDDRaceState(apTeamPlayers[i], ERaceState::FINISHED);
				}

				return;
			}

			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE); // 2019-04-02 19:41:58

			for(unsigned int i = 0; i < PlayersCount; ++i)
				OnFinish(apTeamPlayers[i], TimeTicks, aTimestamp);
			ChangeTeamState(Team, ETeamState::FINISHED); // TODO: Make it better
			OnTeamFinish(Team, apTeamPlayers, PlayersCount, TimeTicks, aTimestamp);
		}
	}
}

bool CGameTeams::CanJoinTeam(int ClientId, int Team, char *pError, int ErrorSize) const
{
	int CurrentTeam = m_Core.Team(ClientId);

	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
	{
		str_format(pError, ErrorSize, "Invalid client ID: %d", ClientId);
		return false;
	}
	if(!IsValidTeamNumber(Team) && Team != TEAM_SUPER)
	{
		str_format(pError, ErrorSize, "Invalid team number: %d", Team);
		return false;
	}
	if(Team != TEAM_SUPER && m_aTeamState[Team] > ETeamState::OPEN && !m_aPractice[Team] && !m_aTeamFlock[Team])
	{
		str_copy(pError, "This team started already", ErrorSize);
		return false;
	}
	if(CurrentTeam == Team)
	{
		str_copy(pError, "You are in this team already", ErrorSize);
		return false;
	}
	if(!Character(ClientId))
	{
		str_copy(pError, "You can't change teams while you are dead/a spectator.", ErrorSize);
		return false;
	}
	if(Team == TEAM_SUPER && !Character(ClientId)->IsSuper())
	{
		str_copy(pError, "You can't join super team if you don't have super rights", ErrorSize);
		return false;
	}
	if(Team != TEAM_SUPER && Character(ClientId)->m_DDRaceState != ERaceState::NONE && (m_aTeamState[CurrentTeam] < ETeamState::FINISHED || Team != 0))
	{
		str_copy(pError, "You have started racing already", ErrorSize);
		return false;
	}
	// No cheating through noob filter with practice and then leaving team
	if(m_aPractice[CurrentTeam] && !m_pGameContext->PracticeByDefault())
	{
		str_copy(pError, "You have used practice mode already", ErrorSize);
		return false;
	}

	// you can not join a team which is currently in the process of saving,
	// because the save-process can fail and then the team is reset into the game
	if(Team != TEAM_SUPER && GetSaving(Team))
	{
		str_copy(pError, "This team is currently saving", ErrorSize);
		return false;
	}
	if(CurrentTeam != TEAM_SUPER && GetSaving(CurrentTeam))
	{
		str_copy(pError, "Your team is currently saving", ErrorSize);
		return false;
	}

	return true;
}

bool CGameTeams::SetCharacterTeam(int ClientId, int Team, char *pError, int ErrorSize)
{
	if(!CanJoinTeam(ClientId, Team, pError, ErrorSize))
		return false;

	SetForceCharacterTeam(ClientId, Team);
	return true;
}

void CGameTeams::SetForceCharacterTeam(int ClientId, int Team)
{
	m_aTeeStarted[ClientId] = false;
	m_aTeeFinished[ClientId] = false;
	int OldTeam = m_Core.Team(ClientId);

	//yirou: remove from old team's relay order when leaving
	if(Team != OldTeam && IsValidTeamNumber(OldTeam))
	{
		RemoveRelayRunner(OldTeam, ClientId);
	}

	if(Team != OldTeam && (OldTeam != TEAM_FLOCK || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO) && OldTeam != TEAM_SUPER && m_aTeamState[OldTeam] != ETeamState::EMPTY)
	{
		bool NoElseInOldTeam = TeamSize(OldTeam) <= 1;
		if(NoElseInOldTeam)
		{
			m_aTeamState[OldTeam] = ETeamState::EMPTY;

			// unlock team when last player leaves
			SetTeamLock(OldTeam, false);
			SetTeamFlock(OldTeam, false);
			ResetRoundState(OldTeam);
			// do not reset SaveTeamResult, because it should be logged into teehistorian even if the team leaves
		}
	}

	m_Core.Team(ClientId, Team);

	//yirou: auto assign relay order when joining a new team
	if(Team != OldTeam && IsValidTeamNumber(Team) && Team > TEAM_FLOCK)
	{
		AutoAssignRelayOrder(Team, ClientId);
	}

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

	if(Team != TEAM_SUPER && (m_aTeamState[Team] == ETeamState::EMPTY || (m_aTeamLocked[Team] && !m_aTeamFlock[Team])))
	{
		if(!m_aTeamLocked[Team])
			ChangeTeamState(Team, ETeamState::OPEN);

		ResetSwitchers(Team);
	}
}

int CGameTeams::TeamSize(int Team) const
{
	if(Team == TEAM_SUPER)
		return -1;

	int Count = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team)
			Count++;

	return Count;
}

void CGameTeams::ChangeTeamState(int Team, ETeamState State)
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
	if(m_aTeamState[Team] != ETeamState::STARTED)
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

	CPlayer *pAsker = GetPlayer(Asker);
	CInteractions Interact;
	Interact.Init(Asker, pAsker ? pAsker->GetUniqueCid() : 0);
	Interact.FillOwnerConnected(
		pAsker && pAsker->GetCharacter() && pAsker->GetCharacter()->IsAlive(),
		m_Core.Team(Asker),
		m_Core.GetSolo(Asker),
		false,
		false); // TODO: these false values make little sense

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
		if(!Interact.CanSee(GameServer(), i))
			continue;

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

ERaceState CGameTeams::GetDDRaceState(const CPlayer *Player) const
{
	if(!Player)
		return ERaceState::NONE;

	const CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_DDRaceState;
	return ERaceState::NONE;
}

void CGameTeams::SetDDRaceState(CPlayer *Player, ERaceState DDRaceState)
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
		return nullptr;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_aCurrentTimeCp;
	return nullptr;
}

void CGameTeams::OnTeamFinish(int Team, CPlayer **Players, unsigned int Size, int TimeTicks, const char *pTimestamp)
{
	int aPlayerCids[MAX_CLIENTS];

	for(unsigned int i = 0; i < Size; i++)
	{
		aPlayerCids[i] = Players[i]->GetCid();

		if(g_Config.m_SvRejoinTeam0 && g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && (!IsValidTeamNumber(m_Core.Team(Players[i]->GetCid())) || !m_aTeamLocked[m_Core.Team(Players[i]->GetCid())]))
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

void CGameTeams::OnFinish(CPlayer *pPlayer, int TimeTicks, const char *pTimestamp)
{
	if(!pPlayer || !pPlayer->IsPlaying())
		return;

	float Time = TimeTicks / (float)Server()->TickSpeed();

	// TODO:DDRace:btd: this ugly
	const int ClientId = pPlayer->GetCid();
	CPlayerData *pData = GameServer()->Score()->PlayerData(ClientId);

	char aBuf[128];
	SetLastTimeCp(pPlayer, -1);
	// Note that the "finished in" message is parsed by the client
	str_format(aBuf, sizeof(aBuf),
		"%s finished in: %d minute(s) %5.2f second(s)",
		Server()->ClientName(ClientId), (int)Time / 60,
		Time - ((int)Time / 60 * 60));
	if(g_Config.m_SvHideScore)
		GameServer()->SendChatTarget(ClientId, aBuf, CGameContext::FLAG_SIX);
	else
		GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1., CGameContext::FLAG_SIX);

	float Diff = absolute(Time - pData->m_BestTime.value_or(0.0f));

	if(Time - pData->m_BestTime.value_or(0.0f) < 0)
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
		if(g_Config.m_SvHideScore)
			GameServer()->SendChatTarget(ClientId, aBuf, CGameContext::FLAG_SIX);
		else
			GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1, CGameContext::FLAG_SIX);
	}
	else if(pData->m_BestTime.has_value()) // tee has already finished?
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

	GameServer()->SendFinish(ClientId, Time, pData->m_BestTime);
	bool CallSaveScore = g_Config.m_SvSaveWorseScores;
	bool NeedToSendNewPersonalRecord = false;
	if(!pData->m_BestTime || Time < pData->m_BestTime)
	{
		// update the score
		pData->Set(Time, GetCurrentTimeCp(pPlayer));
		CallSaveScore = true;
		NeedToSendNewPersonalRecord = true;
	}

	if(CallSaveScore)
		if(g_Config.m_SvNamelessScore || !str_startswith(Server()->ClientName(ClientId), "nameless tee"))
			GameServer()->Score()->SaveScore(ClientId, TimeTicks, pTimestamp,
				GetCurrentTimeCp(pPlayer), pPlayer->m_NotEligibleForFinish);

	bool NeedToSendNewServerRecord = false;
	// update server best time
	if(!GameServer()->m_pController->m_CurrentRecord.has_value())
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

	SetDDRaceState(pPlayer, ERaceState::FINISHED);
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
	if(!NeedToSendNewServerRecord && NeedToSendNewPersonalRecord && pPlayer->GetClientVersion() >= VERSION_DDRACE)
	{
		GameServer()->SendRecord(ClientId);
	}

	int TTime = (int)Time;
	std::optional<float> Score = GameServer()->Score()->PlayerData(ClientId)->m_BestTime;
	if(!Score.has_value() || TTime < Score.value())
	{
		Server()->SetClientScore(ClientId, TTime);
	}

	// Confetti
	CCharacter *pChar = pPlayer->GetCharacter();
	m_pGameContext->CreateFinishEffect(pChar->m_Pos, pChar->TeamMask());
}

CCharacter *CGameTeams::Character(int ClientId)
{
	return GameServer()->GetPlayerChar(ClientId);
}

const CCharacter *CGameTeams::Character(int ClientId) const
{
	return GameServer()->GetPlayerChar(ClientId);
}

CPlayer *CGameTeams::GetPlayer(int ClientId)
{
	return GameServer()->m_apPlayers[ClientId];
}

CGameContext *CGameTeams::GameServer()
{
	return m_pGameContext;
}

const CGameContext *CGameTeams::GameServer() const
{
	return m_pGameContext;
}

class IServer *CGameTeams::Server()
{
	return m_pGameContext->Server();
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
		"You have requested to swap with %s. Use /cancelswap to cancel the request.",
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

	PrimarySavedTee.Load(pTargetPlayer->GetCharacter());
	SecondarySavedTee.Load(pPrimaryPlayer->GetCharacter());

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

void CGameTeams::CancelTeamSwap(CPlayer *pPlayer, int Team)
{
	if(!pPlayer)
		return;

	char aBuf[128];

	// Notification for the swap initiator
	str_format(aBuf, sizeof(aBuf),
		"You have canceled swap with %s.",
		Server()->ClientName(pPlayer->m_SwapTargetsClientId));
	GameServer()->SendChatTarget(pPlayer->GetCid(), aBuf);

	// Notification to the target swap player
	str_format(aBuf, sizeof(aBuf),
		"%s has canceled swap with you.",
		Server()->ClientName(pPlayer->GetCid()));
	GameServer()->SendChatTarget(pPlayer->m_SwapTargetsClientId, aBuf);

	// Notification for the remaining team
	str_format(aBuf, sizeof(aBuf),
		"%s has canceled swap with %s.",
		Server()->ClientName(pPlayer->GetCid()), Server()->ClientName(pPlayer->m_SwapTargetsClientId));
	// Do not send the team notification for team 0
	if(Team != 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Core.Team(i) == Team && i != pPlayer->m_SwapTargetsClientId && i != pPlayer->GetCid())
			{
				GameServer()->SendChatTarget(i, aBuf);
			}
		}
	}

	pPlayer->m_SwapTargetsClientId = -1;
}

void CGameTeams::ProcessSaveTeam()
{
	for(int Team = 0; Team < NUM_DDRACE_TEAMS; Team++)
	{
		if(m_apSaveTeamResult[Team] == nullptr || !m_apSaveTeamResult[Team]->m_Completed)
			continue;

		int Size = m_apSaveTeamResult[Team]->m_SavedTeam.GetMembersCount();
		int State = -1;

		switch(m_apSaveTeamResult[Team]->m_Status)
		{
		case CScoreSaveResult::SAVE_FALLBACKFILE:
			State = SAVESTATE_FALLBACKFILE;
			break;
		case CScoreSaveResult::SAVE_WARNING:
			State = SAVESTATE_WARNING;
			break;
		case CScoreSaveResult::SAVE_SUCCESS:
			State = SAVESTATE_DONE;
			break;
		case CScoreSaveResult::SAVE_FAILED:
			State = SAVESTATE_ERROR;
			break;
		case CScoreSaveResult::LOAD_FAILED:
		case CScoreSaveResult::LOAD_SUCCESS:
			State = -1;
			break;
		}

		if(State != -1)
		{
			GameServer()->SendSaveCode(
				Team,
				Size,
				State,
				(State == SAVESTATE_DONE) ? "" : m_apSaveTeamResult[Team]->m_aMessage,
				m_apSaveTeamResult[Team]->m_aRequestingPlayer,
				m_apSaveTeamResult[Team]->m_aServer,
				m_apSaveTeamResult[Team]->m_aGeneratedCode,
				m_apSaveTeamResult[Team]->m_aCode);
		}

		if(m_apSaveTeamResult[Team]->m_aBroadcast[0] != '\0')
			GameServer()->SendBroadcast(m_apSaveTeamResult[Team]->m_aBroadcast, -1);

		switch(m_apSaveTeamResult[Team]->m_Status)
		{
		case CScoreSaveResult::SAVE_FALLBACKFILE:
		case CScoreSaveResult::SAVE_WARNING:
		case CScoreSaveResult::SAVE_SUCCESS:
		{
			if(GameServer()->TeeHistorianActive())
			{
				GameServer()->TeeHistorian()->RecordTeamSaveSuccess(
					Team,
					m_apSaveTeamResult[Team]->m_SaveId,
					m_apSaveTeamResult[Team]->m_SavedTeam.GetString());
			}
			for(int i = 0; i < Size; i++)
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
			if(TeamSize(Team) > 0)
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
			if(TeamSize(Team) > 0)
			{
				// keep current weak/strong order as on some maps there is no other way of switching
				TeamValid = m_apSaveTeamResult[Team]->m_SavedTeam.Load(GameServer(), Team, true);
			}

			if(!TeamValid)
			{
				GameServer()->SendChatTeam(Team, "Your team has been killed because it contains an invalid tee state");
				KillTeam(Team, -1);
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

	// yirou: if player should be in relay spec AND relay is active (RUNNING or COUNTDOWN), apply it now
	if((m_aTeamRelayState[Team] == RELAY_STATE_RUNNING || m_aTeamRelayState[Team] == RELAY_STATE_COUNTDOWN) && 
	   GameServer()->m_apPlayers[ClientId] && GameServer()->m_apPlayers[ClientId]->IsRelayForcedSpec())
	{
		GameServer()->m_apPlayers[ClientId]->ForceRelaySpec(true);
		// Teleport to record point
		CCharacter *pChr = GameServer()->m_apPlayers[ClientId]->GetCharacter();
		if(pChr)
		{
			GameServer()->Teleport_relay(pChr, m_aTeamRelayRecordPos[Team]);
			pChr->ResetJumps();
			pChr->SetDeepFrozen(false);
			pChr->Unfreeze();
			pChr->ResetVelocity();
		}
		return; // Skip normal team assignment
	}

	if(!IsValidTeamNumber(Team) || !m_aTeamLocked[Team])
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

	// yirou: if player disconnects (WEAPON_WORLD) during relay, reset the team
	if(Weapon == WEAPON_WORLD && IsRelayActive(Team))
	{
		// Remove from relay order first
		RemoveRelayRunner(Team, ClientId);
		// Reset the team
		KillCharacterOrTeam(ClientId, Team);
		// Reset relay state
		ResetRelayState(Team);
		// Unlock team
		SetTeamLock(Team, false);
		// Notify team
		GameServer()->SendChatTeam(Team, "Relay cancelled: player disconnected");
	}

	// yirou: if any player dies during relay (by any means including /kill or K key), pause relay
	if(IsRelayActive(Team))
	{
		// Pause relay - go to IDLE state (need /relaystart to resume)
		m_aTeamRelayState[Team] = RELAY_STATE_IDLE;
		
		// Exit all players from spec
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
			{
				GameServer()->m_apPlayers[i]->ForceRelaySpec(false);
			}
		}
		
		// Reset current runner index so next /relaystart starts from runner 1
		m_aTeamCurrentRunnerIndex[Team] = 0;
		m_aTeamRelayTickStart[Team] = 0;
		
		// Notify team
		GameServer()->SendChatTeam(Team, "Relay paused: player died. Use /relaystart to resume.");
	}

	if(GetSaving(Team))
		return;
	bool Locked = TeamLocked(Team) && Weapon != WEAPON_GAME;

	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO && Team != TEAM_SUPER)
	{
		ChangeTeamState(Team, ETeamState::OPEN);
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

		if(GetTeamState(Team) != ETeamState::OPEN && !m_aTeamFlock[m_Core.Team(ClientId)])
		{
			ChangeTeamState(Team, ETeamState::OPEN);

			if(!m_pGameContext->PracticeByDefault())
			{
				if(!g_Config.m_SvPauseable)
				{
					for(int ClientId1 = 0; ClientId1 < MAX_CLIENTS; ClientId1++)
					{
						if(m_Core.Team(ClientId1) == Team)
						{
							CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId1];
							if(pPlayer && pPlayer->IsPaused() == -1 * CPlayer::PAUSE_SPEC)
								pPlayer->Pause(CPlayer::PAUSE_PAUSED, true);
						}
					}
				}
				m_aPractice[Team] = false;
			}

			if(TeamSize(Team) > 1)
			{
				// Disband team if the team has more players than allowed.
				if(TeamSize(Team) > g_Config.m_SvMaxTeamSize)
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
		if(m_aTeamState[m_Core.Team(ClientId)] == ETeamState::STARTED && !m_aTeeStarted[ClientId] && !m_aTeamFlock[m_Core.Team(ClientId)])
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "This team cannot finish anymore because '%s' left the team before hitting the start", Server()->ClientName(ClientId));
			GameServer()->SendChatTeam(Team, aBuf);
			GameServer()->SendChatTeam(Team, "Enter /practice mode or restart to avoid the entire team being killed in 60 seconds");

			m_aTeamUnfinishableKillTick[Team] = Server()->Tick() + 60 * Server()->TickSpeed();
			ChangeTeamState(Team, ETeamState::STARTED_UNFINISHABLE);
		}
		SetForceCharacterTeam(ClientId, TEAM_FLOCK);
		if(!m_aTeamFlock[m_Core.Team(ClientId)])
			CheckTeamFinished(Team);
	}
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	if(Team != TEAM_FLOCK && IsValidTeamNumber(Team))
		m_aTeamLocked[Team] = Lock;
}

void CGameTeams::SetTeamFlock(int Team, bool Mode)
{
	if(Team != TEAM_FLOCK && IsValidTeamNumber(Team))
		m_aTeamFlock[Team] = Mode;
}

void CGameTeams::ResetInvited(int Team)
{
	m_aInvited[Team].reset();
}

void CGameTeams::SetClientInvited(int Team, int ClientId, bool Invited)
{
	if(Team != TEAM_FLOCK && IsValidTeamNumber(Team))
	{
		if(Invited)
			m_aInvited[Team].set(ClientId);
		else
			m_aInvited[Team].reset(ClientId);
	}
}

void CGameTeams::KillCharacterOrTeam(int ClientId, int Team)
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
		ChangeTeamState(Team, ETeamState::OPEN);
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

std::optional<int> CGameTeams::GetFirstEmptyTeam() const
{
	for(int i = 1; i < TEAM_SUPER; i++)
		if(m_aTeamState[i] == ETeamState::EMPTY)
			return i;
	return std::nullopt;
}

bool CGameTeams::TeeStarted(int ClientId) const
{
	return m_aTeeStarted[ClientId];
}

bool CGameTeams::TeeFinished(int ClientId) const
{
	return m_aTeeFinished[ClientId];
}

ETeamState CGameTeams::GetTeamState(int Team) const
{
	return m_aTeamState[Team];
}

bool CGameTeams::TeamLocked(int Team) const
{
	if(Team == TEAM_FLOCK || !IsValidTeamNumber(Team))
		return false;

	return m_aTeamLocked[Team];
}

bool CGameTeams::TeamFlock(int Team) const
{
	// this is for team0mode, TEAM_FLOCK is handled differently
	if(Team == TEAM_FLOCK || !IsValidTeamNumber(Team))
		return false;

	return m_aTeamFlock[Team];
}

bool CGameTeams::IsInvited(int Team, int ClientId) const
{
	return m_aInvited[Team].test(ClientId);
}

bool CGameTeams::IsStarted(int Team) const
{
	return m_aTeamState[Team] == ETeamState::STARTED;
}

void CGameTeams::SetStarted(int ClientId, bool Started)
{
	m_aTeeStarted[ClientId] = Started;
}

void CGameTeams::SetFinished(int ClientId, bool Finished)
{
	m_aTeeFinished[ClientId] = Finished;
}

void CGameTeams::SetSaving(int TeamId, std::shared_ptr<CScoreSaveResult> &SaveResult)
{
	m_apSaveTeamResult[TeamId] = SaveResult;
}

bool CGameTeams::GetSaving(int TeamId) const
{
	if(!IsValidTeamNumber(TeamId))
		return false;
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && TeamId == TEAM_FLOCK)
		return false;

	return m_apSaveTeamResult[TeamId] != nullptr;
}

void CGameTeams::SetPractice(int Team, bool Enabled)
{
	if(!IsValidTeamNumber(Team))
		return;
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team == TEAM_FLOCK)
	{
		// allow to enable practice in team 0, for practice by default
		if(!g_Config.m_SvTestingCommands)
			return;
	}

	m_aPractice[Team] = Enabled;
}

bool CGameTeams::IsPractice(int Team)
{
	if(!IsValidTeamNumber(Team))
		return false;
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team == TEAM_FLOCK)
	{
		if(GameServer()->PracticeByDefault())
			return true;

		return false;
	}

	return m_aPractice[Team];
}

bool CGameTeams::IsValidTeamNumber(int Team) const
{
	return Team >= TEAM_FLOCK && Team < NUM_DDRACE_TEAMS - 1; // no TEAM_SUPER
}
//yirou
// 设置队伍接力持续时间
void CGameTeams::SetTeamRelayDuration(int Team, int Ticks)
{
	if(Team >= 0 && Team < NUM_DDRACE_TEAMS)
		m_aTeamRelayDurationTicks[Team] = Ticks;
}

// 获取队伍接力持续时间
int CGameTeams::GetTeamRelayDuration(int Team) const
{
	if(Team >= 0 && Team < NUM_DDRACE_TEAMS)
		return m_aTeamRelayDurationTicks[Team];
	return 0;
}
//yirou
void CGameTeams::ResetRelayState(int Team)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return;
	m_aTeamRelayState[Team] = RELAY_STATE_IDLE;
	m_aTeamRelayRunnerCount[Team] = 0;
	m_aTeamCurrentRunnerIndex[Team] = 0;
	m_aTeamRelayTickStart[Team] = 0;
	// Do NOT reset m_aTeamRelayDurationTicks here, preserve user setting
	// m_aTeamRelayDurationTicks[Team] = 5 * Server()->TickSpeed();
	m_aTeamRelayCountdownEndTick[Team] = 0;
	m_aTeamRelayLastWarnedSecond[Team] = -1;
	m_aTeamRelayRaceTimerStarted[Team] = false; // yirou: reset race timer flag
	m_aTeamRelayRecordPos[Team] = vec2(0, 0);
	m_aTeamRelayPrevRecordPos[Team] = vec2(0, 0); // yirou: reset previous record point
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aTeamRelayOrder[Team][i] = -1;
		m_aTeamRelayPlayerFinished[Team][i] = false;
	}
}

bool CGameTeams::IsRelayTeam(int Team) const
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return false;
	// yirou: all valid teams are relay teams by default
	if(!IsValidTeamNumber(Team))
		return false;
	if(m_aTeamRelayState[Team] != RELAY_STATE_IDLE)
		return true;
	return true; // All valid teams are relay teams
}

bool CGameTeams::IsRelayActive(int Team) const
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return false;
	return m_aTeamRelayState[Team] == RELAY_STATE_RUNNING;
}

bool CGameTeams::IsRelayForcedSpec(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;
	if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_DDRaceState == ERaceState::FINISHED)
		return false;
	int Team = m_Core.Team(ClientId);
	if(!IsRelayActive(Team) && m_aTeamRelayState[Team] != RELAY_STATE_COUNTDOWN && m_aTeamRelayState[Team] != RELAY_STATE_RESETTING)
		return false;
	// any team member is forced spec while relay is active/countdown/resetting
	return true;
}

void CGameTeams::SetRelayRunner(int Team, int Order, int ClientId)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(Order < 1 || Order >= MAX_CLIENTS)
		return;
	if(!GameServer()->m_apPlayers[ClientId])
		return;
	if(m_Core.Team(ClientId) != Team)
		return;

	int OldOrder = -1;
	int TargetClientId = m_aTeamRelayOrder[Team][Order];

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamRelayOrder[Team][i] == ClientId)
		{
			OldOrder = i;
			break;
		}
	}

	if(OldOrder == Order)
		return;

	if(OldOrder != -1 && TargetClientId != -1 && TargetClientId != ClientId)
	{
		m_aTeamRelayOrder[Team][OldOrder] = TargetClientId;
		m_aTeamRelayOrder[Team][Order] = ClientId;
	}
	else
	{
		if(OldOrder != -1)
			m_aTeamRelayOrder[Team][OldOrder] = -1;
		m_aTeamRelayOrder[Team][Order] = ClientId;
	}

	m_aTeamRelayRunnerCount[Team] = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamRelayOrder[Team][i] != -1)
			m_aTeamRelayRunnerCount[Team]++;
	}
}

//yirou
void CGameTeams::RemoveRelayRunner(int Team, int ClientId)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;

	for(int i = 1; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamRelayOrder[Team][i] == ClientId)
		{
			m_aTeamRelayOrder[Team][i] = -1;
			break;
		}
	}

	m_aTeamRelayRunnerCount[Team] = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamRelayOrder[Team][i] != -1)
			m_aTeamRelayRunnerCount[Team]++;
	}
}

//yirou
void CGameTeams::AutoAssignRelayOrder(int Team, int ClientId)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(!GameServer()->m_apPlayers[ClientId])
		return;
	if(m_Core.Team(ClientId) != Team)
		return;

	// Check if already assigned
	for(int i = 1; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamRelayOrder[Team][i] == ClientId)
			return; // Already assigned
	}

	// Find first available slot
	for(int i = 1; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamRelayOrder[Team][i] == -1)
		{
			m_aTeamRelayOrder[Team][i] = ClientId;
			m_aTeamRelayRunnerCount[Team]++;
			break;
		}
	}
}

void CGameTeams::SendRelayOrder(int Team)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Relay order for team %d:", Team);
	bool First = true;

	for(int i = 1; i < MAX_CLIENTS; i++)
	{
		int ClientId = m_aTeamRelayOrder[Team][i];
		if(ClientId != -1)
		{
			if(First)
				First = false;
			else
				str_append(aBuf, ",");

			char aEntry[64];
			str_format(aEntry, sizeof(aEntry), " %d:%s", i, Server()->ClientName(ClientId));
			str_append(aBuf, aEntry);
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
			GameServer()->SendChatTarget(i, aBuf);
	}
}

void CGameTeams::StartRelay(int Team, vec2 RecordPos)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return;
	if(!IsRelayTeam(Team))
		return;

	int AnyMember = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			AnyMember = i;
			break;
		}
	}
	if(AnyMember == -1)
		return;

	m_aTeamRelayRecordPos[Team] = RecordPos;
	m_aTeamRelayState[Team] = RELAY_STATE_RESETTING;
	m_aTeamCurrentRunnerIndex[Team] = 0;
	m_aTeamRelayTickStart[Team] = 0;
	m_aTeamRelayCountdownEndTick[Team] = 0;
	m_aTeamRelayLastWarnedSecond[Team] = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aTeamRelayPlayerFinished[Team][i] = false;

	// yirou: lock team before starting relay
	SetTeamLock(Team, true);

	KillCharacterOrTeam(AnyMember, Team);
}

void CGameTeams::AdvanceRelayRunner(int Team, int NowTick)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return;
	if(m_aTeamRelayState[Team] != RELAY_STATE_RUNNING)
		return;

	// Use provided tick or get current
	int CurrentTick = (NowTick >= 0) ? NowTick : Server()->Tick();

	int OldRunnerIndex = m_aTeamCurrentRunnerIndex[Team];
	int OldRunnerId = -1;
	if(OldRunnerIndex >= 1 && OldRunnerIndex < MAX_CLIENTS)
		OldRunnerId = m_aTeamRelayOrder[Team][OldRunnerIndex];

	if(OldRunnerId != -1 && GameServer()->m_apPlayers[OldRunnerId])
	{
		CCharacter *pOldChr = GameServer()->m_apPlayers[OldRunnerId]->GetCharacter();
		if(pOldChr)
		{
			// yirou: save current record point as previous before updating
			m_aTeamRelayPrevRecordPos[Team] = m_aTeamRelayRecordPos[Team];
			m_aTeamRelayRecordPos[Team] = pOldChr->m_Pos;
			
			// Clear old runner's input and hook before going to spec
			pOldChr->ResetInput();
			pOldChr->ResetHook();
			
			GameServer()->m_apPlayers[OldRunnerId]->ForceRelaySpec(true);
		}
	}

	int StartIndex = m_aTeamCurrentRunnerIndex[Team];
	int NextRunnerId = -1;
	int NextRunnerIndex = 0;
	for(int Offset = 1; Offset < MAX_CLIENTS; Offset++)
	{
		int Idx = StartIndex + Offset;
		if(Idx >= MAX_CLIENTS)
			Idx -= MAX_CLIENTS;
		if(Idx == 0)
			continue;
		int Candidate = m_aTeamRelayOrder[Team][Idx];
		if(Candidate != -1 && GameServer()->m_apPlayers[Candidate] && GameServer()->m_apPlayers[Candidate]->GetCharacter())
		{
			NextRunnerId = Candidate;
			NextRunnerIndex = Idx;
			break;
		}
	}

	if(NextRunnerId == -1)
	{
		m_aTeamRelayTickStart[Team] = CurrentTick;
		return;
	}

	CPlayer *pNextPlayer = GameServer()->m_apPlayers[NextRunnerId];
	if(!pNextPlayer)
	{
		m_aTeamRelayTickStart[Team] = CurrentTick;
		return;
	}
	CCharacter *pNextChr = pNextPlayer->GetCharacter();
	if(!pNextChr)
	{
		m_aTeamRelayTickStart[Team] = CurrentTick;
		return;
	}

	pNextPlayer->ForceRelaySpec(false);
	
	// Clear new runner's input and hook before teleport
	pNextChr->ResetInput();
	pNextChr->ResetHook();
	
	GameServer()->Teleport_relay(pNextChr, m_aTeamRelayRecordPos[Team]);
	pNextChr->ResetJumps();
	pNextChr->SetDeepFrozen(false);
	pNextChr->Unfreeze();
	pNextChr->ResetVelocity();

	// Only start race timer for first runner (runner 1), don't reset on handoff
	if(!m_aTeamRelayRaceTimerStarted[Team] && pNextChr->m_DDRaceState != ERaceState::STARTED)
	{
		OnCharacterStart(NextRunnerId);
		m_aTeamRelayRaceTimerStarted[Team] = true;
	}
	// Note: Do NOT reset m_StartTime here - race timer continues across handoffs

	m_aTeamCurrentRunnerIndex[Team] = NextRunnerIndex;
	m_aTeamRelayTickStart[Team] = CurrentTick; // Relay timer resets for each runner

	// yirou: announce runner change to all team members and show next runner
	int AfterNextRunnerId = -1;
	for(int Offset = 1; Offset < MAX_CLIENTS; Offset++)
	{
		int Idx = NextRunnerIndex + Offset;
		if(Idx >= MAX_CLIENTS)
			Idx -= MAX_CLIENTS;
		if(Idx == 0)
			continue;
		int Candidate = m_aTeamRelayOrder[Team][Idx];
		if(Candidate != -1 && GameServer()->m_apPlayers[Candidate] && GameServer()->m_apPlayers[Candidate]->GetCharacter())
		{
			AfterNextRunnerId = Candidate;
			break;
		}
	}

	char aBuf[256];
	if(AfterNextRunnerId != -1)
	{
		str_format(aBuf, sizeof(aBuf), "Runner changed! Now: %s, Next: %s", Server()->ClientName(NextRunnerId), Server()->ClientName(AfterNextRunnerId));
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "Runner changed! Now: %s", Server()->ClientName(NextRunnerId));
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
			GameServer()->SendChatTarget(i, aBuf);
	}
}

void CGameTeams::OnRelayTick(int Team)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return;
	if(!IsRelayTeam(Team))
		return;

	int Now = Server()->Tick();

	switch(m_aTeamRelayState[Team])
	{
	case RELAY_STATE_RESETTING:
	{
		bool AllReady = true;
		bool AnyOnline = false;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Core.Team(i) != Team || !GameServer()->m_apPlayers[i])
				continue;
			AnyOnline = true;
			CCharacter *pChr = GameServer()->m_apPlayers[i]->GetCharacter();
			if(!pChr || !pChr->IsAlive())
			{
				AllReady = false;
				break;
			}
		}
		if(!AnyOnline)
		{
			m_aTeamRelayState[Team] = RELAY_STATE_IDLE;
			return;
		}
		if(AllReady)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_Core.Team(i) != Team || !GameServer()->m_apPlayers[i])
					continue;
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				CCharacter *pChr = pPlayer->GetCharacter();
				if(!pChr)
					continue;
				pPlayer->ForceRelaySpec(true);
				GameServer()->Teleport_relay(pChr, m_aTeamRelayRecordPos[Team]);
				pChr->ResetJumps();
				pChr->SetDeepFrozen(false);
				pChr->Unfreeze();
				pChr->ResetVelocity();
			}
			int CountdownSec = 5;
			m_aTeamRelayCountdownEndTick[Team] = Now + (CountdownSec - 1) * Server()->TickSpeed(); // -1 for 1s offset
			m_aTeamRelayState[Team] = RELAY_STATE_COUNTDOWN;
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "还有 %d 秒", CountdownSec);
			GameServer()->SendChatTeam(Team, aBuf);
		}
		break;
	}
	case RELAY_STATE_COUNTDOWN:
	{
			int Remaining = (m_aTeamRelayCountdownEndTick[Team] - Now) / Server()->TickSpeed() + 1; // +1s offset
		if(Remaining != m_aTeamRelayLastWarnedSecond[Team] && Remaining >= 1 && Remaining <= 5)
		{
			m_aTeamRelayLastWarnedSecond[Team] = Remaining;
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "比赛将在 %d秒后开始", Remaining);
			GameServer()->SendChatTeam(Team, aBuf);
		}
		if(Now >= m_aTeamRelayCountdownEndTick[Team])
		{
			m_aTeamRelayState[Team] = RELAY_STATE_RUNNING;
			m_aTeamRelayTickStart[Team] = Now;
			m_aTeamCurrentRunnerIndex[Team] = 0;
			m_aTeamRelayLastWarnedSecond[Team] = -1;
			GameServer()->SendChatTeam(Team, "开始！! 出发!");
			AdvanceRelayRunner(Team, Now);
		}
		break;
	}
	case RELAY_STATE_RUNNING:
	{
		int Elapsed = Now - m_aTeamRelayTickStart[Team];
		int Duration = m_aTeamRelayDurationTicks[Team];
			int Remaining = (Duration - Elapsed) / Server()->TickSpeed() + 1; // +1s offset for display
		if(Remaining != m_aTeamRelayLastWarnedSecond[Team] && Remaining >= 1 && Remaining <= 3)
		{
			m_aTeamRelayLastWarnedSecond[Team] = Remaining;
			// Find current runner and next runner
			int CurrentRunnerIndex = m_aTeamCurrentRunnerIndex[Team];
			int CurrentRunnerId = (CurrentRunnerIndex >= 1 && CurrentRunnerIndex < MAX_CLIENTS) ? m_aTeamRelayOrder[Team][CurrentRunnerIndex] : -1;
			int NextRunnerId = -1;
			for(int Offset = 1; Offset < MAX_CLIENTS; Offset++)
			{
				int Idx = CurrentRunnerIndex + Offset;
				if(Idx >= MAX_CLIENTS)
					Idx -= MAX_CLIENTS;
				if(Idx == 0)
					continue;
				int Candidate = m_aTeamRelayOrder[Team][Idx];
				if(Candidate != -1 && GameServer()->m_apPlayers[Candidate] && GameServer()->m_apPlayers[Candidate]->GetCharacter())
				{
					NextRunnerId = Candidate;
					break;
				}
			}
			char aBuf[256];
			if(NextRunnerId != -1)
			{
				str_format(aBuf, sizeof(aBuf), "还有 %d 秒, 下一位准备: %s", Remaining, Server()->ClientName(NextRunnerId));
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "还有 %d 秒", Remaining);
			}
			// Send to all team members
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
					GameServer()->SendChatTarget(i, aBuf);
			}
		}
		if(Elapsed >= Duration)
		{
			m_aTeamRelayLastWarnedSecond[Team] = -1;
			AdvanceRelayRunner(Team, Now);
		}
		break;
	}
	default:
		break;
	}
}

void CGameTeams::FinishRelayTeam(int Team, int FinisherId)
{
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return;
	if(m_aTeamRelayState[Team] != RELAY_STATE_RUNNING)
		return;

	m_aTeamRelayState[Team] = RELAY_STATE_FINISHED;
	m_aTeamRelayRaceTimerStarted[Team] = false; // Reset for next relay

	vec2 FinishPos = m_aTeamRelayRecordPos[Team];
	if(FinisherId >= 0 && FinisherId < MAX_CLIENTS && GameServer()->m_apPlayers[FinisherId])
	{
		CCharacter *pFinisherChr = GameServer()->m_apPlayers[FinisherId]->GetCharacter();
		if(pFinisherChr)
			FinishPos = pFinisherChr->m_Pos;
	}

	// Calculate finish time - try multiple sources
	int FinishTimeTicks = 0;
	if(FinisherId >= 0 && FinisherId < MAX_CLIENTS && GameServer()->m_apPlayers[FinisherId])
	{
		CPlayer *pFinisher = GameServer()->m_apPlayers[FinisherId];
		CCharacter *pFinisherChr = pFinisher->GetCharacter();
		
		// Try GetStartTime first
		int StartTime = GetStartTime(pFinisher);
		if(StartTime > 0)
		{
			FinishTimeTicks = Server()->Tick() - StartTime;
		}
		// Fallback: try character's m_StartTime directly
		else if(pFinisherChr && pFinisherChr->m_StartTime > 0)
		{
			FinishTimeTicks = Server()->Tick() - pFinisherChr->m_StartTime;
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) != Team || !GameServer()->m_apPlayers[i])
			continue;
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		CCharacter *pChr = pPlayer->GetCharacter();
		if(!pChr)
			continue;
		pPlayer->ForceRelaySpec(false);
		GameServer()->Teleport_relay(pChr, FinishPos);
		pChr->ResetJumps();
		pChr->SetDeepFrozen(false);
		pChr->Unfreeze();
		pChr->ResetVelocity();
		SetDDRaceState(pPlayer, ERaceState::FINISHED);
	}

	// Always broadcast finish message
	char aBuf[256];
	if(FinishTimeTicks > 0)
	{
		float Time = FinishTimeTicks / (float)Server()->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "Relay finished by %s in %d minute(s) %5.2f second(s)",
			Server()->ClientName(FinisherId), (int)Time / 60, Time - ((int)Time / 60 * 60));
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "Relay finished by %s (time unknown)", Server()->ClientName(FinisherId));
	}
	GameServer()->SendChatTeam(Team, aBuf);
	
	// Also send to all players for visibility
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
			GameServer()->SendChatTarget(i, aBuf);
	}
	
	// Broadcast finish to all players (like normal finish)
	if(FinishTimeTicks > 0 && FinisherId >= 0 && FinisherId < MAX_CLIENTS && GameServer()->m_apPlayers[FinisherId])
	{
		CPlayer *pFinisher = GameServer()->m_apPlayers[FinisherId];
		char aTimestamp[TIMESTAMP_STR_LENGTH];
		str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE);
		OnFinish(pFinisher, FinishTimeTicks, aTimestamp);
	}
}

//yirou
void CGameTeams::PauseAllRelays()
{
	int PausedCount = 0;
	for(int Team = 1; Team < NUM_DDRACE_TEAMS; Team++)
	{
		if(!IsValidTeamNumber(Team))
			continue;
		if(m_aTeamRelayState[Team] != RELAY_STATE_IDLE)
		{
			// Pause this relay
			m_aTeamRelayState[Team] = RELAY_STATE_IDLE;
			
			// Exit all players from spec
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
				{
					GameServer()->m_apPlayers[i]->ForceRelaySpec(false);
				}
			}
			
			// Reset runner index
			m_aTeamCurrentRunnerIndex[Team] = 0;
			m_aTeamRelayTickStart[Team] = 0;
			m_aTeamRelayRaceTimerStarted[Team] = false; // Reset race timer flag
			
			PausedCount++;
		}
	}
	
	if(PausedCount > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "All relays paused (%d team(s))", PausedCount);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i])
				GameServer()->SendChatTarget(i, aBuf);
		}
	}
}
