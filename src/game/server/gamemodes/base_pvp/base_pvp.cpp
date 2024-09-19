#include <base/system.h>
#include <engine/server/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/entities/ddnet_pvp/vanilla_projectile.h>
#include <game/server/gamecontroller.h>
#include <game/server/instagib/sql_stats.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_pvp.h"

CGameControllerPvp::CGameControllerPvp(class CGameContext *pGameServer) :
	CGameControllerDDRace(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;

	UpdateSpawnWeapons(true);

	m_AllowSkinChange = true;

	GameServer()->Tuning()->Set("gun_curvature", 1.25f);
	GameServer()->Tuning()->Set("gun_speed", 2200);
	GameServer()->Tuning()->Set("shotgun_curvature", 1.25f);
	GameServer()->Tuning()->Set("shotgun_speed", 2750);
	GameServer()->Tuning()->Set("shotgun_speeddiff", 0.8f);

	dbg_msg("ddnet-insta", "connecting to database ...");
	// set the stats table to the gametype name in all lowercase
	// if you want to track stats in a sql database for that gametype
	m_pStatsTable = "";
	m_pSqlStats = new CSqlStats(GameServer(), ((CServer *)Server())->DbPool());
	m_pExtraColumns = nullptr;
	m_pSqlStats->SetExtraColumns(m_pExtraColumns);
}

void CGameControllerPvp::OnRoundStart()
{
	dbg_msg("ddnet-insta", "starting new round ...");

	// for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	// {
	// 	if(!pPlayer)
	// 		continue;
	//
	// }
}

CGameControllerPvp::~CGameControllerPvp()
{
	// TODO: we have to make sure to block all operations and save everything if sv_gametype is switched
	//       there should be no data loss no matter in which state and how often the controller is recreated
	//
	//       this also has to save player sprees that were not ended yet!
	dbg_msg("ddnet-insta", "cleaning up database connection ...");
	if(m_pSqlStats)
	{
		delete m_pSqlStats;
		m_pSqlStats = nullptr;
	}

	if(m_pExtraColumns)
	{
		delete m_pExtraColumns;
		m_pExtraColumns = nullptr;
	}
}

int CGameControllerPvp::GameInfoExFlags(int SnappingClient)
{
	int Flags =
		GAMEINFOFLAG_PREDICT_VANILLA | // ddnet-insta
		GAMEINFOFLAG_ENTITIES_VANILLA | // ddnet-insta
		GAMEINFOFLAG_BUG_VANILLA_BOUNCE | // ddnet-insta
		GAMEINFOFLAG_GAMETYPE_VANILLA | // ddnet-insta
		/* GAMEINFOFLAG_TIMESCORE | */ // ddnet-insta
		/* GAMEINFOFLAG_GAMETYPE_RACE | */ // ddnet-insta
		/* GAMEINFOFLAG_GAMETYPE_DDRACE | */ // ddnet-insta
		/* GAMEINFOFLAG_GAMETYPE_DDNET | */ // ddnet-insta
		GAMEINFOFLAG_UNLIMITED_AMMO |
		GAMEINFOFLAG_RACE_RECORD_MESSAGE |
		GAMEINFOFLAG_ALLOW_EYE_WHEEL |
		GAMEINFOFLAG_ALLOW_HOOK_COLL |
		GAMEINFOFLAG_ALLOW_ZOOM |
		GAMEINFOFLAG_BUG_DDRACE_GHOST |
		GAMEINFOFLAG_BUG_DDRACE_INPUT |
		GAMEINFOFLAG_PREDICT_DDRACE |
		GAMEINFOFLAG_PREDICT_DDRACE_TILES |
		GAMEINFOFLAG_ENTITIES_DDNET |
		GAMEINFOFLAG_ENTITIES_DDRACE |
		GAMEINFOFLAG_ENTITIES_RACE |
		GAMEINFOFLAG_RACE;
	if(!g_Config.m_SvAllowZoom) //ddnet-insta
		Flags &= ~(GAMEINFOFLAG_ALLOW_ZOOM);
	if(g_Config.m_SvFastcap) //ddnet-insta
	{
		Flags |= GAMEINFOFLAG_GAMETYPE_FASTCAP;
		Flags |= GAMEINFOFLAG_FLAG_STARTS_RACE;
	}

	// ddnet clients do not predict sv_old_laser correctly
	// https://github.com/ddnet/ddnet/issues/7589
	if(g_Config.m_SvOldLaser)
		Flags &= ~(GAMEINFOFLAG_PREDICT_DDRACE);

	return Flags;
}

int CGameControllerPvp::GameInfoExFlags2(int SnappingClient)
{
	return GAMEINFOFLAG2_HUD_AMMO | GAMEINFOFLAG2_HUD_HEALTH_ARMOR; // ddnet-insta
}

void CGameControllerPvp::OnShowStatsAll(const CSqlStatsPlayer *pStats, class CPlayer *pRequestingPlayer, const char *pRequestedName)
{
	char aBuf[1024];
	str_format(
		aBuf,
		sizeof(aBuf),
		"'%s' kills: %d, requested by '%s'",
		pRequestedName, pStats->m_Kills, Server()->ClientName(pRequestingPlayer->GetCid()));
	GameServer()->SendChat(-1, TEAM_ALL, aBuf);
}

void CGameControllerPvp::OnShowRank(int Rank, int RankedScore, const char *pRankType, class CPlayer *pRequestingPlayer, const char *pRequestedName)
{
	char aBuf[1024];
	str_format(
		aBuf,
		sizeof(aBuf),
		"%d. '%s' %s: %d, requested by '%s'",
		Rank, pRequestedName, pRankType, RankedScore, Server()->ClientName(pRequestingPlayer->GetCid()));
	GameServer()->SendChat(-1, TEAM_ALL, aBuf);
}

void CGameControllerPvp::OnUpdateSpectatorVotesConfig()
{
	// spec votes was activated
	// spoof all specatators to in game dead specs for 0.7
	// so the client side knows it can call votes
	if(g_Config.m_SvSpectatorVotes && g_Config.m_SvSpectatorVotesSixup)
	{
		for(CPlayer *pPlayer : GameServer()->m_apPlayers)
		{
			if(!pPlayer)
				continue;
			if(pPlayer->GetTeam() != TEAM_SPECTATORS)
				continue;
			if(!Server()->IsSixup(pPlayer->GetCid()))
				continue;

			// Every sixup client only needs to see it self as spectator
			// It does not care about others
			protocol7::CNetMsg_Sv_Team Msg;
			Msg.m_ClientId = pPlayer->GetCid();
			Msg.m_Team = TEAM_RED; // fake
			Msg.m_Silent = true;
			Msg.m_CooldownTick = 0;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, pPlayer->GetCid());

			pPlayer->m_IsFakeDeadSpec = true;
		}
	}
	else
	{
		// spec votes were deactivated
		// so revert spoofed in game teams back to regular spectators
		// make sure this does not mess with ACTUAL dead spec tees
		for(CPlayer *pPlayer : GameServer()->m_apPlayers)
		{
			if(!pPlayer)
				continue;
			if(!Server()->IsSixup(pPlayer->GetCid()))
				continue;
			if(!pPlayer->m_IsFakeDeadSpec)
				continue;

			if(pPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				dbg_msg("ddnet-insta", "ERROR: tried to move player back to team=%d but expected spectators", pPlayer->GetTeam());
			}

			protocol7::CNetMsg_Sv_Team Msg;
			Msg.m_ClientId = pPlayer->GetCid();
			Msg.m_Team = pPlayer->GetTeam(); // restore real team
			Msg.m_Silent = true;
			Msg.m_CooldownTick = 0;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, pPlayer->GetCid());

			pPlayer->m_IsFakeDeadSpec = false;
		}
	}
}

bool CGameControllerPvp::IsWinner(const CPlayer *pPlayer, char *pMessage, int SizeOfMessage)
{
	if(pMessage && SizeOfMessage)
		pMessage[0] = '\0';

	// you can only win on round end
	if(GameState() != IGS_END_ROUND)
		return false;
	if(pPlayer->GetTeam() == TEAM_SPECTATORS)
		return false;

	if(IsTeamplay())
	{
		return m_aTeamscore[pPlayer->GetTeam()] > m_aTeamscore[!pPlayer->GetTeam()];
	}
	else
	{
		int OwnScore = pPlayer->m_Score.value_or(0);
		if(!OwnScore)
			return false;

		int Topscore = 0;
		for(auto &pOtherPlayer : GameServer()->m_apPlayers)
		{
			if(!pOtherPlayer)
				continue;
			int Score = pOtherPlayer->m_Score.value_or(0);
			if(Score > Topscore)
				Topscore = Score;
		}
		return OwnScore >= Topscore;
	}

	return false;
}

bool CGameControllerPvp::IsLoser(const CPlayer *pPlayer)
{
	// TODO: we could relax this a bit
	//       and not count every disconnect during a running round as loss
	//       for example when you or your team is leading or its currently a draw
	//       then it could be neither a win or a loss to quit mid round
	//
	//       the draw case also covers quitting a few seconds after round start
	//       which is common for users who connected to the wrong server
	//       or users that quit after the previous round ended
	if(GameState() != IGS_END_ROUND)
		return true;
	if(pPlayer->GetTeam() == TEAM_SPECTATORS)
		return false;

	return !IsWinner(pPlayer, 0, 0);
}

bool CGameControllerPvp::OnVoteNetMessage(const CNetMsg_Cl_Vote *pMsg, int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];

	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		// SendChatTarget(ClientId, "Spectators aren't allowed to vote.");
		return true;
	}
	return false;
}

// called before spam protection on client team join request
bool CGameControllerPvp::OnSetTeamNetMessage(const CNetMsg_Cl_SetTeam *pMsg, int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;

	int Team = pMsg->m_Team;

	// user joins the spectators while allow spec is on
	// we have to mark him as fake dead spec
	if(Server()->IsSixup(ClientId) && g_Config.m_SvSpectatorVotes && g_Config.m_SvSpectatorVotesSixup && !pPlayer->m_IsFakeDeadSpec)
	{
		if(Team == TEAM_SPECTATORS)
		{
			pPlayer->m_IsFakeDeadSpec = true;
			return false;
		}
	}

	if(Server()->IsSixup(ClientId) && g_Config.m_SvSpectatorVotes && pPlayer->m_IsFakeDeadSpec)
	{
		if(Team != TEAM_SPECTATORS)
		{
			// This should be in all cases coming from the hacked recursion branch below
			//
			// the 0.7 client should think it is in game
			// so it should never display a join game button
			// only a join spectators button
			return false;
		}

		pPlayer->m_IsFakeDeadSpec = false;

		// hijack and drop
		// and then call it again
		// as a hack to edit the team
		CNetMsg_Cl_SetTeam Msg;
		Msg.m_Team = TEAM_RED;
		GameServer()->OnSetTeamNetMessage(&Msg, ClientId);
		return true;
	}
	return false;
}

int CGameControllerPvp::GetPlayerTeam(class CPlayer *pPlayer, bool Sixup)
{
	if(g_Config.m_SvTournament)
		return IGameController::GetPlayerTeam(pPlayer, Sixup);

	// hack to let 0.7 players vote as spectators
	if(g_Config.m_SvSpectatorVotes && g_Config.m_SvSpectatorVotesSixup && Sixup && pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		return TEAM_RED;
	}

	return IGameController::GetPlayerTeam(pPlayer, Sixup);
}

int CGameControllerPvp::GetAutoTeam(int NotThisId)
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

void CGameControllerPvp::SendChatTarget(int To, const char *pText, int Flags) const
{
	GameServer()->SendChatTarget(To, pText, Flags);
}

void CGameControllerPvp::SendChat(int ClientId, int Team, const char *pText, int SpamProtectionClientId, int Flags)
{
	GameServer()->SendChat(ClientId, Team, pText, SpamProtectionClientId, Flags);
}

void CGameControllerPvp::UpdateSpawnWeapons(bool Silent)
{
	// these gametypes are weapon bound
	// so the always overwrite sv_spawn_weapons
	if(m_pGameType[0] == 'g' // gDM, gCTF
		|| m_pGameType[0] == 'i' // iDM, iCTF
		|| m_pGameType[0] == 'C' // CTF*
		|| m_pGameType[0] == 'T' // TDM*
		|| m_pGameType[0] == 'D') // DM*
	{
		if(!Silent)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ddnet-insta", "WARNING: sv_spawn_weapons only has an effect in zCatch");
		}
	}
	if(str_find_nocase(m_pGameType, "fng"))
	{
		if(!Silent)
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ddnet-insta", "WARNING: use sv_gametype fng/solofng/bolofng/boomfng to change weapons in fng");
		return;
	}

	const char *pWeapons = Config()->m_SvSpawnWeapons;
	if(!str_comp_nocase(pWeapons, "grenade"))
		m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
	else if(!str_comp_nocase(pWeapons, "laser") || !str_comp_nocase(pWeapons, "rifle"))
		m_SpawnWeapons = SPAWN_WEAPON_LASER;
	else
	{
		dbg_msg("ddnet-insta", "WARNING: invalid spawn weapon falling back to grenade");
		m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
	}

	m_DefaultWeapon = GetDefaultWeaponBasedOnSpawnWeapons();
}

void CGameControllerPvp::ModifyWeapons(IConsole::IResult *pResult, void *pUserData,
	int Weapon, bool Remove)
{
	CGameControllerPvp *pSelf = (CGameControllerPvp *)pUserData;
	CCharacter *pChr = GameServer()->GetPlayerChar(pResult->m_ClientId);
	if(!pChr)
		return;

	if(clamp(Weapon, -1, NUM_WEAPONS - 1) != Weapon)
	{
		pSelf->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"invalid weapon id");
		return;
	}

	if(Weapon == -1)
	{
		pChr->GiveWeapon(WEAPON_SHOTGUN, Remove);
		pChr->GiveWeapon(WEAPON_GRENADE, Remove);
		pChr->GiveWeapon(WEAPON_LASER, Remove);
	}
	else
	{
		pChr->GiveWeapon(Weapon, Remove);
	}

	pChr->m_DDRaceState = DDRACE_CHEAT;
}

int CGameControllerPvp::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	CGameControllerDDRace::OnCharacterDeath(pVictim, pKiller, Weapon);

	// do scoreing
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;

	if(Weapon == WEAPON_SELF)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 3.0f;

	// never count score or win rounds in ddrace teams
	if(GameServer()->GetDDRaceTeam(pKiller->GetCid()))
		return 0;
	if(GameServer()->GetDDRaceTeam(pVictim->GetPlayer()->GetCid()))
		return 0;

	if(pKiller == pVictim->GetPlayer())
		pVictim->GetPlayer()->DecrementScore(); // suicide or world
	else
	{
		if(IsTeamplay() && pVictim->GetPlayer()->GetTeam() == pKiller->GetTeam())
			pKiller->DecrementScore(); // teamkill
		else
			pKiller->IncrementScore(); // normal kill
	}

	// update spectator modes for dead players in survival
	// if(m_GameFlags&GAMEFLAG_SURVIVAL)
	// {
	// 	for(int i = 0; i < MAX_CLIENTS; ++i)
	// 		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_DeadSpecMode)
	// 			GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
	// }

	// selfkill is no kill
	if(pKiller != pVictim->GetPlayer())
		pKiller->m_Stats.m_Kills++;
	// but selfkill is a death
	pVictim->GetPlayer()->m_Stats.m_Deaths++;

	if(pKiller && pVictim)
	{
		if(pKiller->GetCharacter() && pKiller != pVictim->GetPlayer())
		{
			AddSpree(pKiller);
			if(g_Config.m_SvOnFireMode && Weapon == WEAPON_LASER)
			{
				pKiller->GetCharacter()->m_ReloadTimer = 10;
			}
		}
		EndSpree(pVictim->GetPlayer(), pKiller);
	}
	return 0;
}

void CGameControllerPvp::CheckForceUnpauseGame()
{
	if(!Config()->m_SvForceReadyAll)
		return;
	if(m_GamePauseStartTime == -1)
		return;

	const int MinutesPaused = ((time_get() - m_GamePauseStartTime) / time_freq()) / 60;
	// dbg_msg("insta", "paused since %d minutes", MinutesPaused);

	// const int SecondsPausedDebug = (time_get() - m_GamePauseStartTime) / time_freq();
	// dbg_msg("insta", "paused since %d seconds [DEBUG ONLY REMOVE THIS]", SecondsPausedDebug);

	const int64_t ForceUnpauseTime = m_GamePauseStartTime + (Config()->m_SvForceReadyAll * 60 * time_freq());
	// dbg_msg("insta", "    ForceUnpauseTime=%ld secs=%ld secs_diff_now=%ld [DEBUG ONLY REMOVE THIS]", ForceUnpauseTime, ForceUnpauseTime / time_freq(), (time_get() - ForceUnpauseTime) / time_freq());
	// dbg_msg("insta", "m_GamePauseStartTime=%ld secs=%ld secs_diff_now=%ld [DEBUG ONLY REMOVE THIS]", m_GamePauseStartTime, m_GamePauseStartTime / time_freq(), (time_get() - m_GamePauseStartTime) / time_freq());
	const int SecondsUntilForceUnpause = (ForceUnpauseTime - time_get()) / time_freq();
	// dbg_msg("insta", "seconds until force unpause %d [DEBUG ONLY REMOVE THIS]", SecondsUntilForceUnpause);

	char aBuf[512];
	aBuf[0] = '\0';
	if(SecondsUntilForceUnpause == 60)
		str_copy(aBuf, "Game will be force unpaused in 1 minute.");
	else if(SecondsUntilForceUnpause == 10)
		str_copy(aBuf, "Game will be force unpaused in 10 seconds.");
	if(aBuf[0])
	{
		GameServer()->SendChat(-1, TEAM_ALL, aBuf);
	}

	if(MinutesPaused >= Config()->m_SvForceReadyAll)
	{
		GameServer()->SendChat(-1, TEAM_ALL, "Force unpaused the game because the maximum pause time was reached.");
		SetPlayersReadyState(true);
		CheckReadyStates();
	}
}

void CGameControllerPvp::Tick()
{
	CGameControllerDDRace::Tick();

	if(Config()->m_SvPlayerReadyMode && GameServer()->m_World.m_Paused)
	{
		if(Server()->Tick() % (Server()->TickSpeed() * 9) == 0)
		{
			GameServer()->PlayerReadyStateBroadcast();
		}

		// checks that only need to happen every second
		// and not every tick
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			CheckForceUnpauseGame();
		}
	}

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		OnPlayerTick(pPlayer);
	}
	//call anticamper
	if(g_Config.m_SvAnticamper && !GameServer()->m_World.m_Paused)
		Anticamper();
}

void CGameControllerPvp::OnPlayerTick(class CPlayer *pPlayer)
{
	pPlayer->InstagibTick();

	if(pPlayer->m_GameStateBroadcast)
	{
		char aBuf[512];
		str_format(
			aBuf,
			sizeof(aBuf),
			"GameState: %s                                                                                                                               ",
			GameStateToStr(GameState()));
		GameServer()->SendBroadcast(aBuf, pPlayer->GetCid());
	}
}

bool CGameControllerPvp::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	CPlayer *pPlayer = Character.GetPlayer();
	if(Character.m_IsGodmode)
		return true;
	if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCid(), From))
	{
		// boosting mates counts neither as hit nor as miss
		pPlayer->m_Stats.m_ShotsFired--;
		Dmg = 0;
		return false;
	}
	if(g_Config.m_SvOnlyHookKills && From >= 0 && From <= MAX_CLIENTS)
	{
		return false;
	}
	CPlayer *pKiller = nullptr;
	if(From >= 0 && From <= MAX_CLIENTS)
		pKiller = GameServer()->m_apPlayers[From];
	if(g_Config.m_SvOnlyHookKills && pKiller)
	{
		CCharacter *pChr = pKiller->GetCharacter();
		if(!pChr || pChr->GetCore().HookedPlayer() != Character.GetPlayer()->GetCid())
			return false;
	}

	if(From == pPlayer->GetCid())
	{
		// self damage counts as boosting
		// so the hit/misses rate should not be affected
		//
		// yes this means that grenade boost kills
		// can get you a accuracy over 100%
		pPlayer->m_Stats.m_ShotsFired--;
	}
	else if(pKiller)
	{
		pKiller->m_Stats.m_ShotsHit++;
	}

	// instagib damage always kills no matter the armor
	// max vanilla weapon damage is katana with 9 dmg
	if(Dmg >= 10)
	{
		Character.SetArmor(0);
		Character.SetHealth(0);
	}

	Character.AddHealth(-Dmg);

	// check for death
	if(Character.Health() <= 0)
	{
		Character.Die(From, Weapon);

		if(From != Character.GetPlayer()->GetCid() && pKiller)
		{
			CCharacter *pChr = pKiller->GetCharacter();
			if(pChr)
			{
				// set attacker's face to happy (taunt!)
				pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
			}

			// do damage Hit sound
			CClientMask Mask = CClientMask().set(From);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorId == From)
					Mask.set(i);
			}
			GameServer()->CreateSound(pKiller->m_ViewPos, SOUND_HIT, Mask);
		}
		return false;
	}

	/*
	if (Dmg > 2)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);*/

	return false;
}

void CGameControllerPvp::SetSpawnWeapons(class CCharacter *pChr)
{
	switch(CGameControllerPvp::GetSpawnWeapons(pChr->GetPlayer()->GetCid()))
	{
	case SPAWN_WEAPON_LASER:
		pChr->GiveWeapon(WEAPON_LASER, false);
		break;
	case SPAWN_WEAPON_GRENADE:
		pChr->GiveWeapon(WEAPON_GRENADE, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
		break;
	default:
		dbg_msg("zcatch", "invalid sv_spawn_weapons");
		break;
	}
}
int CGameControllerPvp::GetDefaultWeaponBasedOnSpawnWeapons() const
{
	switch(m_SpawnWeapons)
	{
	case SPAWN_WEAPON_LASER:
		return WEAPON_LASER;
		break;
	case SPAWN_WEAPON_GRENADE:
		return WEAPON_GRENADE;
		break;
	default:
		dbg_msg("zcatch", "invalid sv_spawn_weapons");
		break;
	}
	return WEAPON_GUN;
}

void CGameControllerPvp::OnCharacterSpawn(class CCharacter *pChr)
{
	OnCharacterConstruct(pChr);

	pChr->SetTeams(&Teams());
	Teams().OnCharacterSpawn(pChr->GetPlayer()->GetCid());

	// default health
	pChr->IncreaseHealth(10);
}

void CGameControllerPvp::AddSpree(class CPlayer *pPlayer)
{
	pPlayer->m_Spree++;
	const int NumMsg = 5;
	char aBuf[128];

	if(g_Config.m_SvKillingspreeKills > 0 && pPlayer->Spree() % g_Config.m_SvKillingspreeKills == 0)
	{
		static const char aaSpreeMsg[NumMsg][32] = {"is on a killing spree", "is on a rampage", "is dominating", "is unstoppable", "is godlike"};
		int No = pPlayer->Spree() / g_Config.m_SvKillingspreeKills;

		str_format(aBuf, sizeof(aBuf), "'%s' %s with %d kills!", Server()->ClientName(pPlayer->GetCid()), aaSpreeMsg[(No > NumMsg - 1) ? NumMsg - 1 : No], pPlayer->Spree());
		GameServer()->SendChat(-1, TEAM_ALL, aBuf);
	}
}

void CGameControllerPvp::EndSpree(class CPlayer *pPlayer, class CPlayer *pKiller)
{
	if(g_Config.m_SvKillingspreeKills > 0 && pPlayer->Spree() >= g_Config.m_SvKillingspreeKills)
	{
		CCharacter *pChr = pPlayer->GetCharacter();

		if(pChr)
		{
			GameServer()->CreateSound(pChr->m_Pos, SOUND_GRENADE_EXPLODE);
			// GameServer()->CreateExplosion(pChr->m_Pos,  pPlayer->GetCid(), WEAPON_GRENADE, true, -1, -1);
			CNetEvent_Explosion *pEvent = GameServer()->m_Events.Create<CNetEvent_Explosion>(CClientMask());
			if(pEvent)
			{
				pEvent->m_X = (int)pChr->m_Pos.x;
				pEvent->m_Y = (int)pChr->m_Pos.y;
			}

			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "'%s' %d-kills killing spree was ended by %s",
				Server()->ClientName(pPlayer->GetCid()), pPlayer->Spree(), Server()->ClientName(pKiller->GetCid()));
			GameServer()->SendChat(-1, TEAM_ALL, aBuf);
		}
	}
	// pPlayer->m_GotAward = false;

	if(pPlayer->m_Stats.m_BestSpree < pPlayer->Spree())
		pPlayer->m_Stats.m_BestSpree = pPlayer->Spree();
	pPlayer->m_Spree = 0;
}

void CGameControllerPvp::OnPlayerConnect(CPlayer *pPlayer)
{
	OnPlayerConstruct(pPlayer);
	IGameController::OnPlayerConnect(pPlayer);
	int ClientId = pPlayer->GetCid();
	pPlayer->m_Stats.Reset();

	// init the player
	Score()->PlayerData(ClientId)->Reset();

	// Can't set score here as LoadScore() is threaded, run it in
	// LoadScoreThreaded() instead
	Score()->LoadPlayerData(ClientId);

	if(!Server()->ClientPrevIngame(ClientId))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientId), GetTeamName(pPlayer->GetTeam()));
		if(!g_Config.m_SvTournamentJoinMsgs || pPlayer->GetTeam() != TEAM_SPECTATORS)
			GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1, CGameContext::FLAG_SIX);
		else if(g_Config.m_SvTournamentJoinMsgs == 2)
			SendChatSpectators(aBuf, CGameContext::FLAG_SIX);

		GameServer()->SendChatTarget(ClientId, "DDNet-insta https://github.com/ddnet-insta/ddnet-insta/");
		GameServer()->SendChatTarget(ClientId, "DDraceNetwork Mod. Version: " GAME_VERSION);

		GameServer()->AlertOnSpecialInstagibConfigs(ClientId);
		GameServer()->ShowCurrentInstagibConfigsMotd(ClientId);
	}

	CheckReadyStates(); // ddnet-insta

	// update game info
	SendGameInfo(ClientId);
}

void CGameControllerPvp::SendChatSpectators(const char *pMessage, int Flags)
{
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(pPlayer->GetTeam() != TEAM_SPECTATORS)
			continue;
		bool Send = (Server()->IsSixup(pPlayer->GetCid()) && (Flags & CGameContext::FLAG_SIXUP)) ||
			    (!Server()->IsSixup(pPlayer->GetCid()) && (Flags & CGameContext::FLAG_SIX));
		if(!Send)
			continue;

		GameServer()->SendChat(pPlayer->GetCid(), TEAM_ALL, pMessage, -1, Flags);
	}
}

void CGameControllerPvp::OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason)
{
	pPlayer->OnDisconnect();
	int ClientId = pPlayer->GetCid();
	if(Server()->ClientIngame(ClientId))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(ClientId), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(ClientId));
		if(!g_Config.m_SvTournamentJoinMsgs || pPlayer->GetTeam() != TEAM_SPECTATORS)
			GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1, CGameContext::FLAG_SIX);
		else if(g_Config.m_SvTournamentJoinMsgs == 2)
			SendChatSpectators(aBuf, CGameContext::FLAG_SIX);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientId, Server()->ClientName(ClientId));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	// ddnet-insta
	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		--m_aTeamSize[pPlayer->GetTeam()];
	}
}

void CGameControllerPvp::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	int OldTeam = pPlayer->GetTeam(); // ddnet-insta
	pPlayer->SetTeamSpoofed(Team);
	int ClientId = pPlayer->GetCid();

	char aBuf[128];
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(ClientId), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, TEAM_ALL, aBuf, CGameContext::FLAG_SIX);
	}

	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", ClientId, Server()->ClientName(ClientId), Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// OnPlayerInfoChange(pPlayer);

	// ddnet-insta

	if(OldTeam == TEAM_SPECTATORS)
	{
		GameServer()->AlertOnSpecialInstagibConfigs(pPlayer->GetCid());
		GameServer()->ShowCurrentInstagibConfigsMotd(pPlayer->GetCid());
	}

	// update effected game settings
	if(OldTeam != TEAM_SPECTATORS)
	{
		--m_aTeamSize[OldTeam];
		// m_UnbalancedTick = TBALANCE_CHECK;
	}
	if(Team != TEAM_SPECTATORS)
	{
		++m_aTeamSize[Team];
		// m_UnbalancedTick = TBALANCE_CHECK;
		// if(m_GameState == IGS_WARMUP_GAME && HasEnoughPlayers())
		// 	SetGameState(IGS_WARMUP_GAME, 0);
		// pPlayer->m_IsReadyToPlay = !IsPlayerReadyMode();
		// if(m_GameFlags&GAMEFLAG_SURVIVAL)
		// 	pPlayer->m_RespawnDisabled = GetStartRespawnState();
	}
	CheckReadyStates();
}

void CGameControllerPvp::Anticamper()
{
	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		CCharacter *pChr = pPlayer->GetCharacter();

		//Dont do anticamper if there is no character
		if(!pChr)
		{
			pPlayer->m_CampTick = -1;
			pPlayer->m_SentCampMsg = false;
			continue;
		}

		//Dont do anticamper if player is already frozen
		if(pChr->m_FreezeTime > 0 || pChr->GetCore().m_DeepFrozen)
		{
			pPlayer->m_CampTick = -1;
			pPlayer->m_SentCampMsg = false;
			continue;
		}

		int AnticamperTime = g_Config.m_SvAnticamperTime;
		int AnticamperRange = g_Config.m_SvAnticamperRange;

		if(pPlayer->m_CampTick == -1)
		{
			pPlayer->m_CampPos = pChr->m_Pos;
			pPlayer->m_CampTick = Server()->Tick() + Server()->TickSpeed() * AnticamperTime;
		}

		// Check if the player is moving
		if((pPlayer->m_CampPos.x - pChr->m_Pos.x >= (float)AnticamperRange || pPlayer->m_CampPos.x - pChr->m_Pos.x <= -(float)AnticamperRange) || (pPlayer->m_CampPos.y - pChr->m_Pos.y >= (float)AnticamperRange || pPlayer->m_CampPos.y - pChr->m_Pos.y <= -(float)AnticamperRange))
		{
			pPlayer->m_CampTick = -1;
			pPlayer->m_SentCampMsg = false;
		}

		// Send warning to the player
		if(pPlayer->m_CampTick <= Server()->Tick() + Server()->TickSpeed() * 5 && pPlayer->m_CampTick != -1 && !pPlayer->m_SentCampMsg)
		{
			GameServer()->SendBroadcast("ANTICAMPER: Move or die", pPlayer->GetCid());
			pPlayer->m_SentCampMsg = true;
		}

		// Kill him
		if((pPlayer->m_CampTick <= Server()->Tick()) && (pPlayer->m_CampTick > 0))
		{
			if(g_Config.m_SvAnticamperFreeze)
			{
				//Freeze player
				pChr->Freeze(g_Config.m_SvAnticamperFreeze);
				GameServer()->CreateSound(pChr->m_Pos, SOUND_PLAYER_PAIN_LONG);

				//Reset anticamper
				pPlayer->m_CampTick = -1;
				pPlayer->m_SentCampMsg = false;

				continue;
			}
			else
			{
				//Kill Player
				pChr->Die(pPlayer->GetCid(), WEAPON_WORLD);

				//Reset counter on death
				pPlayer->m_CampTick = -1;
				pPlayer->m_SentCampMsg = false;

				continue;
			}
		}
	}
}

bool CGameControllerPvp::OnFireWeapon(CCharacter &Character, int &Weapon, vec2 &Direction, vec2 &MouseTarget, vec2 &ProjStartPos)
{
	Character.GetPlayer()->m_Stats.m_ShotsFired++;

	if(g_Config.m_SvGrenadeAmmoRegenResetOnFire)
		Character.m_Core.m_aWeapons[Character.m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
	if(Character.m_Core.m_aWeapons[Character.m_Core.m_ActiveWeapon].m_Ammo > 0) // -1 == unlimited
		Character.m_Core.m_aWeapons[Character.m_Core.m_ActiveWeapon].m_Ammo--;

	if(Weapon == WEAPON_GUN)
	{
		if(!Character.m_Core.m_Jetpack || !Character.m_pPlayer->m_NinjaJetpack || Character.m_Core.m_HasTelegunGun)
		{
			int Lifetime = (int)(Server()->TickSpeed() * Character.GetTuning(Character.m_TuneZone)->m_GunLifetime);

			new CVanillaProjectile(
				Character.GameWorld(),
				WEAPON_GUN, //Type
				Character.m_pPlayer->GetCid(), //Owner
				ProjStartPos, //Pos
				Direction, //Dir
				Lifetime, //Span
				false, //Freeze
				false, //Explosive
				-1, //SoundImpact
				MouseTarget //InitDir
			);

			GameServer()->CreateSound(Character.m_Pos, SOUND_GUN_FIRE, Character.TeamMask()); // NOLINT(clang-analyzer-unix.Malloc)
		}
	}
	else if(Weapon == WEAPON_SHOTGUN)
	{
		int ShotSpread = 2;

		for(int i = -ShotSpread; i <= ShotSpread; ++i)
		{
			float Spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
			float Angle = angle(Direction);
			Angle += Spreading[i + 2];
			float v = 1 - (absolute(i) / (float)ShotSpread);
			float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);

			// TODO: not sure about Dir and InitDir and prediction

			new CVanillaProjectile(
				Character.GameWorld(),
				WEAPON_SHOTGUN, // Type
				Character.GetPlayer()->GetCid(), // Owner
				ProjStartPos, // Pos
				direction(Angle) * Speed, // Dir
				(int)(Server()->TickSpeed() * GameServer()->Tuning()->m_ShotgunLifetime), // Span
				false, // Freeze
				false, // Explosive
				-1, // SoundImpact
				vec2(cosf(Angle), sinf(Angle)) * Speed); // InitDir
		}

		GameServer()->CreateSound(Character.m_Pos, SOUND_SHOTGUN_FIRE);
	}
	else
	{
		return false;
	}

	Character.m_AttackTick = Server()->Tick();

	if(!Character.m_ReloadTimer)
	{
		float FireDelay;
		Character.GetTuning(Character.m_TuneZone)->Get(38 + Character.m_Core.m_ActiveWeapon, &FireDelay);
		Character.m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
	}

	return true;
}

int CGameControllerPvp::NumActivePlayers()
{
	int Active = 0;
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
		if(pPlayer && (pPlayer->GetTeam() != TEAM_SPECTATORS || pPlayer->m_IsDead))
			Active++;
	return Active;
}

int CGameControllerPvp::NumAlivePlayers()
{
	int Alive = 0;
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
		if(pPlayer && pPlayer->GetCharacter())
			Alive++;
	return Alive;
}

int CGameControllerPvp::NumNonDeadActivePlayers()
{
	int Alive = 0;
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
		if(pPlayer && !pPlayer->m_IsDead && pPlayer->GetTeam() != TEAM_SPECTATORS)
			Alive++;
	return Alive;
}

int CGameControllerPvp::GetHighestSpreeClientId()
{
	int ClientId = -1;
	int Spree = 0;
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(pPlayer->Spree() <= Spree)
			continue;

		ClientId = pPlayer->GetCid();
		Spree = pPlayer->Spree();
	}
	return ClientId;
}

int CGameControllerPvp::GetFirstAlivePlayerId()
{
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
		if(pPlayer && pPlayer->GetCharacter())
			return pPlayer->GetCid();
	return -1;
}
