// ddnet-insta specific gamecontroller methods
#include <engine/shared/config.h>

#include <engine/shared/protocolglue.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/teamscore.h>

#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/entities/dragger.h>
#include <game/server/entities/gun.h>
#include <game/server/entities/light.h>
#include <game/server/entities/pickup.h>
#include <game/server/entities/projectile.h>

#include <game/server/gamecontroller.h>

// ddnet-insta

int IGameController::GetPlayerTeam(CPlayer *pPlayer, bool Sixup)
{
	return pPlayer->GetTeam();
}

void IGameController::ToggleGamePause()
{
	SetPlayersReadyState(false);
	if(GameServer()->m_World.m_Paused)
		SetGameState(IGS_GAME_RUNNING);
	else
		SetGameState(IGS_GAME_PAUSED, TIMER_INFINITE);
}

bool IGameController::IsPlayerReadyMode()
{
	return Config()->m_SvPlayerReadyMode != 0 && (m_GameStateTimer == TIMER_INFINITE && (m_GameState == IGS_WARMUP_USER || m_GameState == IGS_GAME_PAUSED));
}

void IGameController::OnPlayerReadyChange(CPlayer *pPlayer)
{
	if(pPlayer->m_LastReadyChangeTick && pPlayer->m_LastReadyChangeTick + Server()->TickSpeed() * 1 > Server()->Tick())
		return;

	pPlayer->m_LastReadyChangeTick = Server()->Tick();

	if(Config()->m_SvPlayerReadyMode && pPlayer->GetTeam() != TEAM_SPECTATORS && !pPlayer->m_DeadSpecMode)
	{
		// change players ready state
		pPlayer->m_IsReadyToPlay ^= 1;

		if(m_GameState == IGS_GAME_RUNNING && !pPlayer->m_IsReadyToPlay)
		{
			SetGameState(IGS_GAME_PAUSED, TIMER_INFINITE); // one player isn't ready -> pause the game
			GameServer()->SendGameMsg(protocol7::GAMEMSG_GAME_PAUSED, pPlayer->GetCid(), -1);
		}

		GameServer()->PlayerReadyStateBroadcast();
		CheckReadyStates();
	}
	else
		GameServer()->PlayerReadyStateBroadcast();
}

// to be called when a player changes state, spectates or disconnects
void IGameController::CheckReadyStates(int WithoutId)
{
	if(Config()->m_SvPlayerReadyMode)
	{
		switch(m_GameState)
		{
		case IGS_WARMUP_USER:
			// all players are ready -> end warmup
			if(GetPlayersReadyState(WithoutId))
				SetGameState(IGS_WARMUP_USER, 0);
			break;
		case IGS_GAME_PAUSED:
			// all players are ready -> unpause the game
			if(GetPlayersReadyState(WithoutId))
			{
				SetGameState(IGS_GAME_PAUSED, 0);
				GameServer()->SendBroadcastSix("", false); // clear "%d players not ready" 0.6 backport
			}
			break;
		case IGS_GAME_RUNNING:
		case IGS_WARMUP_GAME:
		case IGS_START_COUNTDOWN_UNPAUSE:
		case IGS_START_COUNTDOWN_ROUND_START:
		case IGS_END_ROUND:
			// not affected
			break;
		}
	}
}

bool IGameController::GetPlayersReadyState(int WithoutId, int *pNumUnready)
{
	int Unready = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == WithoutId)
			continue; // skip
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !GameServer()->m_apPlayers[i]->m_IsReadyToPlay)
		{
			if(!pNumUnready)
				return false;
			Unready++;
		}
	}
	if(pNumUnready)
		*pNumUnready = Unready;

	return true;
}

void IGameController::SetPlayersReadyState(bool ReadyState)
{
	for(auto &pPlayer : GameServer()->m_apPlayers)
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS && (ReadyState || !pPlayer->m_DeadSpecMode))
			pPlayer->m_IsReadyToPlay = ReadyState;
}

bool IGameController::DoWincheckRound()
{
	if(IsTeamplay())
	{
		// check score win condition
		if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
			(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE] || m_GameFlags & protocol7::GAMEFLAG_SURVIVAL)
			{
				EndRound();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	else
	{
		// gather some stats
		int Topscore = 0;
		int TopscoreCount = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{
				if(!pPlayer)
					continue;
				int Score = pPlayer->m_Score.value_or(0);
				if(Score > Topscore)
				{
					Topscore = Score;
					TopscoreCount = 1;
				}
				else if(Score == Topscore)
					TopscoreCount++;
			}

		// check score win condition
		if((m_GameInfo.m_ScoreLimit > 0 && Topscore >= m_GameInfo.m_ScoreLimit) ||
			(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
		{
			if(TopscoreCount == 1)
			{
				EndRound();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	return false;
}

void IGameController::CheckGameInfo()
{
	bool GameInfoChanged = (m_GameInfo.m_ScoreLimit != g_Config.m_SvScorelimit) || (m_GameInfo.m_TimeLimit != g_Config.m_SvTimelimit);
	m_GameInfo.m_MatchCurrent = 0;
	m_GameInfo.m_MatchNum = 0;
	m_GameInfo.m_ScoreLimit = g_Config.m_SvScorelimit;
	m_GameInfo.m_TimeLimit = g_Config.m_SvTimelimit;
	if(GameInfoChanged)
		UpdateGameInfo(-1);
}

bool IGameController::IsFriendlyFire(int ClientId1, int ClientId2)
{
	if(ClientId1 == ClientId2)
		return false;

	if(IsTeamplay())
	{
		if(!GameServer()->m_apPlayers[ClientId1] || !GameServer()->m_apPlayers[ClientId2])
			return false;

		// if(!Config()->m_SvTeamdamage && GameServer()->m_apPlayers[ClientId1]->GetTeam() == GameServer()->m_apPlayers[ClientId2]->GetTeam())
		if(true && GameServer()->m_apPlayers[ClientId1]->GetTeam() == GameServer()->m_apPlayers[ClientId2]->GetTeam())
			return true;
	}

	return false;
}

void IGameController::UpdateGameInfo(int ClientId)
{
	// ddnet-insta
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(ClientId != -1)
			if(ClientId != i)
				continue;

		if(Server()->IsSixup(i))
		{
			protocol7::CNetMsg_Sv_GameInfo Msg;
			Msg.m_GameFlags = m_GameFlags;
			Msg.m_MatchCurrent = 1;
			Msg.m_MatchNum = 0;
			Msg.m_ScoreLimit = Config()->m_SvScorelimit;
			Msg.m_TimeLimit = Config()->m_SvTimelimit;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
}

void IGameController::SetGameState(EGameState GameState, int Timer)
{
	// change game state
	switch(GameState)
	{
	case IGS_WARMUP_GAME:
		// game based warmup is only possible when game or any warmup is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER)
		{
			if(Timer == TIMER_INFINITE)
			{
				// run warmup till there're enough players
				m_GameState = GameState;
				m_GameStateTimer = TIMER_INFINITE;

				// enable respawning in survival when activating warmup
				// if(m_GameFlags&GAMEFLAG_SURVIVAL)
				// {
				// 	for(int i = 0; i < MAX_CLIENTS; ++i)
				// 		if(GameServer()->m_apPlayers[i])
				// 			GameServer()->m_apPlayers[i]->m_RespawnDisabled = false;
				// }
			}
			else if(Timer == 0)
			{
				// start new match
				StartRound();
			}
			m_GamePauseStartTime = -1;
		}
		break;
	case IGS_WARMUP_USER:
		// user based warmup is only possible when the game or any warmup is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER)
		{
			if(Timer != 0)
			{
				// start warmup
				if(Timer < 0)
				{
					m_Warmup = 0;
					m_GameState = GameState;
					m_GameStateTimer = TIMER_INFINITE;
					if(Config()->m_SvPlayerReadyMode)
					{
						// run warmup till all players are ready
						SetPlayersReadyState(false);
					}
				}
				else if(Timer > 0)
				{
					// run warmup for a specific time intervall
					m_GameState = GameState;
					m_GameStateTimer = Timer * Server()->TickSpeed(); // TODO: this is vanilla timer
					m_Warmup = Timer * Server()->TickSpeed(); // TODO: this is ddnet timer
				}

				// enable respawning in survival when activating warmup
				// if(m_GameFlags&GAMEFLAG_SURVIVAL)
				// {
				// 	for(int i = 0; i < MAX_CLIENTS; ++i)
				// 		if(GameServer()->m_apPlayers[i])
				// 			GameServer()->m_apPlayers[i]->m_RespawnDisabled = false;
				// }
				GameServer()->m_World.m_Paused = false;
			}
			else
			{
				// start new match
				StartRound();
			}
			m_GamePauseStartTime = -1;
		}
		break;
	case IGS_START_COUNTDOWN_ROUND_START:
	case IGS_START_COUNTDOWN_UNPAUSE:
		// only possible when game, pause or start countdown is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_START_COUNTDOWN_ROUND_START || m_GameState == IGS_START_COUNTDOWN_UNPAUSE)
		{
			int CountDownSeconds = 0;
			if(m_GameState == IGS_GAME_PAUSED)
				CountDownSeconds = Config()->m_SvCountdownUnpause;
			else
				CountDownSeconds = Config()->m_SvCountdownRoundStart;
			if(CountDownSeconds == 0 && m_GameFlags & protocol7::GAMEFLAG_SURVIVAL)
			{
				m_GameState = GameState;
				m_GameStateTimer = 3 * Server()->TickSpeed();
				GameServer()->m_World.m_Paused = true;
			}
			else if(CountDownSeconds > 0)
			{
				m_GameState = GameState;
				m_GameStateTimer = CountDownSeconds * Server()->TickSpeed();
				GameServer()->m_World.m_Paused = true;
			}
			else
			{
				// no countdown, start new match right away
				SetGameState(IGS_GAME_RUNNING);
			}
			// m_GamePauseStartTime = -1; // countdown while paused still counts as paused
		}
		break;
	case IGS_GAME_RUNNING:
		// always possible
		{
			// ddnet-insta specific
			// vanilla does not do this
			// but ddnet sends m_RoundStartTick in snap
			// so we have to also update that to show current game timer
			if(m_GameState == IGS_START_COUNTDOWN_ROUND_START || m_GameState == IGS_GAME_RUNNING)
			{
				m_RoundStartTick = Server()->Tick();
			}
			// this is also ddnet-insta specific
			// no idea how vanilla does it
			// but this solves countdown delaying timelimit end
			// meaning that if countdown and timelimit is set the
			// timerstops at 00:00 and waits the additional countdown time
			// before actually ending the game
			// https://github.com/ddnet-insta/ddnet-insta/issues/41
			if(m_GameState == IGS_START_COUNTDOWN_ROUND_START)
			{
				m_GameStartTick = Server()->Tick();
				dbg_msg("ddnet-insta", "reset m_GameStartTick");
			}
			m_Warmup = 0;
			m_GameState = GameState;
			m_GameStateTimer = TIMER_INFINITE;
			SetPlayersReadyState(true);
			GameServer()->m_World.m_Paused = false;
			m_GamePauseStartTime = -1;
		}
		break;
	case IGS_GAME_PAUSED:
		// only possible when game is running or paused, or when game based warmup is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_WARMUP_GAME)
		{
			if(Timer != 0)
			{
				// start pause
				if(Timer < 0)
				{
					// pauses infinitely till all players are ready or disabled via rcon command
					m_GameStateTimer = TIMER_INFINITE;
					SetPlayersReadyState(false);
				}
				else
				{
					// pauses for a specific time interval
					m_GameStateTimer = Timer * Server()->TickSpeed();
				}

				m_GameState = GameState;
				GameServer()->m_World.m_Paused = true;
			}
			else
			{
				// start a countdown to end pause
				SetGameState(IGS_START_COUNTDOWN_UNPAUSE);
			}
			m_GamePauseStartTime = time_get();
		}
		break;
	case IGS_END_ROUND:
		if(m_Warmup) // game can't end when we are running warmup
			break;
		m_GamePauseStartTime = -1;
		m_GameOverTick = Server()->Tick();
		if(m_GameState == IGS_END_ROUND)
			break;
		// only possible when game is running or over
		// if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_END_ROUND || m_GameState == IGS_GAME_PAUSED)
		{
			m_GameState = GameState;
			m_GameStateTimer = Timer * Server()->TickSpeed();
			// m_GameOverTick = Timer * Server()->Tick();
			m_SuddenDeath = 0;
			GameServer()->m_World.m_Paused = true;

			OnEndRoundInsta();
		}
	}
}

void IGameController::OnFlagReturn(CFlag *pFlag)
{
}

void IGameController::OnFlagGrab(CFlag *pFlag)
{
}

void IGameController::OnFlagCapture(CFlag *pFlag, float Time)
{
}
