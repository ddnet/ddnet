/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/server/score.h>
#include <game/teamscore.h>

#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"

#include "entities/character.h"
#include "entities/door.h"
#include "entities/dragger.h"
#include "entities/gun.h"
#include "entities/light.h"
#include "entities/pickup.h"
#include "entities/projectile.h"

IGameController::IGameController(class CGameContext *pGameServer) :
	m_Teams(pGameServer), m_pLoadBestTimeResult(nullptr)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();
	m_pGameType = "unknown";

	//
	m_GameOverTick = -1;
	m_SuddenDeath = 0;
	m_RoundStartTick = Server()->Tick();
	m_RoundCount = 0;
	m_GameFlags = 0;
	m_aMapWish[0] = 0;

	m_UnbalancedTick = -1;
	m_ForceBalanced = false;

	m_CurrentRecord = 0;

	InitTeleporter();

	// gctf
	m_Warmup = 0;
	m_GameState = IGS_GAME_RUNNING;
	m_GameStateTimer = TIMER_INFINITE;
	m_GameStartTick = Server()->Tick();
	// if(Config()->m_SvWarmup)
	// 	SetGameState(IGS_WARMUP_USER, Config()->m_SvWarmup);
	// else
	// 	SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);

	// if new game starts with warmup
	// then ready change wont kick in
	// because it only pauses running games
	// think about wether we want warump with timer infinite at all?
	// if yes it should also be active when a round finishes
	// until a new restart is triggered
	// then we also have to indicate to users
	// that there is currently no game running
	// also nice for tournaments to activate public chat again
	//
	// i drafted out a sv_casual_rounds config that decides wether
	// games do go into state warmup infinite or running after match ends
	// and then decided to not fully implement the idea
	// because even for pro games it is confusing that rounds do not auto start
	// one might forget to !restart and then we play a full game in state warmup
	// which will block a bunch of features such as winning pausing the game
	// so if we go down that route make sure to properly indicate
	// that a game is not running

	m_aTeamSize[TEAM_RED] = 0;
	m_aTeamSize[TEAM_BLUE] = 0;
	m_GameStartTick = Server()->Tick();
	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;

	// info
	m_GameFlags = 0;
	m_pGameType = "unknown";
	m_GameInfo.m_MatchCurrent = 0;
	m_GameInfo.m_MatchNum = 0;
	m_GameInfo.m_ScoreLimit = Config()->m_SvScorelimit;
	m_GameInfo.m_TimeLimit = Config()->m_SvTimelimit;
}

IGameController::~IGameController() = default;

void IGameController::DoActivityCheck()
{
	if(g_Config.m_SvInactiveKickTime == 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && Server()->GetAuthedState(i) == AUTHED_NO)
		{
			if(Server()->Tick() > GameServer()->m_apPlayers[i]->m_LastActionTick + g_Config.m_SvInactiveKickTime * Server()->TickSpeed() * 60)
			{
				switch(g_Config.m_SvInactiveKick)
				{
				case 0:
				{
					// move player to spectator
					DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
				case 1:
				{
					// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
					int Spectators = 0;
					for(auto &pPlayer : GameServer()->m_apPlayers)
						if(pPlayer && pPlayer->GetTeam() == TEAM_SPECTATORS)
							++Spectators;
					if(Spectators >= g_Config.m_SvSpectatorSlots)
						Server()->Kick(i, "Kicked for inactivity");
					else
						DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
				case 2:
				{
					// kick the player
					Server()->Kick(i, "Kicked for inactivity");
				}
				}
			}
		}
	}
}

float IGameController::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos, int DDTeam)
{
	float Score = 0.0f;
	CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
	for(; pC; pC = (CCharacter *)pC->TypeNext())
	{
		// ignore players in other teams
		if(GameServer()->GetDDRaceTeam(pC->GetPlayer()->GetCID()) != DDTeam)
			continue;

		float d = distance(Pos, pC->m_Pos);
		Score += d == 0 ? 1000000000.0f : 1.0f / d;
	}

	return Score;
}

void IGameController::EvaluateSpawnType(CSpawnEval *pEval, int Type, int DDTeam)
{
	// j == 0: Find an empty slot, j == 1: Take any slot if no empty one found
	for(int j = 0; j < 2 && !pEval->m_Got; j++)
	{
		// get spawn point
		for(const vec2 &SpawnPoint : m_avSpawnPoints[Type])
		{
			vec2 P = SpawnPoint;
			if(j == 0)
			{
				// check if the position is occupado
				CEntity *apEnts[MAX_CLIENTS];
				int Num = GameServer()->m_World.FindEntities(SpawnPoint, 64, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
				vec2 aPositions[5] = {vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f)}; // start, left, up, right, down
				int Result = -1;
				for(int Index = 0; Index < 5 && Result == -1; ++Index)
				{
					Result = Index;
					if(!GameServer()->m_World.m_Core.m_aTuning[0].m_PlayerCollision)
						break;
					for(int c = 0; c < Num; ++c)
					{
						CCharacter *pChr = static_cast<CCharacter *>(apEnts[c]);
						if(GameServer()->Collision()->CheckPoint(SpawnPoint + aPositions[Index]) ||
							distance(pChr->m_Pos, SpawnPoint + aPositions[Index]) <= pChr->GetProximityRadius())
						{
							Result = -1;
							break;
						}
					}
				}
				if(Result == -1)
					continue; // try next spawn point

				P += aPositions[Result];
			}

			float S = EvaluateSpawnPos(pEval, P, DDTeam);
			if(!pEval->m_Got || (j == 0 && pEval->m_Score > S))
			{
				pEval->m_Got = true;
				pEval->m_Score = S;
				pEval->m_Pos = P;
			}
		}
	}
}

bool IGameController::CanSpawn(int Team, vec2 *pOutPos, int DDTeam)
{
	// spectators can't spawn
	if(Team == TEAM_SPECTATORS)
		return false;

	CSpawnEval Eval;
	if(IsTeamplay()) // gctf
	{
		Eval.m_FriendlyTeam = Team;

		// first try own team spawn, then normal spawn and then enemy
		EvaluateSpawnType(&Eval, 1 + (Team & 1), DDTeam);
		if(!Eval.m_Got)
		{
			EvaluateSpawnType(&Eval, 0, DDTeam);
			if(!Eval.m_Got)
				EvaluateSpawnType(&Eval, 1 + ((Team + 1) & 1), DDTeam);
		}
	}
	else
	{
		EvaluateSpawnType(&Eval, 0, DDTeam);
		EvaluateSpawnType(&Eval, 1, DDTeam);
		EvaluateSpawnType(&Eval, 2, DDTeam);
	}

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

bool IGameController::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	// gctf
	// do not spawn pickups
	if(Index == ENTITY_ARMOR_1 || Index == ENTITY_HEALTH_1 || Index == ENTITY_WEAPON_SHOTGUN || Index == ENTITY_WEAPON_GRENADE || Index == ENTITY_WEAPON_LASER || Index == ENTITY_POWERUP_NINJA)
		return false;

	dbg_assert(Index >= 0, "Invalid entity index");

	const vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);

	int aSides[8];
	aSides[0] = GameServer()->Collision()->Entity(x, y + 1, Layer);
	aSides[1] = GameServer()->Collision()->Entity(x + 1, y + 1, Layer);
	aSides[2] = GameServer()->Collision()->Entity(x + 1, y, Layer);
	aSides[3] = GameServer()->Collision()->Entity(x + 1, y - 1, Layer);
	aSides[4] = GameServer()->Collision()->Entity(x, y - 1, Layer);
	aSides[5] = GameServer()->Collision()->Entity(x - 1, y - 1, Layer);
	aSides[6] = GameServer()->Collision()->Entity(x - 1, y, Layer);
	aSides[7] = GameServer()->Collision()->Entity(x - 1, y + 1, Layer);

	if(Index >= ENTITY_SPAWN && Index <= ENTITY_SPAWN_BLUE && Initial)
	{
		const int Type = Index - ENTITY_SPAWN;
		m_avSpawnPoints[Type].push_back(Pos);
	}
	else if(Index == ENTITY_DOOR)
	{
		for(int i = 0; i < 8; i++)
		{
			if(aSides[i] >= ENTITY_LASER_SHORT && aSides[i] <= ENTITY_LASER_LONG)
			{
				new CDoor(
					&GameServer()->m_World, //GameWorld
					Pos, //Pos
					pi / 4 * i, //Rotation
					32 * 3 + 32 * (aSides[i] - ENTITY_LASER_SHORT) * 3, //Length
					Number //Number
				);
			}
		}
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN_EX)
	{
		int Dir;
		if(!Flags)
			Dir = 0;
		else if(Flags == ROTATION_90)
			Dir = 1;
		else if(Flags == ROTATION_180)
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * (pi / 2);
		CProjectile *pBullet = new CProjectile(
			&GameServer()->m_World,
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(std::sin(Deg), std::cos(Deg)), //Dir
			-2, //Span
			true, //Freeze
			true, //Explosive
			(g_Config.m_SvShotgunBulletSound) ? SOUND_GRENADE_EXPLODE : -1, //SoundImpact
			vec2(std::sin(Deg), std::cos(Deg)), // InitDir
			Layer,
			Number);
		pBullet->SetBouncing(2 - (Dir % 2));
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN)
	{
		int Dir;
		if(!Flags)
			Dir = 0;
		else if(Flags == (TILEFLAG_ROTATE))
			Dir = 1;
		else if(Flags == (TILEFLAG_XFLIP | TILEFLAG_YFLIP))
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * (pi / 2);
		CProjectile *pBullet = new CProjectile(
			&GameServer()->m_World,
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(std::sin(Deg), std::cos(Deg)), //Dir
			-2, //Span
			true, //Freeze
			false, //Explosive
			SOUND_GRENADE_EXPLODE,
			vec2(std::sin(Deg), std::cos(Deg)), // InitDir
			Layer,
			Number);
		pBullet->SetBouncing(2 - (Dir % 2));
	}

	int Type = -1;
	int SubType = 0;

	if(Index == ENTITY_ARMOR_1)
		Type = POWERUP_ARMOR;
	else if(Index == ENTITY_ARMOR_SHOTGUN)
		Type = POWERUP_ARMOR_SHOTGUN;
	else if(Index == ENTITY_ARMOR_GRENADE)
		Type = POWERUP_ARMOR_GRENADE;
	else if(Index == ENTITY_ARMOR_NINJA)
		Type = POWERUP_ARMOR_NINJA;
	else if(Index == ENTITY_ARMOR_LASER)
		Type = POWERUP_ARMOR_LASER;
	else if(Index == ENTITY_HEALTH_1)
		Type = POWERUP_HEALTH;
	else if(Index == ENTITY_WEAPON_SHOTGUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_SHOTGUN;
	}
	else if(Index == ENTITY_WEAPON_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_GRENADE;
	}
	else if(Index == ENTITY_WEAPON_LASER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_LASER;
	}
	else if(Index == ENTITY_POWERUP_NINJA)
	{
		Type = POWERUP_NINJA;
		SubType = WEAPON_NINJA;
	}
	else if(Index >= ENTITY_LASER_FAST_CCW && Index <= ENTITY_LASER_FAST_CW)
	{
		int aSides2[8];
		aSides2[0] = GameServer()->Collision()->Entity(x, y + 2, Layer);
		aSides2[1] = GameServer()->Collision()->Entity(x + 2, y + 2, Layer);
		aSides2[2] = GameServer()->Collision()->Entity(x + 2, y, Layer);
		aSides2[3] = GameServer()->Collision()->Entity(x + 2, y - 2, Layer);
		aSides2[4] = GameServer()->Collision()->Entity(x, y - 2, Layer);
		aSides2[5] = GameServer()->Collision()->Entity(x - 2, y - 2, Layer);
		aSides2[6] = GameServer()->Collision()->Entity(x - 2, y, Layer);
		aSides2[7] = GameServer()->Collision()->Entity(x - 2, y + 2, Layer);

		int Ind = Index - ENTITY_LASER_STOP;
		int M;
		if(Ind < 0)
		{
			Ind = -Ind;
			M = 1;
		}
		else if(Ind == 0)
			M = 0;
		else
			M = -1;

		float AngularSpeed = 0.0f;
		if(Ind == 0)
			AngularSpeed = 0.0f;
		else if(Ind == 1)
			AngularSpeed = pi / 360;
		else if(Ind == 2)
			AngularSpeed = pi / 180;
		else if(Ind == 3)
			AngularSpeed = pi / 90;
		AngularSpeed *= M;

		for(int i = 0; i < 8; i++)
		{
			if(aSides[i] >= ENTITY_LASER_SHORT && aSides[i] <= ENTITY_LASER_LONG)
			{
				CLight *pLight = new CLight(&GameServer()->m_World, Pos, pi / 4 * i, 32 * 3 + 32 * (aSides[i] - ENTITY_LASER_SHORT) * 3, Layer, Number);
				pLight->m_AngularSpeed = AngularSpeed;
				if(aSides2[i] >= ENTITY_LASER_C_SLOW && aSides2[i] <= ENTITY_LASER_C_FAST)
				{
					pLight->m_Speed = 1 + (aSides2[i] - ENTITY_LASER_C_SLOW) * 2;
					pLight->m_CurveLength = pLight->m_Length;
				}
				else if(aSides2[i] >= ENTITY_LASER_O_SLOW && aSides2[i] <= ENTITY_LASER_O_FAST)
				{
					pLight->m_Speed = 1 + (aSides2[i] - ENTITY_LASER_O_SLOW) * 2;
					pLight->m_CurveLength = 0;
				}
				else
					pLight->m_CurveLength = pLight->m_Length;
			}
		}
	}
	else if(Index >= ENTITY_DRAGGER_WEAK && Index <= ENTITY_DRAGGER_STRONG)
	{
		new CDragger(&GameServer()->m_World, Pos, Index - ENTITY_DRAGGER_WEAK + 1, false, Layer, Number);
	}
	else if(Index >= ENTITY_DRAGGER_WEAK_NW && Index <= ENTITY_DRAGGER_STRONG_NW)
	{
		new CDragger(&GameServer()->m_World, Pos, Index - ENTITY_DRAGGER_WEAK_NW + 1, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAE)
	{
		new CGun(&GameServer()->m_World, Pos, false, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAF)
	{
		new CGun(&GameServer()->m_World, Pos, true, false, Layer, Number);
	}
	else if(Index == ENTITY_PLASMA)
	{
		new CGun(&GameServer()->m_World, Pos, true, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAU)
	{
		new CGun(&GameServer()->m_World, Pos, false, false, Layer, Number);
	}

	if(Type != -1) // NOLINT(clang-analyzer-unix.Malloc)
	{
		CPickup *pPickup = new CPickup(&GameServer()->m_World, Type, SubType, Layer, Number);
		pPickup->m_Pos = Pos;
		return true; // NOLINT(clang-analyzer-unix.Malloc)
	}

	return false;
}

void IGameController::OnPlayerConnect(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();
	pPlayer->Respawn();

	if(!Server()->ClientPrevIngame(ClientID))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}
}

void IGameController::OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason)
{
	pPlayer->OnDisconnect();
	int ClientID = pPlayer->GetCID();
	if(Server()->ClientIngame(ClientID))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}
}

void IGameController::ResetGame()
{
	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		pPlayer->m_HasGhostCharInGame = pPlayer->GetCharacter() != 0;
	}
	GameServer()->m_World.m_ResetRequested = true;

	// gctf
	m_GameStartTick = Server()->Tick();
	SetGameState(IGS_GAME_RUNNING);
	m_GameStartTick = Server()->Tick();
	m_SuddenDeath = 0;
}

const char *IGameController::GetTeamName(int Team)
{
	// gctf
	if(IsTeamplay())
	{
		if(Team == TEAM_RED)
			return "red team";
		if(Team == TEAM_BLUE)
			return "blue team";
	}

	if(Team == 0)
		return "game";
	return "spectators";
}

void IGameController::StartRound()
{
	ResetGame();

	m_RoundStartTick = Server()->Tick();
	m_SuddenDeath = 0;
	m_GameOverTick = -1;
	GameServer()->m_World.m_Paused = false;
	m_aTeamscore[TEAM_RED] = 0; // gctf
	m_aTeamscore[TEAM_BLUE] = 0; // gctf
	m_ForceBalanced = false;
	Server()->DemoRecorder_HandleAutoStart();
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags & GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

void IGameController::ChangeMap(const char *pToMap)
{
	Server()->ChangeMap(pToMap);
}

void IGameController::OnReset()
{
	for(auto &pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		pPlayer->Respawn();
		pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
		pPlayer->m_Score = 0;
		pPlayer->m_IsReadyToPlay = true; // gctf
	}
}

int IGameController::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	return 0;
}

void IGameController::OnCharacterSpawn(class CCharacter *pChr)
{
	pChr->SetTeams(&Teams());
	Teams().OnCharacterSpawn(pChr->GetPlayer()->GetCID());

	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER);
	pChr->GiveWeapon(WEAPON_GUN);

	pChr->SetTeleports(&m_TeleOuts, &m_TeleCheckOuts);
}

void IGameController::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	// Do nothing by default
}

void IGameController::DoWarmup(int Seconds)
{
	// gets overwritten by SetGameState
	// but SetGameState might not set it
	// and then it is unitialized
	m_Warmup = 0;
	SetGameState(IGS_WARMUP_USER, Seconds);

	if(Seconds)
	{
		if(g_Config.m_SvTournamentChatSmart)
		{
			g_Config.m_SvTournamentChat = g_Config.m_SvTournamentChatSmart;
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, g_Config.m_SvTournamentChatSmart == 1 ? "Spectators can no longer use public chat" : "All can no longer use public chat");
		}
	}
}

bool IGameController::IsForceBalanced()
{
	return false;
}

bool IGameController::CanBeMovedOnBalance(int ClientID)
{
	return true;
}

void IGameController::Tick()
{
	// handle game states
	if(m_GameState != IGS_GAME_RUNNING)
	{
		if(m_GameStateTimer > 0)
		{
			--m_GameStateTimer;

			// 0.6 can do warmup timers (world unpaused incoming reload)
			// but no game countdown timers (world paused incoming unpause)
			if(m_GameState == IGS_START_COUNTDOWN_ROUND_START || m_GameState == IGS_START_COUNTDOWN_UNPAUSE)
			{
				static int s_LastSecs = -1;
				int Secs = m_GameStateTimer / Server()->TickSpeed();
				if(s_LastSecs != Secs)
				{
					s_LastSecs = Secs;
					if(Secs == 0)
						GameServer()->SendBroadcastSix("", false);
					else
					{
						char aBuf[512];
						str_format(aBuf, sizeof(aBuf), "Game starts in: %d", Secs);
						GameServer()->SendBroadcastSix(aBuf, false);
					}
				}
			}
		}

		if(m_GameStateTimer == 0)
		{
			// timer fires
			switch(m_GameState)
			{
			case IGS_WARMUP_USER:
				// end warmup
				SetGameState(IGS_WARMUP_USER, 0);
				break;
			case IGS_START_COUNTDOWN_ROUND_START:
			case IGS_START_COUNTDOWN_UNPAUSE:
				// unpause the game
				SetGameState(IGS_GAME_RUNNING);
				break;
			case IGS_GAME_PAUSED:
				// end pause
				SetGameState(IGS_GAME_PAUSED, 0);
				break;
			case IGS_END_ROUND:
				StartRound();
				break;
			case IGS_END_MATCH:
				// start next match
				// if(m_MatchCount >= m_GameInfo.m_MatchNum-1)
				// 	CycleMap();

				// if(Config()->m_SvMatchSwap)
				// 	GameServer()->SwapTeams();
				// m_MatchCount++;
				StartMatch(true);
				break;
			case IGS_WARMUP_GAME:
			case IGS_GAME_RUNNING:
				// not effected
				break;
			}
		}
		else
		{
			// timer still running
			switch(m_GameState)
			{
			case IGS_WARMUP_USER:
				// check if player ready mode was disabled and it waits that all players are ready -> end warmup
				if(!Config()->m_SvPlayerReadyMode && m_GameStateTimer == TIMER_INFINITE)
					SetGameState(IGS_WARMUP_USER, 0);
				break;
			case IGS_START_COUNTDOWN_ROUND_START:
			case IGS_START_COUNTDOWN_UNPAUSE:
			case IGS_GAME_PAUSED:
				// freeze the game
				++m_RoundStartTick;
				++m_GameStartTick;
				break;
			case IGS_WARMUP_GAME:
			case IGS_GAME_RUNNING:
			case IGS_END_MATCH:
			case IGS_END_ROUND:
				// not effected
				break;
			}
		}
	}

	// do warmup
	if(m_Warmup)
	{
		m_Warmup--;
		// gctf uses StartRound() in SetGameState() vanilla style
		// if(!m_Warmup)
		// 	StartRound();
	}

	if(m_GameOverTick != -1)
	{
		// game over.. wait for restart
		if(Server()->Tick() > m_GameOverTick + Server()->TickSpeed() * 10)
		{
			StartRound();
			m_RoundCount++;
		}
	}

	if(m_pLoadBestTimeResult != nullptr && m_pLoadBestTimeResult->m_Completed)
	{
		if(m_pLoadBestTimeResult->m_Success)
		{
			m_CurrentRecord = m_pLoadBestTimeResult->m_CurrentRecord;

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetClientVersion() >= VERSION_DDRACE)
				{
					GameServer()->SendRecord(i);
				}
			}
		}
		m_pLoadBestTimeResult = nullptr;
	}

	DoActivityCheck();
}

void IGameController::Snap(int SnappingClient)
{
	CNetObj_GameInfo *pGameInfoObj = Server()->SnapNewItem<CNetObj_GameInfo>(0);
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	if(m_GameOverTick != -1)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	if(m_SuddenDeath)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	if(GameServer()->m_World.m_Paused)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;
	pGameInfoObj->m_RoundStartTick = m_RoundStartTick;
	pGameInfoObj->m_WarmupTimer = m_Warmup;

	pGameInfoObj->m_ScoreLimit = g_Config.m_SvScorelimit;
	pGameInfoObj->m_TimeLimit = g_Config.m_SvTimelimit;

	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = m_RoundCount + 1;

	CCharacter *pChr;
	CPlayer *pPlayer = SnappingClient != SERVER_DEMO_CLIENT ? GameServer()->m_apPlayers[SnappingClient] : 0;
	CPlayer *pPlayer2;

	if(pPlayer && (pPlayer->m_TimerType == CPlayer::TIMERTYPE_GAMETIMER || pPlayer->m_TimerType == CPlayer::TIMERTYPE_GAMETIMER_AND_BROADCAST) && pPlayer->GetClientVersion() >= VERSION_DDNET_GAMETICK)
	{
		if((pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && pPlayer->m_SpectatorID != SPEC_FREEVIEW && (pPlayer2 = GameServer()->m_apPlayers[pPlayer->m_SpectatorID]))
		{
			if((pChr = pPlayer2->GetCharacter()) && pChr->m_DDRaceState == DDRACE_STARTED)
			{
				pGameInfoObj->m_WarmupTimer = -pChr->m_StartTime;
				pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_RACETIME;
			}
		}
		else if((pChr = pPlayer->GetCharacter()) && pChr->m_DDRaceState == DDRACE_STARTED)
		{
			pGameInfoObj->m_WarmupTimer = -pChr->m_StartTime;
			pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_RACETIME;
		}
	}

	CNetObj_GameInfoEx *pGameInfoEx = Server()->SnapNewItem<CNetObj_GameInfoEx>(0);
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags =
		/* GAMEINFOFLAG_TIMESCORE | */ // gctf
		GAMEINFOFLAG_GAMETYPE_RACE |
		GAMEINFOFLAG_GAMETYPE_DDRACE |
		GAMEINFOFLAG_GAMETYPE_DDNET |
		GAMEINFOFLAG_UNLIMITED_AMMO |
		GAMEINFOFLAG_RACE_RECORD_MESSAGE |
		GAMEINFOFLAG_ALLOW_EYE_WHEEL |
		GAMEINFOFLAG_ALLOW_HOOK_COLL |
		/* GAMEINFOFLAG_ALLOW_ZOOM | */ // gctf
		GAMEINFOFLAG_BUG_DDRACE_GHOST |
		GAMEINFOFLAG_BUG_DDRACE_INPUT |
		GAMEINFOFLAG_PREDICT_DDRACE |
		GAMEINFOFLAG_PREDICT_DDRACE_TILES |
		GAMEINFOFLAG_ENTITIES_DDNET |
		GAMEINFOFLAG_ENTITIES_DDRACE |
		GAMEINFOFLAG_ENTITIES_RACE |
		GAMEINFOFLAG_RACE;
	if(g_Config.m_SvFastcap) //gctf
	{
		pGameInfoEx->m_Flags |= GAMEINFOFLAG_GAMETYPE_FASTCAP;
		pGameInfoEx->m_Flags |= GAMEINFOFLAG_FLAG_STARTS_RACE;
	}
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_HUD_AMMO | GAMEINFOFLAG2_HUD_HEALTH_ARMOR; // gctf
	if(g_Config.m_SvNoWeakHook)
		pGameInfoEx->m_Flags2 |= GAMEINFOFLAG2_NO_WEAK_HOOK;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;

	if(Server()->IsSixup(SnappingClient))
	{
		protocol7::CNetObj_GameData *pGameData = Server()->SnapNewItem<protocol7::CNetObj_GameData>(0);
		if(!pGameData)
			return;

		pGameData->m_GameStartTick = m_RoundStartTick;
		pGameData->m_GameStateFlags = 0;
		if(m_GameOverTick != -1)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_GAMEOVER;
		if(m_SuddenDeath)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_SUDDENDEATH;
		if(GameServer()->m_World.m_Paused)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_PAUSED;

		pGameData->m_GameStateEndTick = 0;

		protocol7::CNetObj_GameDataRace *pRaceData = Server()->SnapNewItem<protocol7::CNetObj_GameDataRace>(0);
		if(!pRaceData)
			return;

		pRaceData->m_BestTime = round_to_int(m_CurrentRecord * 1000);
		pRaceData->m_Precision = 2;
		pRaceData->m_RaceFlags = protocol7::RACEFLAG_HIDE_KILLMSG | protocol7::RACEFLAG_KEEP_WANTED_WEAPON;

		// gctf
		if(IsTeamplay())
		{
			protocol7::CNetObj_GameDataTeam *pGameDataTeam = static_cast<protocol7::CNetObj_GameDataTeam *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_GAMEDATATEAM, 0, sizeof(protocol7::CNetObj_GameDataTeam)));
			if(!pGameDataTeam)
				return;

			pGameDataTeam->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
			pGameDataTeam->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];
		}
		switch(m_GameState)
		{
		case IGS_WARMUP_GAME:
		case IGS_WARMUP_USER:
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_WARMUP;
			if(m_GameStateTimer != TIMER_INFINITE)
				pGameData->m_GameStateEndTick = Server()->Tick() + m_GameStateTimer;
			break;
		case IGS_START_COUNTDOWN_ROUND_START:
		case IGS_START_COUNTDOWN_UNPAUSE:
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_STARTCOUNTDOWN | protocol7::GAMESTATEFLAG_PAUSED;
			if(m_GameStateTimer != TIMER_INFINITE)
				pGameData->m_GameStateEndTick = Server()->Tick() + m_GameStateTimer;
			break;
		case IGS_GAME_PAUSED:
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_PAUSED;
			if(m_GameStateTimer != TIMER_INFINITE)
				pGameData->m_GameStateEndTick = Server()->Tick() + m_GameStateTimer;
			break;
		case IGS_END_ROUND:
			pGameData->m_GameStateFlags = pGameData->m_GameStateFlags & ~protocol7::GAMESTATEFLAG_PAUSED; // clear pause
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_ROUNDOVER;
			pGameData->m_GameStateEndTick = Server()->Tick() - m_GameStartTick - TIMER_END / 2 * Server()->TickSpeed() + m_GameStateTimer;
			break;
		case IGS_END_MATCH:
			pGameData->m_GameStateFlags = pGameData->m_GameStateFlags & ~protocol7::GAMESTATEFLAG_PAUSED; // clear pause
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_GAMEOVER;
			pGameData->m_GameStateEndTick = Server()->Tick() - m_GameStartTick - TIMER_END * Server()->TickSpeed() + m_GameStateTimer;
			break;
		case IGS_GAME_RUNNING:
			// not effected
			break;
		}
	}

	if(!GameServer()->Switchers().empty())
	{
		int Team = pPlayer && pPlayer->GetCharacter() ? pPlayer->GetCharacter()->Team() : 0;

		if(pPlayer && (pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && pPlayer->m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[pPlayer->m_SpectatorID] && GameServer()->m_apPlayers[pPlayer->m_SpectatorID]->GetCharacter())
			Team = GameServer()->m_apPlayers[pPlayer->m_SpectatorID]->GetCharacter()->Team();

		if(Team == TEAM_SUPER)
			return;

		int SentTeam = Team;
		if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
			SentTeam = 0;

		CNetObj_SwitchState *pSwitchState = Server()->SnapNewItem<CNetObj_SwitchState>(SentTeam);
		if(!pSwitchState)
			return;

		pSwitchState->m_HighestSwitchNumber = clamp((int)GameServer()->Switchers().size() - 1, 0, 255);
		mem_zero(pSwitchState->m_aStatus, sizeof(pSwitchState->m_aStatus));

		std::vector<std::pair<int, int>> vEndTicks; // <EndTick, SwitchNumber>

		for(int i = 0; i <= pSwitchState->m_HighestSwitchNumber; i++)
		{
			int Status = (int)GameServer()->Switchers()[i].m_aStatus[Team];
			pSwitchState->m_aStatus[i / 32] |= (Status << (i % 32));

			int EndTick = GameServer()->Switchers()[i].m_aEndTick[Team];
			if(EndTick > 0 && EndTick < Server()->Tick() + 3 * Server()->TickSpeed() && GameServer()->Switchers()[i].m_aLastUpdateTick[Team] < Server()->Tick())
			{
				// only keep track of EndTicks that have less than three second left and are not currently being updated by a player being present on a switch tile, to limit how often these are sent
				vEndTicks.emplace_back(GameServer()->Switchers()[i].m_aEndTick[Team], i);
			}
		}

		// send the endtick of switchers that are about to toggle back (up to four, prioritizing those with the earliest endticks)
		mem_zero(pSwitchState->m_aSwitchNumbers, sizeof(pSwitchState->m_aSwitchNumbers));
		mem_zero(pSwitchState->m_aEndTicks, sizeof(pSwitchState->m_aEndTicks));

		std::sort(vEndTicks.begin(), vEndTicks.end());
		const int NumTimedSwitchers = minimum((int)vEndTicks.size(), (int)std::size(pSwitchState->m_aEndTicks));

		for(int i = 0; i < NumTimedSwitchers; i++)
		{
			pSwitchState->m_aSwitchNumbers[i] = vEndTicks[i].second;
			pSwitchState->m_aEndTicks[i] = vEndTicks[i].first;
		}
	}
}

int IGameController::GetAutoTeam(int NotThisID)
{
	int aNumplayers[2] = {0, 0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_BLUE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	int Team = 0;

	if(CanJoinTeam(Team, NotThisID))
		return Team;
	return -1;
}

bool IGameController::CanJoinTeam(int Team, int NotThisID)
{
	if(Team == TEAM_SPECTATORS || (GameServer()->m_apPlayers[NotThisID] && GameServer()->m_apPlayers[NotThisID]->GetTeam() != TEAM_SPECTATORS))
		return true;

	int aNumplayers[2] = {0, 0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_BLUE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	return (aNumplayers[0] + aNumplayers[1]) < Server()->MaxClients() - g_Config.m_SvSpectatorSlots;
}

int IGameController::ClampTeam(int Team)
{
	if(Team < TEAM_RED)
		return TEAM_SPECTATORS;
	if(IsTeamplay())
		return Team & 1;
	return TEAM_RED;
}

CClientMask IGameController::GetMaskForPlayerWorldEvent(int Asker, int ExceptID)
{
	if(Asker == -1)
		return CClientMask().set().reset(ExceptID);

	return Teams().TeamMask(GameServer()->GetDDRaceTeam(Asker), ExceptID, Asker);
}

void IGameController::InitTeleporter()
{
	if(!GameServer()->Collision()->Layers()->TeleLayer())
		return;
	int Width = GameServer()->Collision()->Layers()->TeleLayer()->m_Width;
	int Height = GameServer()->Collision()->Layers()->TeleLayer()->m_Height;

	for(int i = 0; i < Width * Height; i++)
	{
		int Number = GameServer()->Collision()->TeleLayer()[i].m_Number;
		int Type = GameServer()->Collision()->TeleLayer()[i].m_Type;
		if(Number > 0)
		{
			if(Type == TILE_TELEOUT)
			{
				m_TeleOuts[Number - 1].emplace_back(i % Width * 32.0f + 16.0f, i / Width * 32.0f + 16.0f);
			}
			else if(Type == TILE_TELECHECKOUT)
			{
				m_TeleCheckOuts[Number - 1].emplace_back(i % Width * 32.0f + 16.0f, i / Width * 32.0f + 16.0f);
			}
		}
	}
}

void IGameController::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	pPlayer->SetTeam(Team);
	int ClientID = pPlayer->GetCID();

	char aBuf[128];
	DoChatMsg = false;
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", ClientID, Server()->ClientName(ClientID), Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// OnPlayerInfoChange(pPlayer);
}

// gctf

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
			GameServer()->SendGameMsg(protocol7::GAMEMSG_GAME_PAUSED, pPlayer->GetCID(), -1);
		}

		GameServer()->PlayerReadyStateBroadcast();
		CheckReadyStates();
	}
	else
		GameServer()->PlayerReadyStateBroadcast();
}

// to be called when a player changes state, spectates or disconnects
void IGameController::CheckReadyStates(int WithoutID)
{
	if(Config()->m_SvPlayerReadyMode)
	{
		switch(m_GameState)
		{
		case IGS_WARMUP_USER:
			// all players are ready -> end warmup
			if(GetPlayersReadyState(WithoutID))
				SetGameState(IGS_WARMUP_USER, 0);
			break;
		case IGS_GAME_PAUSED:
			// all players are ready -> unpause the game
			if(GetPlayersReadyState(WithoutID))
			{
				SetGameState(IGS_GAME_PAUSED, 0);
				GameServer()->SendBroadcastSix("", false); // clear "%d players not ready" 0.6 backport
			}
			break;
		case IGS_GAME_RUNNING:
		case IGS_WARMUP_GAME:
		case IGS_START_COUNTDOWN_UNPAUSE:
		case IGS_START_COUNTDOWN_ROUND_START:
		case IGS_END_MATCH:
		case IGS_END_ROUND:
			// not affected
			break;
		}
	}
}

bool IGameController::GetPlayersReadyState(int WithoutID, int *pNumUnready)
{
	int Unready = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == WithoutID)
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

bool IGameController::DoWincheckMatch()
{
	if(IsTeamplay())
	{
		// check score win condition
		if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
			(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE] || m_GameFlags & protocol7::GAMEFLAG_SURVIVAL)
			{
				EndMatch();
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
				EndMatch();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	return false;
}

void IGameController::StartMatch(bool RoundEnd)
{
	ResetGame();

	m_RoundCount = 0;
	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;

	// only auto start round if we are in casual mode and there is no tournament running
	// otherwise set infinite warmup and wait for !restart
	// if(RoundEnd && (!g_Config.m_SvCasualRounds || g_Config.m_SvTournament))
	// 	SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
	// else
	SetGameState(IGS_START_COUNTDOWN_ROUND_START);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "start match type='%s' teamplay='%d'", m_pGameType, m_GameFlags & GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

int IGameController::GetStartTeam()
{
	if(Config()->m_SvTournamentMode)
		return TEAM_SPECTATORS;

	// determine new team
	int Team = TEAM_RED;
	if(IsTeamplay())
	{
#ifdef CONF_DEBUG
		if(!Config()->m_DbgStress) // this will force the auto balancer to work overtime aswell
#endif
			Team = m_aTeamSize[TEAM_RED] > m_aTeamSize[TEAM_BLUE] ? TEAM_BLUE : TEAM_RED;
	}

	// check if there're enough player slots left
	// TODO: add SvPlayerSlots in upstream
	if(m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE] < Server()->MaxClients() - g_Config.m_SvSpectatorSlots)
	{
		++m_aTeamSize[Team];
		// m_UnbalancedTick = TBALANCE_CHECK;
		// if(m_GameState == IGS_WARMUP_GAME && HasEnoughPlayers())
		// 	SetGameState(IGS_WARMUP_GAME, 0);
		return Team;
	}
	return TEAM_SPECTATORS;
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

bool IGameController::IsFriendlyFire(int ClientID1, int ClientID2)
{
	if(ClientID1 == ClientID2)
		return false;

	if(IsTeamplay())
	{
		if(!GameServer()->m_apPlayers[ClientID1] || !GameServer()->m_apPlayers[ClientID2])
			return false;

		// if(!Config()->m_SvTeamdamage && GameServer()->m_apPlayers[ClientID1]->GetTeam() == GameServer()->m_apPlayers[ClientID2]->GetTeam())
		if(true && GameServer()->m_apPlayers[ClientID1]->GetTeam() == GameServer()->m_apPlayers[ClientID2]->GetTeam())
			return true;
	}

	return false;
}

void IGameController::UpdateGameInfo(int ClientID)
{
	// gctf
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(ClientID != -1)
			if(ClientID != i)
				continue;

		if(Server()->IsSixup(i))
		{
			protocol7::CNetMsg_Sv_GameInfo Msg;
			Msg.m_GameFlags = protocol7::GAMEFLAG_RACE;
			Msg.m_GameFlags = protocol7::GAMEFLAG_TEAMS | protocol7::GAMEFLAG_FLAGS;
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
				StartMatch(false);
			}
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
				StartMatch(false);
			}
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
		}
		break;
	case IGS_GAME_RUNNING:
		// always possible
		{
			// gctf specific
			// vanilla does not do this
			// but ddnet sends m_RoundStartTick in snap
			// so we have to also update that to show current game timer
			if(m_GameState == IGS_START_COUNTDOWN_ROUND_START || m_GameState == IGS_GAME_RUNNING)
			{
				m_RoundStartTick = Server()->Tick();
				dbg_msg("ddnet-insta", "reset m_RoundStartTick");
			}
			// this is also gctf specific
			// no idea how vanilla does it
			// but this solves countdown delaying timelimit end
			// meaning that if countdown and timelimit is set the
			// timerstops at 00:00 and waits the additional countdown time
			// before actually ending the game
			// https://github.com/ZillyInsta/ddnet-insta/issues/41
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
		}
		break;
	case IGS_END_ROUND:
	case IGS_END_MATCH:
		if(m_Warmup) // game can't end when we are running warmup
			break;
		m_GameOverTick = Server()->Tick();
		if(GameState == IGS_END_ROUND && DoWincheckMatch())
			break;
		// only possible when game is running or over
		// if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_END_MATCH || m_GameState == IGS_END_ROUND || m_GameState == IGS_GAME_PAUSED)
		{
			m_GameState = GameState;
			m_GameStateTimer = Timer * Server()->TickSpeed();
			// m_GameOverTick = Timer * Server()->Tick();
			m_SuddenDeath = 0;
			GameServer()->m_World.m_Paused = true;

			if(g_Config.m_SvTournamentChatSmart)
			{
				g_Config.m_SvTournamentChat = 0;
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, g_Config.m_SvTournamentChatSmart == 1 ? "Spectators can use public chat again" : "All can use public chat again");
			}
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
