/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include "score.h"
#include "teehistorian.h"
#include <engine/shared/config.h>

#include "entities/character.h"
#include "player.h"

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
		m_TeamState[i] = TEAMSTATE_EMPTY;
		m_TeamLocked[i] = false;
		m_TeeFinished[i] = false;
		m_LastChat[i] = 0;
		m_pSaveTeamResult[i] = nullptr;

		m_Invited[i] = 0;
		m_Practice[i] = false;
		m_LastSwap[i] = 0;
	}
}

void CGameTeams::ResetRoundState(int Team)
{
	ResetInvited(Team);
	ResetSwitchers(Team);
	m_LastSwap[Team] = 0;

	m_Practice[Team] = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_VotedForPractice = false;
			GameServer()->m_apPlayers[i]->m_SwapTargetsClientID = -1;
		}
	}
}

void CGameTeams::ResetSwitchers(int Team)
{
	if(GameServer()->Collision()->m_NumSwitchers > 0)
	{
		for(int i = 0; i < GameServer()->Collision()->m_NumSwitchers + 1; ++i)
		{
			GameServer()->Collision()->m_pSwitchers[i].m_Status[Team] = GameServer()->Collision()->m_pSwitchers[i].m_Initial;
			GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team] = 0;
			GameServer()->Collision()->m_pSwitchers[i].m_Type[Team] = TILE_SWITCHOPEN;
		}
	}
}

void CGameTeams::OnCharacterStart(int ClientID)
{
	int Tick = Server()->Tick();
	CCharacter *pStartingChar = Character(ClientID);
	if(!pStartingChar)
		return;
	if((g_Config.m_SvTeam == 3 || m_Core.Team(ClientID) != TEAM_FLOCK) && pStartingChar->m_DDRaceState == DDRACE_FINISHED)
		return;
	if(g_Config.m_SvTeam != 3 &&
		(m_Core.Team(ClientID) == TEAM_FLOCK || m_Core.Team(ClientID) == TEAM_SUPER))
	{
		pStartingChar->m_DDRaceState = DDRACE_STARTED;
		pStartingChar->m_StartTime = Tick;
		return;
	}
	bool Waiting = false;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_Core.Team(ClientID) != m_Core.Team(i))
			continue;
		CPlayer *pPlayer = GetPlayer(i);
		if(!pPlayer || !pPlayer->IsPlaying())
			continue;
		if(GetDDRaceState(pPlayer) != DDRACE_FINISHED)
			continue;

		Waiting = true;
		pStartingChar->m_DDRaceState = DDRACE_NONE;

		if(m_LastChat[ClientID] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s has finished and didn't go through start yet, wait for him or join another team.",
				Server()->ClientName(i));
			GameServer()->SendChatTarget(ClientID, aBuf);
			m_LastChat[ClientID] = Tick;
		}
		if(m_LastChat[i] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s wants to start a new round, kill or walk to start.",
				Server()->ClientName(ClientID));
			GameServer()->SendChatTarget(i, aBuf);
			m_LastChat[i] = Tick;
		}
	}

	if(m_TeamState[m_Core.Team(ClientID)] < TEAMSTATE_STARTED && !Waiting)
	{
		ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_STARTED);

		int NumPlayers = Count(m_Core.Team(ClientID));

		char aBuf[512];
		str_format(
			aBuf,
			sizeof(aBuf),
			"Team %d started with %d player%s: ",
			m_Core.Team(ClientID),
			NumPlayers,
			NumPlayers == 1 ? "" : "s");

		bool First = true;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_Core.Team(ClientID) == m_Core.Team(i))
			{
				CPlayer *pPlayer = GetPlayer(i);
				// TODO: THE PROBLEM IS THAT THERE IS NO CHARACTER SO START TIME CAN'T BE SET!
				if(pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientID))))
				{
					SetDDRaceState(pPlayer, DDRACE_STARTED);
					SetStartTime(pPlayer, Tick);

					if(First)
						First = false;
					else
						str_append(aBuf, ", ", sizeof(aBuf));

					str_append(aBuf, GameServer()->Server()->ClientName(i), sizeof(aBuf));
				}
			}
		}

		if(g_Config.m_SvTeam < 3 && g_Config.m_SvTeamMaxSize != 2 && g_Config.m_SvPauseable)
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				CPlayer *pPlayer = GetPlayer(i);
				if(m_Core.Team(ClientID) == m_Core.Team(i) && pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientID))))
				{
					GameServer()->SendChatTarget(i, aBuf);
				}
			}
		}
	}
}

void CGameTeams::OnCharacterFinish(int ClientID)
{
	if((m_Core.Team(ClientID) == TEAM_FLOCK && g_Config.m_SvTeam != 3) || m_Core.Team(ClientID) == TEAM_SUPER)
	{
		CPlayer *pPlayer = GetPlayer(ClientID);
		if(pPlayer && pPlayer->IsPlaying())
		{
			float Time = (float)(Server()->Tick() - GetStartTime(pPlayer)) / ((float)Server()->TickSpeed());
			if(Time < 0.000001f)
				return;
			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE); // 2019-04-02 19:41:58

			OnFinish(pPlayer, Time, aTimestamp);
		}
	}
	else
	{
		m_TeeFinished[ClientID] = true;

		CheckTeamFinished(m_Core.Team(ClientID));
	}
}

void CGameTeams::CheckTeamFinished(int Team)
{
	if(TeamFinished(Team))
	{
		CPlayer *TeamPlayers[MAX_CLIENTS];
		unsigned int PlayersCount = 0;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(Team == m_Core.Team(i))
			{
				CPlayer *pPlayer = GetPlayer(i);
				if(pPlayer && pPlayer->IsPlaying())
				{
					m_TeeFinished[i] = false;

					TeamPlayers[PlayersCount++] = pPlayer;
				}
			}
		}

		if(PlayersCount > 0)
		{
			float Time = (float)(Server()->Tick() - GetStartTime(TeamPlayers[0])) / ((float)Server()->TickSpeed());
			if(Time < 0.000001f)
			{
				return;
			}

			if(m_Practice[Team])
			{
				ChangeTeamState(Team, TEAMSTATE_FINISHED);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf),
					"Your team would've finished in: %d minute(s) %5.2f second(s). Since you had practice mode enabled your rank doesn't count.",
					(int)Time / 60, Time - ((int)Time / 60 * 60));

				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
					{
						GameServer()->SendChatTarget(i, aBuf);
					}
				}

				for(unsigned int i = 0; i < PlayersCount; ++i)
				{
					SetDDRaceState(TeamPlayers[i], DDRACE_FINISHED);
				}

				return;
			}

			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE); // 2019-04-02 19:41:58

			for(unsigned int i = 0; i < PlayersCount; ++i)
				OnFinish(TeamPlayers[i], Time, aTimestamp);
			ChangeTeamState(Team, TEAMSTATE_FINISHED); // TODO: Make it better
			OnTeamFinish(TeamPlayers, PlayersCount, Time, aTimestamp);
		}
	}
}

const char *CGameTeams::SetCharacterTeam(int ClientID, int Team)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return "Invalid client ID";
	if(Team < 0 || Team >= MAX_CLIENTS + 1)
		return "Invalid team number";
	if(Team != TEAM_SUPER && m_TeamState[Team] > TEAMSTATE_OPEN)
		return "This team started already";
	if(m_Core.Team(ClientID) == Team)
		return "You are in this team already";
	if(!Character(ClientID))
		return "Your character is not valid";
	if(Team == TEAM_SUPER && !Character(ClientID)->m_Super)
		return "You can't join super team if you don't have super rights";
	if(Team != TEAM_SUPER && Character(ClientID)->m_DDRaceState != DDRACE_NONE)
		return "You have started racing already";
	// No cheating through noob filter with practice and then leaving team
	if(m_Practice[m_Core.Team(ClientID)])
		return "You have used practice mode already";

	// you can not join a team which is currently in the process of saving,
	// because the save-process can fail and then the team is reset into the game
	if(Team != TEAM_SUPER && GetSaving(Team))
		return "Your team is currently saving";
	if(m_Core.Team(ClientID) != TEAM_SUPER && GetSaving(m_Core.Team(ClientID)))
		return "This team is currently saving";

	SetForceCharacterTeam(ClientID, Team);
	return nullptr;
}

void CGameTeams::SetForceCharacterTeam(int ClientID, int Team)
{
	m_TeeFinished[ClientID] = false;
	int OldTeam = m_Core.Team(ClientID);

	if(Team != OldTeam && (OldTeam != TEAM_FLOCK || g_Config.m_SvTeam == 3) && OldTeam != TEAM_SUPER && m_TeamState[OldTeam] != TEAMSTATE_EMPTY)
	{
		bool NoElseInOldTeam = Count(OldTeam) <= 1;
		if(NoElseInOldTeam)
		{
			m_TeamState[OldTeam] = TEAMSTATE_EMPTY;

			// unlock team when last player leaves
			SetTeamLock(OldTeam, false);
			ResetRoundState(OldTeam);
			// do not reset SaveTeamResult, because it should be logged into teehistorian even if the team leaves
		}
	}

	m_Core.Team(ClientID, Team);

	if(OldTeam != Team)
	{
		for(int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(GetPlayer(LoopClientID))
				SendTeamsState(LoopClientID);

		if(GetPlayer(ClientID))
		{
			GetPlayer(ClientID)->m_VotedForPractice = false;
			GetPlayer(ClientID)->m_SwapTargetsClientID = -1;
		}
	}

	if(Team != TEAM_SUPER && (m_TeamState[Team] == TEAMSTATE_EMPTY || m_TeamLocked[Team]))
	{
		if(!m_TeamLocked[Team])
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
	m_TeamState[Team] = State;
}

bool CGameTeams::TeamFinished(int Team)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team && !m_TeeFinished[i])
			return false;
	return true;
}

int64_t CGameTeams::TeamMask(int Team, int ExceptID, int Asker)
{
	int64_t Mask = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == ExceptID)
			continue; // Explicitly excluded
		if(!GetPlayer(i))
			continue; // Player doesn't exist

		if(!(GetPlayer(i)->GetTeam() == -1 || GetPlayer(i)->IsPaused()))
		{ // Not spectator
			if(i != Asker)
			{ // Actions of other players
				if(!Character(i))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == 2)
				{
					if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == 0)
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
		else if(GetPlayer(i)->m_SpectatorID != SPEC_FREEVIEW)
		{ // Spectating specific player
			if(GetPlayer(i)->m_SpectatorID != Asker)
			{ // Actions of other players
				if(!Character(GetPlayer(i)->m_SpectatorID))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == 2)
				{
					if(m_Core.Team(GetPlayer(i)->m_SpectatorID) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorID) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == 0)
				{
					if(m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if(m_Core.GetSolo(GetPlayer(i)->m_SpectatorID))
						continue; // When in solo part don't show others
					if(m_Core.Team(GetPlayer(i)->m_SpectatorID) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorID) != TEAM_SUPER)
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

		Mask |= 1LL << i;
	}
	return Mask;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	if(g_Config.m_SvTeam == 3)
		return;

	if(!m_pGameContext->m_apPlayers[ClientID])
		return;

	CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);
	CMsgPacker MsgLegacy(NETMSGTYPE_SV_TEAMSSTATELEGACY);

	for(unsigned i = 0; i < MAX_CLIENTS; i++)
	{
		Msg.AddInt(m_Core.Team(i));
		MsgLegacy.AddInt(m_Core.Team(i));
	}

	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	int ClientVersion = m_pGameContext->m_apPlayers[ClientID]->GetClientVersion();
	if(!Server()->IsSixup(ClientID) && VERSION_DDRACE < ClientVersion && ClientVersion < VERSION_DDNET_MSG_LEGACY)
	{
		Server()->SendMsg(&MsgLegacy, MSGFLAG_VITAL, ClientID);
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

void CGameTeams::SetCpActive(CPlayer *Player, int CpActive)
{
	if(!Player)
		return;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		pChar->m_CpActive = CpActive;
}

float *CGameTeams::GetCpCurrent(CPlayer *Player)
{
	if(!Player)
		return NULL;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_CpCurrent;
	return NULL;
}

void CGameTeams::OnTeamFinish(CPlayer **Players, unsigned int Size, float Time, const char *pTimestamp)
{
	int PlayerCIDs[MAX_CLIENTS];

	for(unsigned int i = 0; i < Size; i++)
	{
		PlayerCIDs[i] = Players[i]->GetCID();

		if(g_Config.m_SvRejoinTeam0 && g_Config.m_SvTeam != 3 && (m_Core.Team(Players[i]->GetCID()) >= TEAM_SUPER || !m_TeamLocked[m_Core.Team(Players[i]->GetCID())]))
		{
			SetForceCharacterTeam(Players[i]->GetCID(), TEAM_FLOCK);
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "%s joined team 0",
				GameServer()->Server()->ClientName(Players[i]->GetCID()));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		}
	}

	if(Size >= 2)
		GameServer()->Score()->SaveTeamScore(PlayerCIDs, Size, Time, pTimestamp);
}

void CGameTeams::OnFinish(CPlayer *Player, float Time, const char *pTimestamp)
{
	if(!Player || !Player->IsPlaying())
		return;
	// TODO:DDRace:btd: this ugly
	const int ClientID = Player->GetCID();
	CPlayerData *pData = GameServer()->Score()->PlayerData(ClientID);

	char aBuf[128];
	SetCpActive(Player, -2);
	// Note that the "finished in" message is parsed by the client
	str_format(aBuf, sizeof(aBuf),
		"%s finished in: %d minute(s) %5.2f second(s)",
		Server()->ClientName(ClientID), (int)Time / 60,
		Time - ((int)Time / 60 * 60));
	if(g_Config.m_SvHideScore || !g_Config.m_SvSaveWorseScores)
		GameServer()->SendChatTarget(ClientID, aBuf, CGameContext::CHAT_SIX);
	else
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1., CGameContext::CHAT_SIX);

	float Diff = fabs(Time - pData->m_BestTime);

	if(Time - pData->m_BestTime < 0)
	{
		// new record \o/
		Server()->SaveDemo(ClientID, Time);

		if(Diff >= 60)
			str_format(aBuf, sizeof(aBuf), "New record: %d minute(s) %5.2f second(s) better.",
				(int)Diff / 60, Diff - ((int)Diff / 60 * 60));
		else
			str_format(aBuf, sizeof(aBuf), "New record: %5.2f second(s) better.",
				Diff);
		if(g_Config.m_SvHideScore || !g_Config.m_SvSaveWorseScores)
			GameServer()->SendChatTarget(ClientID, aBuf, CGameContext::CHAT_SIX);
		else
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, CGameContext::CHAT_SIX);
	}
	else if(pData->m_BestTime != 0) // tee has already finished?
	{
		Server()->StopRecord(ClientID);

		if(Diff <= 0.005f)
		{
			GameServer()->SendChatTarget(ClientID,
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
			GameServer()->SendChatTarget(ClientID, aBuf, CGameContext::CHAT_SIX); // this is private, sent only to the tee
		}
	}
	else
	{
		Server()->SaveDemo(ClientID, Time);
	}

	bool CallSaveScore = g_Config.m_SvSaveWorseScores;

	if(!pData->m_BestTime || Time < pData->m_BestTime)
	{
		// update the score
		pData->Set(Time, GetCpCurrent(Player));
		CallSaveScore = true;
	}

	if(CallSaveScore)
		if(g_Config.m_SvNamelessScore || !str_startswith(Server()->ClientName(ClientID), "nameless tee"))
			GameServer()->Score()->SaveScore(ClientID, Time, pTimestamp,
				GetCpCurrent(Player), Player->m_NotEligibleForFinish);

	bool NeedToSendNewRecord = false;
	// update server best time
	if(GameServer()->m_pController->m_CurrentRecord == 0 || Time < GameServer()->m_pController->m_CurrentRecord)
	{
		// check for nameless
		if(g_Config.m_SvNamelessScore || !str_startswith(Server()->ClientName(ClientID), "nameless tee"))
		{
			GameServer()->m_pController->m_CurrentRecord = Time;
			NeedToSendNewRecord = true;
		}
	}

	SetDDRaceState(Player, DDRACE_FINISHED);
	// set player score
	if(!pData->m_CurrentTime || pData->m_CurrentTime > Time)
	{
		pData->m_CurrentTime = Time;
		NeedToSendNewRecord = true;
	}

	if(NeedToSendNewRecord && Player->GetClientVersion() >= VERSION_DDRACE)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetClientVersion() >= VERSION_DDRACE)
			{
				GameServer()->SendRecord(i);
			}
		}
	}

	{
		CNetMsg_Sv_DDRaceTime Msg;
		CNetMsg_Sv_DDRaceTimeLegacy MsgLegacy;
		MsgLegacy.m_Time = Msg.m_Time = (int)(Time * 100.0f);
		MsgLegacy.m_Check = Msg.m_Check = 0;
		MsgLegacy.m_Finish = Msg.m_Finish = 1;

		if(pData->m_BestTime)
		{
			float Diff = (Time - pData->m_BestTime) * 100;
			MsgLegacy.m_Check = Msg.m_Check = (int)Diff;
		}

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		if(!Server()->IsSixup(ClientID) && VERSION_DDRACE <= Player->GetClientVersion() && Player->GetClientVersion() < VERSION_DDNET_MSG_LEGACY)
		{
			Server()->SendPackMsg(&MsgLegacy, MSGFLAG_VITAL, ClientID);
		}
	}

	{
		protocol7::CNetMsg_Sv_RaceFinish Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Time = Time * 1000;
		Msg.m_Diff = Diff * 1000 * (Time < pData->m_BestTime ? -1 : 1);
		Msg.m_RecordPersonal = Time < pData->m_BestTime;
		Msg.m_RecordServer = Time < GameServer()->m_pController->m_CurrentRecord;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
	}

	int TTime = 0 - (int)Time;
	if(Player->m_Score < TTime || !Player->m_HasFinishScore)
	{
		Player->m_Score = TTime;
		Player->m_HasFinishScore = true;
	}
}

void CGameTeams::RequestTeamSwap(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team)
{
	if(!pPlayer || !pTargetPlayer)
		return;

	char aBuf[512];
	if(pPlayer->m_SwapTargetsClientID == pTargetPlayer->GetCID())
	{
		str_format(aBuf, sizeof(aBuf),
			"%s has already requested to swap with %s.",
			Server()->ClientName(pPlayer->GetCID()), Server()->ClientName(pTargetPlayer->GetCID()));

		GameServer()->SendChatTeam(Team, aBuf);
		return;
	}

	str_format(aBuf, sizeof(aBuf),
		"%s has requested to swap with %s. Please wait %d seconds then type /swap %s.",
		Server()->ClientName(pPlayer->GetCID()), Server()->ClientName(pTargetPlayer->GetCID()), g_Config.m_SvSaveSwapGamesDelay, Server()->ClientName(pPlayer->GetCID()));

	GameServer()->SendChatTeam(Team, aBuf);

	pPlayer->m_SwapTargetsClientID = pTargetPlayer->GetCID();
	m_LastSwap[Team] = Server()->Tick();
}

void CGameTeams::SwapTeamCharacters(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team)
{
	if(!pPlayer || !pTargetPlayer)
		return;

	char aBuf[128];

	int Since = (Server()->Tick() - m_LastSwap[Team]) / Server()->TickSpeed();
	if(Since < g_Config.m_SvSaveSwapGamesDelay)
	{
		str_format(aBuf, sizeof(aBuf),
			"You have to wait %d seconds until you can swap.",
			g_Config.m_SvSaveSwapGamesDelay - Since);

		GameServer()->SendChatTeam(Team, aBuf);

		return;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_SwapTargetsClientID = -1;
		}
	}

	int TimeoutAfterDelay = g_Config.m_SvSaveSwapGamesDelay + g_Config.m_SvSwapTimeout;
	if(Since >= TimeoutAfterDelay)
	{
		str_format(aBuf, sizeof(aBuf),
			"Your swap request timed out %d seconds ago. Use /swap again to re-initiate it.",
			Since - g_Config.m_SvSwapTimeout);

		GameServer()->SendChatTeam(Team, aBuf);

		return;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->GetCharacter()->ResetHook();
			GameServer()->m_World.ReleaseHooked(i);
		}
	}

	CSaveTee PrimarySavedTee;
	PrimarySavedTee.Save(pPlayer->GetCharacter());

	CSaveTee SecondarySavedTee;
	SecondarySavedTee.Save(pTargetPlayer->GetCharacter());

	PrimarySavedTee.Load(pTargetPlayer->GetCharacter(), Team);
	SecondarySavedTee.Load(pPlayer->GetCharacter(), Team);

	str_format(aBuf, sizeof(aBuf),
		"%s has swapped with %s.",
		Server()->ClientName(pPlayer->GetCID()), Server()->ClientName(pTargetPlayer->GetCID()));

	GameServer()->SendChatTeam(Team, aBuf);
}

void CGameTeams::ProcessSaveTeam()
{
	for(int Team = 0; Team < MAX_CLIENTS; Team++)
	{
		if(m_pSaveTeamResult[Team] == nullptr || !m_pSaveTeamResult[Team]->m_Completed)
			continue;
		if(m_pSaveTeamResult[Team]->m_aBroadcast[0] != '\0')
			GameServer()->SendBroadcast(m_pSaveTeamResult[Team]->m_aBroadcast, -1);
		if(m_pSaveTeamResult[Team]->m_aMessage[0] != '\0' && m_pSaveTeamResult[Team]->m_Status != CScoreSaveResult::LOAD_FAILED)
			GameServer()->SendChatTeam(Team, m_pSaveTeamResult[Team]->m_aMessage);
		switch(m_pSaveTeamResult[Team]->m_Status)
		{
		case CScoreSaveResult::SAVE_SUCCESS:
		{
			if(GameServer()->TeeHistorianActive())
			{
				GameServer()->TeeHistorian()->RecordTeamSaveSuccess(
					Team,
					m_pSaveTeamResult[Team]->m_SaveID,
					m_pSaveTeamResult[Team]->m_SavedTeam.GetString());
			}
			for(int i = 0; i < m_pSaveTeamResult[Team]->m_SavedTeam.GetMembersCount(); i++)
			{
				if(m_pSaveTeamResult[Team]->m_SavedTeam.m_pSavedTees->IsHooking())
				{
					int ClientID = m_pSaveTeamResult[Team]->m_SavedTeam.m_pSavedTees->GetClientID();
					if(GameServer()->m_apPlayers[ClientID] != nullptr)
						GameServer()->SendChatTarget(ClientID, "Start holding the hook before loading the savegame to keep the hook");
				}
			}
			ResetSavedTeam(m_pSaveTeamResult[Team]->m_RequestingPlayer, Team);
			char aSaveID[UUID_MAXSTRSIZE];
			FormatUuid(m_pSaveTeamResult[Team]->m_SaveID, aSaveID, UUID_MAXSTRSIZE);
			dbg_msg("save", "Save successful: %s", aSaveID);
			break;
		}
		case CScoreSaveResult::SAVE_FAILED:
			if(GameServer()->TeeHistorianActive())
				GameServer()->TeeHistorian()->RecordTeamSaveFailure(Team);
			if(Count(Team) > 0)
			{
				// load weak/strong order to prevent switching weak/strong while saving
				m_pSaveTeamResult[Team]->m_SavedTeam.Load(Team, false);
			}
			break;
		case CScoreSaveResult::LOAD_SUCCESS:
		{
			if(GameServer()->TeeHistorianActive())
			{
				GameServer()->TeeHistorian()->RecordTeamLoadSuccess(
					Team,
					m_pSaveTeamResult[Team]->m_SaveID,
					m_pSaveTeamResult[Team]->m_SavedTeam.GetString());
			}
			if(Count(Team) > 0)
			{
				// keep current weak/strong order as on some maps there is no other way of switching
				m_pSaveTeamResult[Team]->m_SavedTeam.Load(Team, true);
			}
			char aSaveID[UUID_MAXSTRSIZE];
			FormatUuid(m_pSaveTeamResult[Team]->m_SaveID, aSaveID, UUID_MAXSTRSIZE);
			dbg_msg("save", "Load successful: %s", aSaveID);
			break;
		}
		case CScoreSaveResult::LOAD_FAILED:
			if(GameServer()->TeeHistorianActive())
				GameServer()->TeeHistorian()->RecordTeamLoadFailure(Team);
			if(m_pSaveTeamResult[Team]->m_aMessage[0] != '\0')
				GameServer()->SendChatTarget(m_pSaveTeamResult[Team]->m_RequestingPlayer, m_pSaveTeamResult[Team]->m_aMessage);
			break;
		}
		m_pSaveTeamResult[Team] = nullptr;
	}
}

void CGameTeams::OnCharacterSpawn(int ClientID)
{
	m_Core.SetSolo(ClientID, false);
	int Team = m_Core.Team(ClientID);

	if(GetSaving(Team))
		return;

	if(m_Core.Team(ClientID) >= TEAM_SUPER || !m_TeamLocked[Team])
	{
		if(g_Config.m_SvTeam != 3)
			SetForceCharacterTeam(ClientID, TEAM_FLOCK);
		else
			SetForceCharacterTeam(ClientID, ClientID); // initialize team
		CheckTeamFinished(Team);
	}
}

void CGameTeams::OnCharacterDeath(int ClientID, int Weapon)
{
	m_Core.SetSolo(ClientID, false);

	int Team = m_Core.Team(ClientID);
	if(GetSaving(Team))
		return;
	bool Locked = TeamLocked(Team) && Weapon != WEAPON_GAME;

	if(g_Config.m_SvTeam == 3 && Team != TEAM_SUPER)
	{
		ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);
		ResetRoundState(Team);
	}
	else if(Locked)
	{
		SetForceCharacterTeam(ClientID, Team);

		if(GetTeamState(Team) != TEAMSTATE_OPEN)
		{
			ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);

			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Everyone in your locked team was killed because '%s' %s.", Server()->ClientName(ClientID), Weapon == WEAPON_SELF ? "killed" : "died");

			m_Practice[Team] = false;

			for(int i = 0; i < MAX_CLIENTS; i++)
				if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
				{
					GameServer()->m_apPlayers[i]->m_VotedForPractice = false;

					if(i != ClientID)
					{
						GameServer()->m_apPlayers[i]->KillCharacter(WEAPON_SELF);
						if(Weapon == WEAPON_SELF)
							GameServer()->m_apPlayers[i]->Respawn(true); // spawn the rest of team with weak hook on the killer
					}
					if(Count(Team) > 1)
						GameServer()->SendChatTarget(i, aBuf);
				}
		}
	}
	else
	{
		SetForceCharacterTeam(ClientID, TEAM_FLOCK);
		CheckTeamFinished(Team);
	}
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
		m_TeamLocked[Team] = Lock;
}

void CGameTeams::ResetInvited(int Team)
{
	m_Invited[Team] = 0;
}

void CGameTeams::SetClientInvited(int Team, int ClientID, bool Invited)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		if(Invited)
			m_Invited[Team] |= 1ULL << ClientID;
		else
			m_Invited[Team] &= ~(1ULL << ClientID);
	}
}

void CGameTeams::KillSavedTeam(int ClientID, int Team)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_VotedForPractice = false;
			GameServer()->m_apPlayers[i]->KillCharacter(WEAPON_SELF);
		}
	}
}

void CGameTeams::ResetSavedTeam(int ClientID, int Team)
{
	if(g_Config.m_SvTeam == 3)
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
