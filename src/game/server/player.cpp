/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include "player.h"

#include <engine/server.h>
#include <engine/server/server.h>
#include "gamecontext.h"
#include <game/gamecore.h>
#include <game/version.h>
#include <game/server/teams.h>
#include "gamemodes/DDRace.h"
#include <stdio.h>
#include <time.h>


MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_pCharacter = 0;
	Reset();
}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Reset()
{
	m_RespawnTick = Server()->Tick();
	m_DieTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	if (m_pCharacter)
		delete m_pCharacter;
	m_pCharacter = 0;
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();

	int* idMap = Server()->GetIdMap(m_ClientID);
	for (int i = 1;i < VANILLA_MAX_CLIENTS;i++)
	{
		idMap[i] = -1;
	}
	idMap[0] = m_ClientID;

	// DDRace

	m_LastCommandPos = 0;
	m_LastPlaytime = time_get();
	m_Sent1stAfkWarning = 0;
	m_Sent2ndAfkWarning = 0;
	m_ChatScore = 0;
	m_EyeEmote = true;
	m_TimerType = g_Config.m_SvDefaultTimerType;
	m_DefEmote = EMOTE_NORMAL;
	m_Afk = false;
	m_LastWhisperTo = -1;
	m_LastSetSpectatorMode = 0;
	m_TimeoutCode[0] = '\0';

	m_TuneZone = 0;
	m_TuneZoneOld = m_TuneZone;
	m_Halloween = false;
	m_FirstPacket = true;

	if (g_Config.m_SvEvents)
	{
		time_t rawtime;
		struct tm* timeinfo;
		char d[16], m[16], y[16];
		int dd, mm;
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		strftime (d,sizeof(y),"%d",timeinfo);
		strftime (m,sizeof(m),"%m",timeinfo);
		strftime (y,sizeof(y),"%Y",timeinfo);
		dd = atoi(d);
		mm = atoi(m);
		if ((mm == 12 && dd == 31) || (mm == 1 && dd == 1))
		{ // New Year
			m_DefEmote = EMOTE_HAPPY;
		}
		else if ((mm == 10 && dd == 31) || (mm == 11 && dd == 1))
		{ // Halloween
			m_DefEmote = EMOTE_ANGRY;
			m_Halloween = true;
		}
		else
		{
			m_DefEmote = EMOTE_NORMAL;
		}
	}
	m_DefEmoteReset = -1;

	GameServer()->Score()->PlayerData(m_ClientID)->Reset();

	m_ClientVersion = VERSION_VANILLA;
	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowAll = g_Config.m_SvShowAllDefault;
	m_SpecTeam = 0;
	m_NinjaJetpack = false;

	m_Paused = PAUSED_NONE;
	m_DND = false;

	m_NextPauseTick = 0;

	// Variable initialized:
	m_Last_Team = 0;
#if defined(CONF_SQL)
	m_LastSQLQuery = 0;
#endif
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	if (m_ChatScore > 0)
		m_ChatScore--;

	if (m_ForcePauseTime > 0)
		m_ForcePauseTime--;

	Server()->SetClientScore(m_ClientID, m_Score);

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(((CServer *)Server())->m_NetServer.ErrorString(m_ClientID)[0])
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		((CServer *)(Server()))->m_NetServer.ResetErrorString(m_ClientID);
	}

	if(!GameServer()->m_World.m_Paused)
	{
		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x-m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y-m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				if(m_Paused >= PAUSED_FORCE)
				{
					if(m_ForcePauseTime == 0)
					m_Paused = PAUSED_NONE;
					ProcessPause();
				}
				else if(m_Paused == PAUSED_PAUSED && m_NextPauseTick < Server()->Tick())
				{
					if((!m_pCharacter->GetWeaponGot(WEAPON_NINJA) || m_pCharacter->m_FreezeTime) && m_pCharacter->IsGrounded() && m_pCharacter->m_Pos == m_pCharacter->m_PrevPos)
						ProcessPause();
				}
				else if(m_NextPauseTick < Server()->Tick())
				{
					ProcessPause();
				}
				if(!m_Paused)
					m_ViewPos = m_pCharacter->m_Pos;
			}
			else if(!m_pCharacter->IsPaused())
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && m_RespawnTick <= Server()->Tick())
			TryRespawn();
	}
	else
	{
		++m_RespawnTick;
		++m_DieTick;
		++m_ScoreStartTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
	}

	m_TuneZoneOld = m_TuneZone; // determine needed tunings with viewpos
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_ViewPos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (m_TuneZone != m_TuneZoneOld) // dont send tunigs all the time
	{
		GameServer()->SendTuningParams(m_ClientID, m_TuneZone);
	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if((m_Team == TEAM_SPECTATORS || m_Paused) && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID] && GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter())
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter()->m_Pos;
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	int id = m_ClientID;
	if (SnappingClient > -1 && !Server()->Translate(id, SnappingClient)) return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	if (m_StolenSkin && SnappingClient != m_ClientID && g_Config.m_SvSkinStealAction == 1)
	{
		StrToInts(&pClientInfo->m_Skin0, 6, "pinky");
		pClientInfo->m_UseCustomColor = 0;
		pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
		pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;
	} else
	{
		StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
		pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
		pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
		pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;
	}

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	pPlayerInfo->m_Local = 0;
	pPlayerInfo->m_ClientID = id;
	pPlayerInfo->m_Score = abs(m_Score) * -1;
	pPlayerInfo->m_Team = (m_Paused != PAUSED_SPEC || m_ClientID != SnappingClient) && m_Paused < PAUSED_PAUSED ? m_Team : TEAM_SPECTATORS;

	if(m_ClientID == SnappingClient)
		pPlayerInfo->m_Local = 1;

	if(m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused))
	{
		CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;

		pSpectatorInfo->m_SpectatorID = m_SpectatorID;
		pSpectatorInfo->m_X = m_ViewPos.x;
		pSpectatorInfo->m_Y = m_ViewPos.y;
	}

	// send 0 if times of others are not shown
	if(SnappingClient != m_ClientID && g_Config.m_SvHideScore)
		pPlayerInfo->m_Score = -9999;
	else
		pPlayerInfo->m_Score = abs(m_Score) * -1;
}

void CPlayer::FakeSnap(int SnappingClient)
{
	// This is problematic when it's sent before we know whether it's a non-64-player-client
	// Then we can't spectate players at the start
	IServer::CClientInfo info;
	Server()->GetClientInfo(SnappingClient, &info);
	CGameContext *GameContext = (CGameContext *) GameServer();
	if (SnappingClient > -1 && GameContext->m_apPlayers[SnappingClient] && GameContext->m_apPlayers[SnappingClient]->m_ClientVersion >= VERSION_DDNET_OLD)
		return;

	int id = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
}

void CPlayer::OnDisconnect(const char *pReason)
{
	KillCharacter();

	if(Server()->ClientIngame(m_ClientID))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(m_ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	AfkVoteTimer(NewInput);

	if(m_FirstPacket)
	{
		if(g_Config.m_SvClientSuggestion)
		{
			if(m_ClientVersion <= VERSION_DDNET_OLD)
				GameServer()->SendBroadcast(g_Config.m_SvClientSuggestionOther, m_ClientID);
			if(m_ClientVersion < CLIENT_VERSIONNR)
				GameServer()->SendBroadcast(g_Config.m_SvClientSuggestionOld, m_ClientID);
			m_FirstPacket = false;
		}
	}

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if (AfkTimer(NewInput->m_TargetX, NewInput->m_TargetY))
		return; // we must return if kicked, as player struct is already deleted
	AfkVoteTimer(NewInput);

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
	// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
		return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

	m_PlayerFlags = NewInput->m_PlayerFlags;
 		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
	{
		if(!m_Paused)
			m_pCharacter->OnDirectInput(NewInput);
		else
			m_pCharacter->ResetInput();
	}

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	if(((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && m_SpectatorID == SPEC_FREEVIEW)
		m_ViewPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
	}
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
		if (m_RespawnTick > Server()->Tick())
			return;

		m_pCharacter->Die(m_ClientID, Weapon);

		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn()
{
	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

CCharacter* CPlayer::ForceSpawn(vec2 Pos)
{
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, Pos);
	m_Team = 0;
	return m_pCharacter;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	// clamp the team
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	char aBuf[512];
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	if(Team == TEAM_SPECTATORS)
	{
		CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
		Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);
	}

	KillCharacter();

	m_Team = Team;
	m_LastSetTeam = Server()->Tick();
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;
	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	//GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
		return;

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;

	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));

	if(g_Config.m_SvTeam == 3)
	{
		int NewTeam = 0;
		for(; NewTeam < TEAM_SUPER; NewTeam++)
			if(Controller->m_Teams.Count(NewTeam) == 0)
				break;

		if(NewTeam == TEAM_SUPER)
			NewTeam = 0;

		Controller->m_Teams.SetForceCharacterTeam(GetCID(), NewTeam);
	}
}

bool CPlayer::AfkTimer(int NewTargetX, int NewTargetY)
{
	/*
		afk timer (x, y = mouse coordinates)
		Since a player has to move the mouse to play, this is a better method than checking
		the player's position in the game world, because it can easily be bypassed by just locking a key.
		Frozen players could be kicked as well, because they can't move.
		It also works for spectators.
		returns true if kicked
	*/

	if(m_Authed)
		return false; // don't kick admins
	if(g_Config.m_SvMaxAfkTime == 0)
		return false; // 0 = disabled

	if(NewTargetX != m_LastTarget_x || NewTargetY != m_LastTarget_y)
	{
		m_LastPlaytime = time_get();
		m_LastTarget_x = NewTargetX;
		m_LastTarget_y = NewTargetY;
		m_Sent1stAfkWarning = 0; // afk timer's 1st warning after 50% of sv_max_afk_time
		m_Sent2ndAfkWarning = 0;

	}
	else
	{
		if(!m_Paused)
		{
			// not playing, check how long
			if(m_Sent1stAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.5))
			{
				sprintf(
					m_pAfkMsg,
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime*0.5),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent1stAfkWarning = 1;
			}
			else if(m_Sent2ndAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.9))
			{
				sprintf(
					m_pAfkMsg,
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime*0.9),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent2ndAfkWarning = 1;
			}
			else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkTime)
			{
				CServer* serv =	(CServer*)m_pGameServer->Server();
				serv->Kick(m_ClientID,"Away from keyboard");
				return true;
			}
		}
	}
	return false;
}

void CPlayer::AfkVoteTimer(CNetObj_PlayerInput *NewTarget)
{
	if(g_Config.m_SvMaxAfkVoteTime == 0)
		return;

	if(mem_comp(NewTarget, &m_LastTarget, sizeof(CNetObj_PlayerInput)) != 0)
	{
		m_LastPlaytime = time_get();
		mem_copy(&m_LastTarget, NewTarget, sizeof(CNetObj_PlayerInput));
	}
	else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkVoteTime)
	{
		m_Afk = true;
		return;
	}

	m_Afk = false;
}

void CPlayer::ProcessPause()
{
	if(!m_pCharacter)
		return;

	char aBuf[128];
	if(m_Paused >= PAUSED_PAUSED)
	{
		if(!m_pCharacter->IsPaused())
		{
			m_pCharacter->Pause(true);
			if(g_Config.m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), (m_Paused == PAUSED_PAUSED) ? "'%s' paused" : "'%s' was force-paused for %ds", Server()->ClientName(m_ClientID), m_ForcePauseTime/Server()->TickSpeed());
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			}
			GameServer()->CreateDeath(m_pCharacter->m_Pos, m_ClientID, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
			GameServer()->CreateSound(m_pCharacter->m_Pos, SOUND_PLAYER_DIE, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
			m_NextPauseTick = Server()->Tick() + g_Config.m_SvPauseFrequency * Server()->TickSpeed();
		}
	}
	else
	{
		if(m_pCharacter->IsPaused())
		{
			m_pCharacter->Pause(false);
			if(g_Config.m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), "'%s' resumed", Server()->ClientName(m_ClientID));
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			}
			GameServer()->CreatePlayerSpawn(m_pCharacter->m_Pos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
			m_NextPauseTick = Server()->Tick() + g_Config.m_SvPauseFrequency * Server()->TickSpeed();
		}
	}
}

bool CPlayer::IsPlaying()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return true;
	return false;
}

void CPlayer::FindDuplicateSkins()
{
	if (m_TeeInfos.m_UseCustomColor == 0 && !m_StolenSkin) return;
	m_StolenSkin = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (i == m_ClientID) continue;
		if(GameServer()->m_apPlayers[i])
		{
			if (GameServer()->m_apPlayers[i]->m_StolenSkin) continue;
			if ((GameServer()->m_apPlayers[i]->m_TeeInfos.m_UseCustomColor == m_TeeInfos.m_UseCustomColor) &&
			(GameServer()->m_apPlayers[i]->m_TeeInfos.m_ColorFeet == m_TeeInfos.m_ColorFeet) &&
			(GameServer()->m_apPlayers[i]->m_TeeInfos.m_ColorBody == m_TeeInfos.m_ColorBody) &&
			!str_comp(GameServer()->m_apPlayers[i]->m_TeeInfos.m_SkinName, m_TeeInfos.m_SkinName))
			{
				m_StolenSkin = 1;
				return;
			}
		}
	}
}
