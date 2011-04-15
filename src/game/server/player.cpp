/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include "player.h"

#include <engine/server.h>
#include <engine/server/server.h>
#include "gamecontext.h"
#include <game/gamecore.h>
#include "gamemodes/DDRace.h"
#include <stdio.h>


MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_RespawnTick = Server()->Tick();
	m_DieTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	Character = 0;
	this->m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();

	// DDRace

	m_LastPlaytime = time_get();
	m_LastTarget_x = 0;
	m_LastTarget_y = 0;
	m_SentAfkWarning = 0;
	m_SentAfkWarning2 = 0;
	m_ChatScore = 0;
	m_PauseInfo.m_Respawn = false;

	GameServer()->Score()->PlayerData(ClientID)->Reset();

	m_IsUsingDDRaceClient = false;
	m_ShowOthers = false;

	// Variable initialized:
	m_Last_Pause = 0;
	m_Last_Team = 0;
}

CPlayer::~CPlayer()
{
	delete Character;
	Character = 0;
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

	if(!Character && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick())
		m_Spawning = true;

	if(Character)
	{
		if(Character->IsAlive())
		{
			m_ViewPos = Character->m_Pos;
		}
		else
		{
			delete Character;
			Character = 0;
		}
	}
	else if(m_Spawning && m_RespawnTick <= Server()->Tick())
		TryRespawn();
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
	if(m_Team == TEAM_SPECTATORS && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID])
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->m_ViewPos;
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, m_ClientID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
	pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	pPlayerInfo->m_Local = 0;
	pPlayerInfo->m_ClientID = m_ClientID;
	pPlayerInfo->m_Score = m_Score;
	pPlayerInfo->m_Team = m_Team;

	if(m_ClientID == SnappingClient)
		pPlayerInfo->m_Local = 1;

	if(m_ClientID == SnappingClient && m_Team == TEAM_SPECTATORS)
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
		pPlayerInfo->m_Score = 0;
	else
		pPlayerInfo->m_Score = m_Score;

	pPlayerInfo->m_Team = m_Team;
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
	Controller->m_Teams.m_Core.Team(m_ClientID, 0);
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	if(Character)
		Character->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(Character)
		Character->OnDirectInput(NewInput);

	if(!Character && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	if(!Character && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
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
	if(Character && Character->IsAlive())
		return Character;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(Character)
	{
		Character->Die(m_ClientID, Weapon);
		delete Character;
		Character = 0;
	}
}

void CPlayer::Respawn()
{
	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

void CPlayer::SetTeam(int Team)
{
	// clamp the team
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
	GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	KillCharacter();

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	//str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	//GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

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
	if(m_PauseInfo.m_Respawn)
	{
		Character = new(m_ClientID) CCharacter(&GameServer()->m_World);
		Character->Spawn(this, m_PauseInfo.m_Core.m_Pos);
		GameServer()->CreatePlayerSpawn(m_PauseInfo.m_Core.m_Pos, ((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.TeamMask((m_PauseInfo.m_Team > 0 && m_PauseInfo.m_Team < TEAM_SUPER) ? m_PauseInfo.m_Team : 0));
		LoadCharacter();
	}
	else
	{
		vec2 SpawnPos;

		if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
			return;

		m_Spawning = false;
		Character = new(m_ClientID) CCharacter(&GameServer()->m_World);
		Character->Spawn(this, SpawnPos);
		GameServer()->CreatePlayerSpawn(SpawnPos);
	}
}

void CPlayer::LoadCharacter()
{
	Character->SetCore(m_PauseInfo.m_Core);
	if(g_Config.m_SvPauseTime)
		Character->m_StartTime = Server()->Tick() - (m_PauseInfo.m_PauseTime - m_PauseInfo.m_StartTime);
	else
		Character->m_StartTime = m_PauseInfo.m_StartTime;
	Character->m_DDRaceState = m_PauseInfo.m_DDRaceState;
	Character->m_RefreshTime = Server()->Tick();
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		if(m_PauseInfo.m_aHasWeapon[i])
		{
			if(!m_PauseInfo.m_FreezeTime)
				Character->GiveWeapon(i, -1);
			else
				Character->GiveWeapon(i, 0);
		}
	}
	Character->m_FreezeTime = m_PauseInfo.m_FreezeTime;
	Character->SetLastAction(Server()->Tick());
	Character->SetArmor(m_PauseInfo.m_Armor);
	Character->m_LastMove = m_PauseInfo.m_LastMove;
	Character->m_PrevPos = m_PauseInfo.m_PrevPos;
	Character->SetActiveWeapon(m_PauseInfo.m_ActiveWeapon);
	Character->SetLastWeapon(m_PauseInfo.m_LastWeapon);
	Character->m_Super = m_PauseInfo.m_Super;
	Character->m_DeepFreeze = m_PauseInfo.m_DeepFreeze;
	Character->m_EndlessHook = m_PauseInfo.m_EndlessHook;
	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	Controller->m_Teams.m_Core.Team(GetCID(), m_PauseInfo.m_Team);
	m_PauseInfo.m_Respawn = false;
	m_InfoSaved = false;
}

void CPlayer::SaveCharacter()
{
	m_PauseInfo.m_Core = Character->GetCore();
	m_PauseInfo.m_StartTime = Character->m_StartTime;
	m_PauseInfo.m_DDRaceState = Character->m_DDRaceState;
	for(int i = 0; i < WEAPON_NINJA; ++i)
	{
		m_PauseInfo.m_aHasWeapon[i] = Character->GetWeaponGot(i);
	}
	m_PauseInfo.m_FreezeTime=Character->m_FreezeTime;
	m_PauseInfo.m_Armor = Character->GetArmor();
	m_PauseInfo.m_LastMove = Character->m_LastMove;
	m_PauseInfo.m_PrevPos = Character->m_PrevPos;
	m_PauseInfo.m_ActiveWeapon = Character->GetActiveWeapon();
	m_PauseInfo.m_LastWeapon = Character->GetLastWeapon();
	m_PauseInfo.m_Super = Character->m_Super;
	m_PauseInfo.m_DeepFreeze = Character->m_DeepFreeze;
	m_PauseInfo.m_EndlessHook = Character->m_EndlessHook;
	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	m_PauseInfo.m_Team = Controller->m_Teams.m_Core.Team(GetCID());
	m_PauseInfo.m_PauseTime = Server()->Tick();
	//m_PauseInfo.m_RefreshTime = Character->m_RefreshTime;
}

void CPlayer::AfkTimer(int new_target_x, int new_target_y)
{
	/*
		afk timer (x, y = mouse coordinates)
		Since a player has to move the mouse to play, this is a better method than checking
		the player's position in the game world, because it can easily be bypassed by just locking a key.
		Frozen players could be kicked as well, because they can't move.
		It also works for spectators.
	*/

	if(m_Authed) return; // don't kick admins
	if(g_Config.m_SvMaxAfkTime == 0) return; // 0 = disabled

	if(new_target_x != m_LastTarget_x || new_target_y != m_LastTarget_y)
	{
		m_LastPlaytime = time_get();
		m_LastTarget_x = new_target_x;
		m_LastTarget_y = new_target_y;
		m_SentAfkWarning = 0; // afk timer's 1st warning after 50% of sv_max_afk_time
		m_SentAfkWarning2 = 0;

	}
	else
	{
		// not playing, check how long
		if(m_SentAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.5))
		{
			sprintf(
				m_pAfkMsg,
				"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
				(int)(g_Config.m_SvMaxAfkTime*0.5),
				g_Config.m_SvMaxAfkTime
			);
			m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
			m_SentAfkWarning = 1;
		} else if(m_SentAfkWarning2 == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.9))
		{
			sprintf(
				m_pAfkMsg,
				"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
				(int)(g_Config.m_SvMaxAfkTime*0.9),
				g_Config.m_SvMaxAfkTime
			);
			m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
			m_SentAfkWarning = 1;
		} else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkTime)
		{
			CServer* serv =	(CServer*)m_pGameServer->Server();
			serv->Kick(m_ClientID,"Away from keyboard");
		}
	}
}
