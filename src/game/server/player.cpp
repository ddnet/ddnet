/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"
#include "entities/character.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "score.h"

#include <base/system.h>

#include <engine/antibot.h>
#include <engine/server.h>
#include <engine/shared/config.h>

#include <game/gamecore.h>
#include <game/teamscore.h>

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, uint32_t UniqueClientID, int ClientID, int Team) :
	m_UniqueClientID(UniqueClientID)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_NumInputs = 0;
	Reset();
	GameServer()->Antibot()->OnPlayerInit(m_ClientID);
}

CPlayer::~CPlayer()
{
	GameServer()->Antibot()->OnPlayerDestroy(m_ClientID);
	delete m_pLastTarget;
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Reset()
{
	m_DieTick = Server()->Tick();
	m_PreviousDieTick = m_DieTick;
	m_JoinTick = Server()->Tick();
	delete m_pCharacter;
	m_pCharacter = 0;
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_LastInvited = 0;
	m_WeakHookSpawn = false;

	int *pIdMap = Server()->GetIdMap(m_ClientID);
	for(int i = 1; i < VANILLA_MAX_CLIENTS; i++)
	{
		pIdMap[i] = -1;
	}
	pIdMap[0] = m_ClientID;

	// DDRace

	m_LastCommandPos = 0;
	m_LastPlaytime = 0;
	m_ChatScore = 0;
	m_Moderating = false;
	m_EyeEmoteEnabled = true;
	if(Server()->IsSixup(m_ClientID))
		m_TimerType = TIMERTYPE_SIXUP;
	else
		m_TimerType = (g_Config.m_SvDefaultTimerType == TIMERTYPE_GAMETIMER || g_Config.m_SvDefaultTimerType == TIMERTYPE_GAMETIMER_AND_BROADCAST) ? TIMERTYPE_BROADCAST : g_Config.m_SvDefaultTimerType;

	m_DefEmote = EMOTE_NORMAL;
	m_Afk = true;
	m_LastWhisperTo = -1;
	m_LastSetSpectatorMode = 0;
	m_aTimeoutCode[0] = '\0';
	delete m_pLastTarget;
	m_pLastTarget = new CNetObj_PlayerInput({0});
	m_LastTargetInit = false;
	m_TuneZone = 0;
	m_TuneZoneOld = m_TuneZone;
	m_Halloween = false;
	m_FirstPacket = true;

	m_SendVoteIndex = -1;

	if(g_Config.m_Events)
	{
		time_t RawTime;
		struct tm *pTimeInfo;
		time(&RawTime);
		pTimeInfo = localtime(&RawTime);
		if((pTimeInfo->tm_mon == 11 && pTimeInfo->tm_mday == 31) || (pTimeInfo->tm_mon == 0 && pTimeInfo->tm_mday == 1))
		{ // New Year
			m_DefEmote = EMOTE_HAPPY;
		}
		else if((pTimeInfo->tm_mon == 9 && pTimeInfo->tm_mday == 31) || (pTimeInfo->tm_mon == 10 && pTimeInfo->tm_mday == 1))
		{ // Halloween
			m_DefEmote = EMOTE_ANGRY;
			m_Halloween = true;
		}
		else
		{
			m_DefEmote = EMOTE_NORMAL;
		}
	}
	m_OverrideEmoteReset = -1;

	GameServer()->Score()->PlayerData(m_ClientID)->Reset();

	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowAll = g_Config.m_SvShowAllDefault;
	m_ShowDistance = vec2(1200, 800);
	m_SpecTeam = false;
	m_NinjaJetpack = false;

	m_Paused = PAUSE_NONE;
	m_DND = false;

	m_LastPause = 0;
	m_Score = -9999;
	m_HasFinishScore = false;

	// Variable initialized:
	m_Last_Team = 0;
	m_LastSQLQuery = 0;
	m_ScoreQueryResult = nullptr;
	m_ScoreFinishResult = nullptr;

	int64_t Now = Server()->Tick();
	int64_t TickSpeed = Server()->TickSpeed();
	// If the player joins within ten seconds of the server becoming
	// non-empty, allow them to vote immediately. This allows players to
	// vote after map changes or when they join an empty server.
	//
	// Otherwise, block voting in the beginning after joining.
	if(Now > GameServer()->m_NonEmptySince + 10 * TickSpeed)
		m_FirstVoteTick = Now + g_Config.m_SvJoinVoteDelay * TickSpeed;
	else
		m_FirstVoteTick = Now;

	m_NotEligibleForFinish = false;
	m_EligibleForFinishCheck = 0;
	m_VotedForPractice = false;
	m_SwapTargetsClientID = -1;
	m_BirthdayAnnounced = false;
}

static int PlayerFlags_SixToSeven(int Flags)
{
	int Seven = 0;
	if(Flags & PLAYERFLAG_CHATTING)
		Seven |= protocol7::PLAYERFLAG_CHATTING;
	if(Flags & PLAYERFLAG_SCOREBOARD)
		Seven |= protocol7::PLAYERFLAG_SCOREBOARD;

	return Seven;
}

void CPlayer::Tick()
{
	if(m_ScoreQueryResult != nullptr && m_ScoreQueryResult->m_Completed)
	{
		ProcessScoreResult(*m_ScoreQueryResult);
		m_ScoreQueryResult = nullptr;
	}
	if(m_ScoreFinishResult != nullptr && m_ScoreFinishResult->m_Completed)
	{
		ProcessScoreResult(*m_ScoreFinishResult);
		m_ScoreFinishResult = nullptr;
	}

	bool ClientIngame = Server()->ClientIngame(m_ClientID);
#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies && m_ClientID >= MAX_CLIENTS - g_Config.m_DbgDummies)
	{
		ClientIngame = true;
	}
#endif
	if(!ClientIngame)
		return;

	if(m_ChatScore > 0)
		m_ChatScore--;

	Server()->SetClientScore(m_ClientID, m_Score);

	if(m_Moderating && m_Afk)
	{
		m_Moderating = false;
		GameServer()->SendChatTarget(m_ClientID, "Active moderator mode disabled because you are afk.");

		if(!GameServer()->PlayerModerating())
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");
	}

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = maximum(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = minimum(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum / Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(Server()->GetNetErrorString(m_ClientID)[0])
	{
		m_Afk = true;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		Server()->ResetNetErrorString(m_ClientID);
	}

	if(!GameServer()->m_World.m_Paused)
	{
		int EarliestRespawnTick = m_PreviousDieTick + Server()->TickSpeed() * 3;
		int RespawnTick = maximum(m_DieTick, EarliestRespawnTick) + 2;
		if(!m_pCharacter && RespawnTick <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				ProcessPause();
				if(!m_Paused)
					m_ViewPos = m_pCharacter->m_Pos;
			}
			else if(!m_pCharacter->IsPaused())
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && !m_WeakHookSpawn)
			TryRespawn();
	}
	else
	{
		++m_DieTick;
		++m_PreviousDieTick;
		++m_JoinTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
	}

	m_TuneZoneOld = m_TuneZone; // determine needed tunings with viewpos
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_ViewPos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if(m_TuneZone != m_TuneZoneOld) // don't send tunings all the time
	{
		GameServer()->SendTuningParams(m_ClientID, m_TuneZone);
	}

	if(m_OverrideEmoteReset >= 0 && m_OverrideEmoteReset <= Server()->Tick())
	{
		m_OverrideEmoteReset = -1;
	}

	if(m_Halloween && m_pCharacter && !m_pCharacter->IsPaused())
	{
		if(1200 - ((Server()->Tick() - m_pCharacter->GetLastAction()) % (1200)) < 5)
		{
			GameServer()->SendEmoticon(GetCID(), EMOTICON_GHOST);
		}
	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags & PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aCurLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if((m_Team == TEAM_SPECTATORS || m_Paused) && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID] && GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter())
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter()->m_Pos;
}

void CPlayer::PostPostTick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif
		if(!Server()->ClientIngame(m_ClientID))
			return;

	if(!GameServer()->m_World.m_Paused && !m_pCharacter && m_Spawning && m_WeakHookSpawn)
		TryRespawn();
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif
		if(!Server()->ClientIngame(m_ClientID))
			return;

	int id = m_ClientID;
	if(!Server()->Translate(id, SnappingClient))
		return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_aSkinName);
	pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	int Latency = SnappingClient == SERVER_DEMO_CLIENT ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aCurLatency[m_ClientID];
	int Score = abs(m_Score) * -1;

	// send 0 if times of others are not shown
	if(SnappingClient != m_ClientID && g_Config.m_SvHideScore)
		Score = -9999;

	if(!Server()->IsSixup(SnappingClient))
	{
		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Local = (int)(m_ClientID == SnappingClient && (m_Paused != PAUSE_PAUSED || SnappingClientVersion >= VERSION_DDNET_OLD));
		pPlayerInfo->m_ClientID = id;
		pPlayerInfo->m_Team = m_Team;
		if(SnappingClientVersion < VERSION_DDNET_INDEPENDENT_SPECTATORS_TEAM)
		{
			// In older versions the SPECTATORS TEAM was also used if the own player is in PAUSE_PAUSED or if any player is in PAUSE_SPEC.
			pPlayerInfo->m_Team = (m_Paused != PAUSE_PAUSED || m_ClientID != SnappingClient) && m_Paused < PAUSE_SPEC ? m_Team : TEAM_SPECTATORS;
		}
	}
	else
	{
		protocol7::CNetObj_PlayerInfo *pPlayerInfo = static_cast<protocol7::CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(protocol7::CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_PlayerFlags = PlayerFlags_SixToSeven(m_PlayerFlags);
		if(SnappingClientVersion >= VERSION_DDRACE && (m_PlayerFlags & PLAYERFLAG_AIM))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_AIM;
		if(Server()->ClientAuthed(m_ClientID))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_ADMIN;

		// Times are in milliseconds for 0.7
		pPlayerInfo->m_Score = Score == -9999 ? -1 : -Score * 1000;
		pPlayerInfo->m_Latency = Latency;
	}

	if(m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused))
	{
		if(!Server()->IsSixup(SnappingClient))
		{
			CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
		else
		{
			protocol7::CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<protocol7::CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(protocol7::CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpecMode = m_SpectatorID == SPEC_FREEVIEW ? protocol7::SPEC_FREEVIEW : protocol7::SPEC_PLAYER;
			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
	}

	CNetObj_DDNetPlayer *pDDNetPlayer = static_cast<CNetObj_DDNetPlayer *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPLAYER, id, sizeof(CNetObj_DDNetPlayer)));
	if(!pDDNetPlayer)
		return;

	pDDNetPlayer->m_AuthLevel = Server()->GetAuthedState(id);
	pDDNetPlayer->m_Flags = 0;
	if(m_Afk)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_AFK;
	if(m_Paused == PAUSE_SPEC)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_SPEC;
	if(m_Paused == PAUSE_PAUSED)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_PAUSED;

	if(Server()->IsSixup(SnappingClient) && m_pCharacter && m_pCharacter->m_DDRaceState == DDRACE_STARTED &&
		GameServer()->m_apPlayers[SnappingClient]->m_TimerType == TIMERTYPE_SIXUP)
	{
		protocol7::CNetObj_PlayerInfoRace *pRaceInfo = static_cast<protocol7::CNetObj_PlayerInfoRace *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_PLAYERINFORACE, id, sizeof(protocol7::CNetObj_PlayerInfoRace)));
		if(!pRaceInfo)
			return;
		pRaceInfo->m_RaceStartTick = m_pCharacter->m_StartTime;
	}

	bool ShowSpec = m_pCharacter && m_pCharacter->IsPaused() && m_pCharacter->CanSnapCharacter(SnappingClient);

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		ShowSpec = ShowSpec && (GameServer()->GetDDRaceTeam(m_ClientID) == GameServer()->GetDDRaceTeam(SnappingClient) || pSnapPlayer->m_ShowOthers == SHOW_OTHERS_ON || (pSnapPlayer->GetTeam() == TEAM_SPECTATORS || pSnapPlayer->IsPaused()));
	}

	if(ShowSpec)
	{
		CNetObj_SpecChar *pSpecChar = static_cast<CNetObj_SpecChar *>(Server()->SnapNewItem(NETOBJTYPE_SPECCHAR, id, sizeof(CNetObj_SpecChar)));
		if(!pSpecChar)
			return;

		pSpecChar->m_X = m_pCharacter->Core()->m_Pos.x;
		pSpecChar->m_Y = m_pCharacter->Core()->m_Pos.y;
	}
}

void CPlayer::FakeSnap()
{
	if(GetClientVersion() >= VERSION_DDNET_OLD)
		return;

	if(Server()->IsSixup(m_ClientID))
		return;

	int FakeID = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, FakeID, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, "");
	StrToInts(&pClientInfo->m_Skin0, 6, "default");

	if(m_Paused != PAUSE_PAUSED)
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, FakeID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = m_Latency.m_Min;
	pPlayerInfo->m_Local = 1;
	pPlayerInfo->m_ClientID = FakeID;
	pPlayerInfo->m_Score = -9999;
	pPlayerInfo->m_Team = TEAM_SPECTATORS;

	CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, FakeID, sizeof(CNetObj_SpectatorInfo)));
	if(!pSpectatorInfo)
		return;

	pSpectatorInfo->m_SpectatorID = m_SpectatorID;
	pSpectatorInfo->m_X = m_ViewPos.x;
	pSpectatorInfo->m_Y = m_ViewPos.y;
}

void CPlayer::OnDisconnect()
{
	KillCharacter();

	m_Moderating = false;
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags & PLAYERFLAG_CHATTING) && (pNewInput->m_PlayerFlags & PLAYERFLAG_CHATTING))
		return;

	AfkTimer();

	m_NumInputs++;

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnPredictedInput(pNewInput);

	// Magic number when we can hope that client has successfully identified itself
	if(m_NumInputs == 20 && g_Config.m_SvClientSuggestion[0] != '\0' && GetClientVersion() <= VERSION_DDNET_OLD)
		GameServer()->SendBroadcast(g_Config.m_SvClientSuggestion, m_ClientID);
	else if(m_NumInputs == 200 && Server()->IsSixup(m_ClientID))
		GameServer()->SendBroadcast("This server uses an experimental translation from Teeworlds 0.7 to 0.6. Please report bugs on ddnet.org/discord", m_ClientID);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	Server()->SetClientFlags(m_ClientID, pNewInput->m_PlayerFlags);

	AfkTimer();

	if(((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && m_SpectatorID == SPEC_FREEVIEW)
		m_ViewPos = vec2(pNewInput->m_TargetX, pNewInput->m_TargetY);

	// check for activity
	if(mem_comp(pNewInput, m_pLastTarget, sizeof(CNetObj_PlayerInput)))
	{
		mem_copy(m_pLastTarget, pNewInput, sizeof(CNetObj_PlayerInput));
		// Ignore the first direct input and keep the player afk as it is sent automatically
		if(m_LastTargetInit)
			UpdatePlaytime();
		m_LastActionTick = Server()->Tick();
		m_LastTargetInit = true;
	}
}

void CPlayer::OnPredictedEarlyInput(CNetObj_PlayerInput *pNewInput)
{
	m_PlayerFlags = pNewInput->m_PlayerFlags;

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (pNewInput->m_Fire & 1))
		m_Spawning = true;

	// skip the input if chat is active
	if(m_PlayerFlags & PLAYERFLAG_CHATTING)
		return;

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnDirectInput(pNewInput);
}

int CPlayer::GetClientVersion() const
{
	return m_pGameServer->GetClientVersion(m_ClientID);
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);

		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn(bool WeakHook)
{
	if(m_Team != TEAM_SPECTATORS)
	{
		m_WeakHookSpawn = WeakHook;
		m_Spawning = true;
	}
}

CCharacter *CPlayer::ForceSpawn(vec2 Pos)
{
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World, GameServer()->GetLastPlayerInput(m_ClientID));
	m_pCharacter->Spawn(this, Pos);
	m_Team = 0;
	return m_pCharacter;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	KillCharacter();

	m_Team = Team;
	m_LastSetTeam = Server()->Tick();
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;

	protocol7::CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = m_ClientID;
	Msg.m_Team = m_Team;
	Msg.m_Silent = !DoChatMsg;
	Msg.m_CooldownTick = m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(pPlayer && pPlayer->m_SpectatorID == m_ClientID)
				pPlayer->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

bool CPlayer::SetTimerType(int TimerType)
{
	if(TimerType == TIMERTYPE_DEFAULT)
	{
		if(Server()->IsSixup(m_ClientID))
			m_TimerType = TIMERTYPE_SIXUP;
		else
			SetTimerType(g_Config.m_SvDefaultTimerType);

		return true;
	}

	if(Server()->IsSixup(m_ClientID))
	{
		if(TimerType == TIMERTYPE_SIXUP || TimerType == TIMERTYPE_NONE)
		{
			m_TimerType = TimerType;
			return true;
		}
		else
			return false;
	}

	if(TimerType == TIMERTYPE_GAMETIMER)
	{
		if(GetClientVersion() >= VERSION_DDNET_GAMETICK)
			m_TimerType = TimerType;
		else
			return false;
	}
	else if(TimerType == TIMERTYPE_GAMETIMER_AND_BROADCAST)
	{
		if(GetClientVersion() >= VERSION_DDNET_GAMETICK)
			m_TimerType = TimerType;
		else
		{
			m_TimerType = TIMERTYPE_BROADCAST;
			return false;
		}
	}
	else
		m_TimerType = TimerType;

	return true;
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos, GameServer()->GetDDRaceTeam(m_ClientID)))
		return;

	m_WeakHookSpawn = false;
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World, GameServer()->GetLastPlayerInput(m_ClientID));
	m_ViewPos = SpawnPos;
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientID));

	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
		m_pCharacter->SetSolo(true);
}

void CPlayer::UpdatePlaytime()
{
	m_LastPlaytime = time_get();
}

void CPlayer::AfkTimer()
{
	if(g_Config.m_SvMaxAfkTime == 0)
		return;

	if(m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkTime)
	{
		m_Afk = true;
		return;
	}

	m_Afk = false;
}

int CPlayer::GetDefaultEmote() const
{
	if(m_OverrideEmoteReset >= 0)
		return m_OverrideEmote;

	return m_DefEmote;
}

void CPlayer::OverrideDefaultEmote(int Emote, int Tick)
{
	m_OverrideEmote = Emote;
	m_OverrideEmoteReset = Tick;
	m_LastEyeEmote = Server()->Tick();
}

bool CPlayer::CanOverrideDefaultEmote() const
{
	return m_LastEyeEmote == 0 || m_LastEyeEmote + (int64_t)g_Config.m_SvEyeEmoteChangeDelay * Server()->TickSpeed() < Server()->Tick();
}

void CPlayer::ProcessPause()
{
	if(m_ForcePauseTime && m_ForcePauseTime < Server()->Tick())
	{
		m_ForcePauseTime = 0;
		Pause(PAUSE_NONE, true);
	}

	if(m_Paused == PAUSE_SPEC && !m_pCharacter->IsPaused() && m_pCharacter->IsGrounded() && m_pCharacter->m_Pos == m_pCharacter->m_PrevPos)
	{
		m_pCharacter->Pause(true);
		GameServer()->CreateDeath(m_pCharacter->m_Pos, m_ClientID, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientID));
		GameServer()->CreateSound(m_pCharacter->m_Pos, SOUND_PLAYER_DIE, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientID));
	}
}

int CPlayer::Pause(int State, bool Force)
{
	if(State < PAUSE_NONE || State > PAUSE_SPEC) // Invalid pause state passed
		return 0;

	if(!m_pCharacter)
		return 0;

	char aBuf[128];
	if(State != m_Paused)
	{
		// Get to wanted state
		switch(State)
		{
		case PAUSE_PAUSED:
		case PAUSE_NONE:
			if(m_pCharacter->IsPaused()) // First condition might be unnecessary
			{
				if(!Force && m_LastPause && m_LastPause + (int64_t)g_Config.m_SvSpecFrequency * Server()->TickSpeed() > Server()->Tick())
				{
					GameServer()->SendChatTarget(m_ClientID, "Can't /spec that quickly.");
					return m_Paused; // Do not update state. Do not collect $200
				}
				m_pCharacter->Pause(false);
				m_ViewPos = m_pCharacter->m_Pos;
				GameServer()->CreatePlayerSpawn(m_pCharacter->m_Pos, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientID));
			}
			[[fallthrough]];
		case PAUSE_SPEC:
			if(g_Config.m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), (State > PAUSE_NONE) ? "'%s' speced" : "'%s' resumed", Server()->ClientName(m_ClientID));
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			}
			break;
		}

		// Update state
		m_Paused = State;
		m_LastPause = Server()->Tick();

		// Sixup needs a teamchange
		protocol7::CNetMsg_Sv_Team Msg;
		Msg.m_ClientID = m_ClientID;
		Msg.m_CooldownTick = Server()->Tick();
		Msg.m_Silent = true;
		Msg.m_Team = m_Paused ? protocol7::TEAM_SPECTATORS : m_Team;

		GameServer()->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, m_ClientID);
	}

	return m_Paused;
}

int CPlayer::ForcePause(int Time)
{
	m_ForcePauseTime = Server()->Tick() + Server()->TickSpeed() * Time;

	if(g_Config.m_SvPauseMessages)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' was force-paused for %ds", Server()->ClientName(m_ClientID), Time);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	return Pause(PAUSE_SPEC, true);
}

int CPlayer::IsPaused()
{
	return m_ForcePauseTime ? m_ForcePauseTime : -1 * m_Paused;
}

bool CPlayer::IsPlaying()
{
	return m_pCharacter && m_pCharacter->IsAlive();
}

void CPlayer::SpectatePlayerName(const char *pName)
{
	if(!pName)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i != m_ClientID && Server()->ClientIngame(i) && !str_comp(pName, Server()->ClientName(i)))
		{
			m_SpectatorID = i;
			return;
		}
	}
}

void CPlayer::ProcessScoreResult(CScorePlayerResult &Result)
{
	if(Result.m_Success) // SQL request was successful
	{
		switch(Result.m_MessageKind)
		{
		case CScorePlayerResult::DIRECT:
			for(auto &aMessage : Result.m_Data.m_aaMessages)
			{
				if(aMessage[0] == 0)
					break;
				GameServer()->SendChatTarget(m_ClientID, aMessage);
			}
			break;
		case CScorePlayerResult::ALL:
		{
			bool PrimaryMessage = true;
			for(auto &aMessage : Result.m_Data.m_aaMessages)
			{
				if(aMessage[0] == 0)
					break;

				if(GameServer()->ProcessSpamProtection(m_ClientID) && PrimaryMessage)
					break;

				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aMessage, -1);
				PrimaryMessage = false;
			}
			break;
		}
		case CScorePlayerResult::BROADCAST:
			if(Result.m_Data.m_aBroadcast[0] != 0)
				GameServer()->SendBroadcast(Result.m_Data.m_aBroadcast, -1);
			break;
		case CScorePlayerResult::MAP_VOTE:
			GameServer()->m_VoteType = CGameContext::VOTE_TYPE_OPTION;
			GameServer()->m_LastMapVote = time_get();

			char aCmd[256];
			str_format(aCmd, sizeof(aCmd),
				"sv_reset_file types/%s/flexreset.cfg; change_map \"%s\"",
				Result.m_Data.m_MapVote.m_aServer, Result.m_Data.m_MapVote.m_aMap);

			char aChatmsg[512];
			str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)",
				Server()->ClientName(m_ClientID), Result.m_Data.m_MapVote.m_aMap, "/map");

			GameServer()->CallVote(m_ClientID, Result.m_Data.m_MapVote.m_aMap, aCmd, "/map", aChatmsg);
			break;
		case CScorePlayerResult::PLAYER_INFO:
			GameServer()->Score()->PlayerData(m_ClientID)->Set(Result.m_Data.m_Info.m_Time, Result.m_Data.m_Info.m_aTimeCp);
			m_Score = Result.m_Data.m_Info.m_Score;
			m_HasFinishScore = Result.m_Data.m_Info.m_HasFinishScore;
			// -9999 stands for no time and isn't displayed in scoreboard, so
			// shift the time by a second if the player actually took 9999
			// seconds to finish the map.
			if(m_HasFinishScore && m_Score == -9999)
				m_Score = -10000;
			Server()->ExpireServerInfo();
			int Birthday = Result.m_Data.m_Info.m_Birthday;
			if(Birthday != 0 && !m_BirthdayAnnounced)
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf),
					"Happy DDNet birthday to %s for finishing their first map %d year%s ago!",
					Server()->ClientName(m_ClientID), Birthday, Birthday > 1 ? "s" : "");
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, m_ClientID);
				str_format(aBuf, sizeof(aBuf),
					"Happy DDNet birthday, %s!\nYou have finished your first map exactly %d year%s ago!",
					Server()->ClientName(m_ClientID), Birthday, Birthday > 1 ? "s" : "");
				GameServer()->SendBroadcast(aBuf, m_ClientID);
				m_BirthdayAnnounced = true;
			}
			GameServer()->SendRecord(m_ClientID);
			break;
		}
	}
}
