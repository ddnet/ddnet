/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include "DDRace.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#define GAME_TYPE_NAME "DDraceNetwork"
#define TEST_TYPE_NAME "TestDDraceNetwork"

CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
	IGameController(pGameServer), m_Teams(pGameServer), m_pInitResult(nullptr)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;

	InitTeleporter();
}

CGameControllerDDRace::~CGameControllerDDRace() = default;

CScore *CGameControllerDDRace::Score()
{
	return GameServer()->Score();
}

void CGameControllerDDRace::OnCharacterSpawn(CCharacter *pChr)
{
	IGameController::OnCharacterSpawn(pChr);
	pChr->SetTeams(&m_Teams);
	pChr->SetTeleports(&m_TeleOuts, &m_TeleCheckOuts);
	m_Teams.OnCharacterSpawn(pChr->GetPlayer()->GetCID());
}

void CGameControllerDDRace::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	CPlayer *pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	int m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	//Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);

	const int PlayerDDRaceState = pChr->m_DDRaceState;
	bool IsOnStartTile = (m_TileIndex == TILE_START) || (m_TileFIndex == TILE_START) || FTile1 == TILE_START || FTile2 == TILE_START || FTile3 == TILE_START || FTile4 == TILE_START || Tile1 == TILE_START || Tile2 == TILE_START || Tile3 == TILE_START || Tile4 == TILE_START;
	// start
	if(IsOnStartTile && PlayerDDRaceState != DDRACE_CHEAT)
	{
		const int Team = GetPlayerTeam(ClientID);
		if(m_Teams.GetSaving(Team))
		{
			GameServer()->SendStartWarning(ClientID, "You can't start while loading/saving of team is in progress");
			pChr->Die(ClientID, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && (Team == TEAM_FLOCK || m_Teams.Count(Team) <= 1))
		{
			GameServer()->SendStartWarning(ClientID, "You have to be in a team with other tees to start");
			pChr->Die(ClientID, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team > TEAM_FLOCK && Team < TEAM_SUPER && m_Teams.Count(Team) < g_Config.m_SvMinTeamSize)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Your team has fewer than %d players, so your team rank won't count", g_Config.m_SvMinTeamSize);
			GameServer()->SendStartWarning(ClientID, aBuf);
		}
		if(g_Config.m_SvResetPickups)
		{
			pChr->ResetPickups();
		}

		m_Teams.OnCharacterStart(ClientID);
		pChr->m_LastTimeCp = -1;
		pChr->m_LastTimeCpBroadcasted = -1;
		for(float &CurrentTimeCp : pChr->m_aCurrentTimeCp)
		{
			CurrentTimeCp = 0.0f;
		}
	}

	// finish
	if(((m_TileIndex == TILE_FINISH) || (m_TileFIndex == TILE_FINISH) || FTile1 == TILE_FINISH || FTile2 == TILE_FINISH || FTile3 == TILE_FINISH || FTile4 == TILE_FINISH || Tile1 == TILE_FINISH || Tile2 == TILE_FINISH || Tile3 == TILE_FINISH || Tile4 == TILE_FINISH) && PlayerDDRaceState == DDRACE_STARTED)
		m_Teams.OnCharacterFinish(ClientID);

	// unlock team
	else if(((m_TileIndex == TILE_UNLOCK_TEAM) || (m_TileFIndex == TILE_UNLOCK_TEAM)) && m_Teams.TeamLocked(GetPlayerTeam(ClientID)))
	{
		m_Teams.SetTeamLock(GetPlayerTeam(ClientID), false);
		GameServer()->SendChatTeam(GetPlayerTeam(ClientID), "Your team was unlocked by an unlock team tile");
	}

	// solo part
	if(((m_TileIndex == TILE_SOLO_ENABLE) || (m_TileFIndex == TILE_SOLO_ENABLE)) && !m_Teams.m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now in a solo part");
		pChr->SetSolo(true);
	}
	else if(((m_TileIndex == TILE_SOLO_DISABLE) || (m_TileFIndex == TILE_SOLO_DISABLE)) && m_Teams.m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now out of the solo part");
		pChr->SetSolo(false);
	}
}

void CGameControllerDDRace::OnPlayerConnect(CPlayer *pPlayer)
{
	IGameController::OnPlayerConnect(pPlayer);
	int ClientID = pPlayer->GetCID();

	// init the player
	Score()->PlayerData(ClientID)->Reset();
	pPlayer->m_Score = Score()->PlayerData(ClientID)->m_BestTime ? Score()->PlayerData(ClientID)->m_BestTime : -9999;

	// Can't set score here as LoadScore() is threaded, run it in
	// LoadScoreThreaded() instead
	Score()->LoadPlayerData(ClientID);

	if(!Server()->ClientPrevIngame(ClientID))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);

		GameServer()->SendChatTarget(ClientID, "DDraceNetwork Mod. Version: " GAME_VERSION);
		GameServer()->SendChatTarget(ClientID, "please visit DDNet.org or say /info and make sure to read our /rules");
	}
}

void CGameControllerDDRace::OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason)
{
	int ClientID = pPlayer->GetCID();
	bool WasModerator = pPlayer->m_Moderating && Server()->ClientIngame(ClientID);

	IGameController::OnPlayerDisconnect(pPlayer, pReason);

	if(!GameServer()->PlayerModerating() && WasModerator)
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");

	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO)
		m_Teams.SetForceCharacterTeam(ClientID, TEAM_FLOCK);
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();
	m_Teams.ProcessSaveTeam();
	m_Teams.Tick();

	if(m_pInitResult != nullptr && m_pInitResult->m_Completed)
	{
		if(m_pInitResult->m_Success)
		{
			m_CurrentRecord = m_pInitResult->m_CurrentRecord;
		}
		m_pInitResult = nullptr;
	}
}

void CGameControllerDDRace::DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	CCharacter *pCharacter = pPlayer->GetCharacter();

	if(Team == TEAM_SPECTATORS)
	{
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && pCharacter)
		{
			// Joining spectators should not kill a locked team, but should still
			// check if the team finished by you leaving it.
			int DDRTeam = pCharacter->Team();
			m_Teams.SetForceCharacterTeam(pPlayer->GetCID(), TEAM_FLOCK);
			m_Teams.CheckTeamFinished(DDRTeam);
		}
	}

	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);
}

int64_t CGameControllerDDRace::GetMaskForPlayerWorldEvent(int Asker, int ExceptID)
{
	if(Asker == -1)
		return CmaskAllExceptOne(ExceptID);

	return m_Teams.TeamMask(GetPlayerTeam(Asker), ExceptID, Asker);
}

void CGameControllerDDRace::InitTeleporter()
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
				m_TeleOuts[Number - 1].push_back(
					vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
			else if(Type == TILE_TELECHECKOUT)
			{
				m_TeleCheckOuts[Number - 1].push_back(
					vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
		}
	}
}

int CGameControllerDDRace::GetPlayerTeam(int ClientID) const
{
	return m_Teams.m_Core.Team(ClientID);
}
