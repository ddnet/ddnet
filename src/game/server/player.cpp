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

CPlayer::CPlayer(CGameContext *pGameServer, uint32_t UniqueClientId, int ClientId, int Team) :
	m_UniqueClientId(UniqueClientId)
{
	m_pGameServer = pGameServer;
	m_ClientId = ClientId;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_NumInputs = 0;
	Reset();
	GameServer()->Antibot()->OnPlayerInit(m_ClientId);
}

CPlayer::~CPlayer()
{
	GameServer()->Antibot()->OnPlayerDestroy(m_ClientId);
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
	m_SpectatorId = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_LastSetTeam = 0;
	m_LastInvited = 0;
	m_WeakHookSpawn = false;

	int *pIdMap = Server()->GetIdMap(m_ClientId);
	for(int i = 1; i < VANILLA_MAX_CLIENTS; i++)
	{
		pIdMap[i] = -1;
	}
	pIdMap[0] = m_ClientId;

	// DDRace

	m_LastCommandPos = 0;
	m_LastPlaytime = 0;
	m_ChatScore = 0;
	m_Moderating = false;
	m_EyeEmoteEnabled = true;
	if(Server()->IsSixup(m_ClientId))
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
		const ETimeSeason Season = time_season();
		if(Season == SEASON_NEWYEAR)
		{
			m_DefEmote = EMOTE_HAPPY;
		}
		else if(Season == SEASON_HALLOWEEN)
		{
			m_DefEmote = EMOTE_ANGRY;
			m_Halloween = true;
		}
		else
		{
			m_DefEmote = EMOTE_NORMAL;
		}
	}
	m_OverrideEmoteReset = -1;

	GameServer()->Score()->PlayerData(m_ClientId)->Reset();

	m_Last_KickVote = 0;
	m_Last_Team = 0;
	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowAll = g_Config.m_SvShowAllDefault;
	m_ShowDistance = vec2(1200, 800);
	m_SpecTeam = false;
	m_NinjaJetpack = false;

	m_Paused = PAUSE_NONE;
	m_DND = false;
	m_Whispers = true;

	m_LastPause = 0;
	m_Score.reset();

	// Variable initialized:
	m_Last_Team = 0;
	m_LastSqlQuery = 0;
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
	m_SwapTargetsClientId = -1;
	m_BirthdayAnnounced = false;
	m_RescueMode = RESCUEMODE_AUTO;

	m_CameraInfo.Reset();
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
	if(m_ScoreQueryResult != nullptr && m_ScoreQueryResult->m_Completed && m_SentSnaps >= 3)
	{
		ProcessScoreResult(*m_ScoreQueryResult);
		m_ScoreQueryResult = nullptr;
	}
	if(m_ScoreFinishResult != nullptr && m_ScoreFinishResult->m_Completed)
	{
		ProcessScoreResult(*m_ScoreFinishResult);
		m_ScoreFinishResult = nullptr;
	}

	if(!Server()->ClientIngame(m_ClientId))
		return;

	if(m_ChatScore > 0)
		m_ChatScore--;

	Server()->SetClientScore(m_ClientId, m_Score);

	if(m_Moderating && m_Afk)
	{
		m_Moderating = false;
		GameServer()->SendChatTarget(m_ClientId, "Active moderator mode disabled because you are afk.");

		if(!GameServer()->PlayerModerating())
			GameServer()->SendChat(-1, TEAM_ALL, "Server kick/spec votes are no longer actively moderated.");
	}

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientId, &Info))
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

	if(Server()->GetNetErrorString(m_ClientId)[0])
	{
		SetInitialAfk(true);

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientId));
		GameServer()->SendChat(-1, TEAM_ALL, aBuf);
		Server()->ResetNetErrorString(m_ClientId);
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
		GameServer()->SendTuningParams(m_ClientId, m_TuneZone);
	}

	if(m_OverrideEmoteReset >= 0 && m_OverrideEmoteReset <= Server()->Tick())
	{
		m_OverrideEmoteReset = -1;
	}

	if(m_Halloween && m_pCharacter && !m_pCharacter->IsPaused())
	{
		if(1200 - ((Server()->Tick() - m_pCharacter->GetLastAction()) % (1200)) < 5)
		{
			GameServer()->SendEmoticon(GetCid(), EMOTICON_GHOST, -1);
		}
	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags & PLAYERFLAG_IN_MENU)
		m_aCurLatency[m_ClientId] = GameServer()->m_apPlayers[m_ClientId]->m_Latency.m_Min;

	if(m_PlayerFlags & PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aCurLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if((m_Team == TEAM_SPECTATORS || m_Paused) && m_SpectatorId != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorId] && GameServer()->m_apPlayers[m_SpectatorId]->GetCharacter())
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorId]->GetCharacter()->m_Pos;
}

void CPlayer::PostPostTick()
{
	if(!Server()->ClientIngame(m_ClientId))
		return;

	if(!GameServer()->m_World.m_Paused && !m_pCharacter && m_Spawning && m_WeakHookSpawn)
		TryRespawn();
}

void CPlayer::Snap(int SnappingClient)
{
	if(!Server()->ClientIngame(m_ClientId))
		return;

	int id = m_ClientId;
	if(!Server()->Translate(id, SnappingClient))
		return;

	CNetObj_ClientInfo *pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(id);
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientId));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientId));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientId);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_aSkinName);
	pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	int Latency = SnappingClient == SERVER_DEMO_CLIENT ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aCurLatency[m_ClientId];

	int Score;
	// This is the time sent to the player while ingame (do not confuse to the one reported to the master server).
	// Due to clients expecting this as a negative value, we have to make sure it's negative.
	// Special numbers:
	// -9999: means no time and isn't displayed in the scoreboard.
	if(m_Score.has_value())
	{
		// shift the time by a second if the player actually took 9999
		// seconds to finish the map.
		if(m_Score.value() == 9999)
			Score = -10000;
		else
			Score = -m_Score.value();
	}
	else
	{
		Score = -9999;
	}

	// send 0 if times of others are not shown
	if(SnappingClient != m_ClientId && g_Config.m_SvHideScore)
		Score = -9999;

	if(!Server()->IsSixup(SnappingClient))
	{
		CNetObj_PlayerInfo *pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(id);
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Local = (int)(m_ClientId == SnappingClient && (m_Paused != PAUSE_PAUSED || SnappingClientVersion >= VERSION_DDNET_OLD));
		pPlayerInfo->m_ClientId = id;
		pPlayerInfo->m_Team = m_Team;
		if(SnappingClientVersion < VERSION_DDNET_INDEPENDENT_SPECTATORS_TEAM)
		{
			// In older versions the SPECTATORS TEAM was also used if the own player is in PAUSE_PAUSED or if any player is in PAUSE_SPEC.
			pPlayerInfo->m_Team = (m_Paused != PAUSE_PAUSED || m_ClientId != SnappingClient) && m_Paused < PAUSE_SPEC ? m_Team : TEAM_SPECTATORS;
		}
	}
	else
	{
		protocol7::CNetObj_PlayerInfo *pPlayerInfo = Server()->SnapNewItem<protocol7::CNetObj_PlayerInfo>(id);
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_PlayerFlags = PlayerFlags_SixToSeven(m_PlayerFlags);
		if(SnappingClientVersion >= VERSION_DDRACE && (m_PlayerFlags & PLAYERFLAG_AIM))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_AIM;
		if(Server()->GetAuthedState(m_ClientId) != AUTHED_NO)
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_ADMIN;

		// Times are in milliseconds for 0.7
		pPlayerInfo->m_Score = m_Score.has_value() ? GameServer()->Score()->PlayerData(m_ClientId)->m_BestTime * 1000 : -1;
		pPlayerInfo->m_Latency = Latency;
	}

	if(m_ClientId == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused))
	{
		if(!Server()->IsSixup(SnappingClient))
		{
			CNetObj_SpectatorInfo *pSpectatorInfo = Server()->SnapNewItem<CNetObj_SpectatorInfo>(m_ClientId);
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpectatorId = m_SpectatorId;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
		else
		{
			protocol7::CNetObj_SpectatorInfo *pSpectatorInfo = Server()->SnapNewItem<protocol7::CNetObj_SpectatorInfo>(m_ClientId);
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpecMode = m_SpectatorId == SPEC_FREEVIEW ? protocol7::SPEC_FREEVIEW : protocol7::SPEC_PLAYER;
			pSpectatorInfo->m_SpectatorId = m_SpectatorId;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
	}

	CNetObj_DDNetPlayer *pDDNetPlayer = Server()->SnapNewItem<CNetObj_DDNetPlayer>(id);
	if(!pDDNetPlayer)
		return;

	pDDNetPlayer->m_AuthLevel = Server()->GetAuthedState(m_ClientId);
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
		protocol7::CNetObj_PlayerInfoRace *pRaceInfo = Server()->SnapNewItem<protocol7::CNetObj_PlayerInfoRace>(id);
		if(!pRaceInfo)
			return;
		pRaceInfo->m_RaceStartTick = m_pCharacter->m_StartTime;
	}

	bool ShowSpec = m_pCharacter && m_pCharacter->IsPaused() && m_pCharacter->CanSnapCharacter(SnappingClient);

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		ShowSpec = ShowSpec && (GameServer()->GetDDRaceTeam(m_ClientId) == GameServer()->GetDDRaceTeam(SnappingClient) || pSnapPlayer->m_ShowOthers == SHOW_OTHERS_ON || (pSnapPlayer->GetTeam() == TEAM_SPECTATORS || pSnapPlayer->IsPaused()));
	}

	if(ShowSpec)
	{
		CNetObj_SpecChar *pSpecChar = Server()->SnapNewItem<CNetObj_SpecChar>(id);
		if(!pSpecChar)
			return;

		pSpecChar->m_X = m_pCharacter->Core()->m_Pos.x;
		pSpecChar->m_Y = m_pCharacter->Core()->m_Pos.y;
	}
}

void CPlayer::FakeSnap()
{
	m_SentSnaps++;
	if(GetClientVersion() >= VERSION_DDNET_OLD)
		return;

	if(Server()->IsSixup(m_ClientId))
		return;

	int FakeId = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(FakeId);

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, "");
	StrToInts(&pClientInfo->m_Skin0, 6, "default");

	if(m_Paused != PAUSE_PAUSED)
		return;

	CNetObj_PlayerInfo *pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(FakeId);
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = m_Latency.m_Min;
	pPlayerInfo->m_Local = 1;
	pPlayerInfo->m_ClientId = FakeId;
	pPlayerInfo->m_Score = -9999;
	pPlayerInfo->m_Team = TEAM_SPECTATORS;

	CNetObj_SpectatorInfo *pSpectatorInfo = Server()->SnapNewItem<CNetObj_SpectatorInfo>(FakeId);
	if(!pSpectatorInfo)
		return;

	pSpectatorInfo->m_SpectatorId = m_SpectatorId;
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

	if(m_pCharacter && !m_Paused && !(pNewInput->m_PlayerFlags & PLAYERFLAG_SPEC_CAM))
		m_pCharacter->OnPredictedInput(pNewInput);

	// Magic number when we can hope that client has successfully identified itself
	if(m_NumInputs == 20 && g_Config.m_SvClientSuggestion[0] != '\0' && GetClientVersion() <= VERSION_DDNET_OLD)
		GameServer()->SendBroadcast(g_Config.m_SvClientSuggestion, m_ClientId);
	else if(m_NumInputs == 200 && Server()->IsSixup(m_ClientId))
		GameServer()->SendBroadcast("This server uses an experimental translation from Teeworlds 0.7 to 0.6. Please report bugs on ddnet.org/discord", m_ClientId);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	Server()->SetClientFlags(m_ClientId, pNewInput->m_PlayerFlags);

	AfkTimer();

	if(((pNewInput->m_PlayerFlags & PLAYERFLAG_SPEC_CAM) || GetClientVersion() < VERSION_DDNET_PLAYERFLAG_SPEC_CAM) && ((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && m_SpectatorId == SPEC_FREEVIEW)
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

	if(m_pCharacter && !m_Paused && !(m_PlayerFlags & PLAYERFLAG_SPEC_CAM))
		m_pCharacter->OnDirectInput(pNewInput);
}

int CPlayer::GetClientVersion() const
{
	return m_pGameServer->GetClientVersion(m_ClientId);
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

const CCharacter *CPlayer::GetCharacter() const
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon, bool SendKillMsg)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientId, Weapon, SendKillMsg);

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
	m_pCharacter = new(m_ClientId) CCharacter(&GameServer()->m_World, GameServer()->GetLastPlayerInput(m_ClientId));
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
	m_SpectatorId = SPEC_FREEVIEW;

	protocol7::CNetMsg_Sv_Team Msg;
	Msg.m_ClientId = m_ClientId;
	Msg.m_Team = m_Team;
	Msg.m_Silent = !DoChatMsg;
	Msg.m_CooldownTick = m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(pPlayer && pPlayer->m_SpectatorId == m_ClientId)
				pPlayer->m_SpectatorId = SPEC_FREEVIEW;
		}
	}

	Server()->ExpireServerInfo();
}

bool CPlayer::SetTimerType(int TimerType)
{
	if(TimerType == TIMERTYPE_DEFAULT)
	{
		if(Server()->IsSixup(m_ClientId))
			m_TimerType = TIMERTYPE_SIXUP;
		else
			SetTimerType(g_Config.m_SvDefaultTimerType);

		return true;
	}

	if(Server()->IsSixup(m_ClientId))
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

	if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos, GameServer()->GetDDRaceTeam(m_ClientId)))
		return;

	m_WeakHookSpawn = false;
	m_Spawning = false;
	m_pCharacter = new(m_ClientId) CCharacter(&GameServer()->m_World, GameServer()->GetLastPlayerInput(m_ClientId));
	m_ViewPos = SpawnPos;
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientId));

	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
		m_pCharacter->SetSolo(true);
}

void CPlayer::UpdatePlaytime()
{
	m_LastPlaytime = time_get();
}

void CPlayer::AfkTimer()
{
	SetAfk(g_Config.m_SvMaxAfkTime != 0 && m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkTime);
}

void CPlayer::SetAfk(bool Afk)
{
	if(m_Afk != Afk)
	{
		Server()->ExpireServerInfo();
		m_Afk = Afk;
	}
}

void CPlayer::SetInitialAfk(bool Afk)
{
	if(g_Config.m_SvMaxAfkTime == 0)
	{
		SetAfk(false);
		return;
	}

	SetAfk(Afk);

	// Ensure that the AFK state is not reset again automatically
	if(Afk)
		m_LastPlaytime = time_get() - time_freq() * g_Config.m_SvMaxAfkTime - 1;
	else
		m_LastPlaytime = time_get();
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

bool CPlayer::CanSpec() const
{
	return m_pCharacter->IsGrounded() && m_pCharacter->m_Pos == m_pCharacter->m_PrevPos;
}

void CPlayer::ProcessPause()
{
	if(m_ForcePauseTime && m_ForcePauseTime < Server()->Tick())
	{
		m_ForcePauseTime = 0;
		Pause(PAUSE_NONE, true);
	}

	if(m_Paused == PAUSE_SPEC && !m_pCharacter->IsPaused() && CanSpec())
	{
		m_pCharacter->Pause(true);
		GameServer()->CreateDeath(m_pCharacter->m_Pos, m_ClientId, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientId));
		GameServer()->CreateSound(m_pCharacter->m_Pos, SOUND_PLAYER_DIE, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientId));
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
					GameServer()->SendChatTarget(m_ClientId, "Can't /spec that quickly.");
					return m_Paused; // Do not update state. Do not collect $200
				}
				m_pCharacter->Pause(false);
				m_ViewPos = m_pCharacter->m_Pos;
				GameServer()->CreatePlayerSpawn(m_pCharacter->m_Pos, GameServer()->m_pController->GetMaskForPlayerWorldEvent(m_ClientId));
			}
			[[fallthrough]];
		case PAUSE_SPEC:
			if(g_Config.m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), (State > PAUSE_NONE) ? "'%s' speced" : "'%s' resumed", Server()->ClientName(m_ClientId));
				GameServer()->SendChat(-1, TEAM_ALL, aBuf);
			}
			break;
		}

		// Update state
		m_Paused = State;
		m_LastPause = Server()->Tick();

		// Sixup needs a teamchange
		protocol7::CNetMsg_Sv_Team Msg;
		Msg.m_ClientId = m_ClientId;
		Msg.m_CooldownTick = Server()->Tick();
		Msg.m_Silent = true;
		Msg.m_Team = m_Paused ? protocol7::TEAM_SPECTATORS : m_Team;

		GameServer()->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, m_ClientId);
	}

	return m_Paused;
}

int CPlayer::ForcePause(int Time)
{
	m_ForcePauseTime = Server()->Tick() + Server()->TickSpeed() * Time;

	if(g_Config.m_SvPauseMessages)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' was force-paused for %ds", Server()->ClientName(m_ClientId), Time);
		GameServer()->SendChat(-1, TEAM_ALL, aBuf);
	}

	return Pause(PAUSE_SPEC, true);
}

int CPlayer::IsPaused() const
{
	return m_ForcePauseTime ? m_ForcePauseTime : -1 * m_Paused;
}

bool CPlayer::IsPlaying() const
{
	return m_pCharacter && m_pCharacter->IsAlive();
}

void CPlayer::SpectatePlayerName(const char *pName)
{
	if(!pName)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i != m_ClientId && Server()->ClientIngame(i) && !str_comp(pName, Server()->ClientName(i)))
		{
			m_SpectatorId = i;
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
				GameServer()->SendChatTarget(m_ClientId, aMessage);
			}
			break;
		case CScorePlayerResult::ALL:
		{
			bool PrimaryMessage = true;
			for(auto &aMessage : Result.m_Data.m_aaMessages)
			{
				if(aMessage[0] == 0)
					break;

				if(GameServer()->ProcessSpamProtection(m_ClientId) && PrimaryMessage)
					break;

				GameServer()->SendChat(-1, TEAM_ALL, aMessage, -1);
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
				Server()->ClientName(m_ClientId), Result.m_Data.m_MapVote.m_aMap, "/map");

			GameServer()->CallVote(m_ClientId, Result.m_Data.m_MapVote.m_aMap, aCmd, "/map", aChatmsg);
			break;
		case CScorePlayerResult::PLAYER_INFO:
		{
			if(Result.m_Data.m_Info.m_Time.has_value())
			{
				GameServer()->Score()->PlayerData(m_ClientId)->Set(Result.m_Data.m_Info.m_Time.value(), Result.m_Data.m_Info.m_aTimeCp);
				m_Score = Result.m_Data.m_Info.m_Time;
			}
			Server()->ExpireServerInfo();
			int Birthday = Result.m_Data.m_Info.m_Birthday;
			if(Birthday != 0 && !m_BirthdayAnnounced && GetCharacter())
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf),
					"Happy DDNet birthday to %s for finishing their first map %d year%s ago!",
					Server()->ClientName(m_ClientId), Birthday, Birthday > 1 ? "s" : "");
				GameServer()->SendChat(-1, TEAM_ALL, aBuf, m_ClientId);
				str_format(aBuf, sizeof(aBuf),
					"Happy DDNet birthday, %s!\nYou have finished your first map exactly %d year%s ago!",
					Server()->ClientName(m_ClientId), Birthday, Birthday > 1 ? "s" : "");
				GameServer()->SendBroadcast(aBuf, m_ClientId);
				m_BirthdayAnnounced = true;

				GameServer()->CreateBirthdayEffect(GetCharacter()->m_Pos, GetCharacter()->TeamMask());
			}
			GameServer()->SendRecord(m_ClientId);
			break;
		}
		case CScorePlayerResult::PLAYER_TIMECP:
			GameServer()->Score()->PlayerData(m_ClientId)->SetBestTimeCp(Result.m_Data.m_Info.m_aTimeCp);
			char aBuf[128], aTime[32];
			str_time_float(Result.m_Data.m_Info.m_Time.value(), TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "Showing the checkpoint times for '%s' with a race time of %s", Result.m_Data.m_Info.m_aRequestedPlayer, aTime);
			GameServer()->SendChatTarget(m_ClientId, aBuf);
			break;
		}
	}
}

vec2 CPlayer::CCameraInfo::ConvertTargetToWorld(vec2 Position, vec2 Target) const
{
	vec2 TargetCameraOffset(0, 0);
	float l = length(Target);

	if(l > 0.0001f) // make sure that this isn't 0
	{
		float OffsetAmount = maximum(l - m_Deadzone, 0.0f) * (m_FollowFactor / 100.0f);
		TargetCameraOffset = normalize_pre_length(Target, l) * OffsetAmount;
	}

	return Position + (Target - TargetCameraOffset) * m_Zoom + TargetCameraOffset;
}

void CPlayer::CCameraInfo::Write(const CNetMsg_Cl_CameraInfo *Msg)
{
	m_Zoom = Msg->m_Zoom / 1000.0f;
	m_Deadzone = Msg->m_Deadzone;
	m_FollowFactor = Msg->m_FollowFactor;
}

void CPlayer::CCameraInfo::Reset()
{
	m_Zoom = 1.0f;
	m_Deadzone = 0.0f;
	m_FollowFactor = 0.0f;
}
