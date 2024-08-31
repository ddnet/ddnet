#include <base/system.h>
#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_pvp.h"

CGameControllerPvp::CGameControllerPvp(class CGameContext *pGameServer) :
	CGameControllerDDRace(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;

	m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
	m_AllowSkinChange = true;

	GameServer()->Tuning()->Set("gun_curvature", 1.25f);
	GameServer()->Tuning()->Set("gun_speed", 2200);
	GameServer()->Tuning()->Set("shotgun_curvature", 1.25f);
	GameServer()->Tuning()->Set("shotgun_speed", 2750);
	GameServer()->Tuning()->Set("shotgun_speeddiff", 0.8f);
}

CGameControllerPvp::~CGameControllerPvp() = default;

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

void CGameControllerPvp::UpdateSpawnWeapons()
{
	const char *pWeapons = Config()->m_SvSpawnWeapons;
	if(!str_comp_nocase(pWeapons, "grenade"))
		m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
	else if(!str_comp_nocase(pWeapons, "laser") || !str_comp_nocase(pWeapons, "rifle"))
		m_SpawnWeapons = SPAWN_WEAPON_LASER;
	else
	{
		dbg_msg("ddnet-insta", "warning invalid spawn weapon falling back to grenade");
		m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
	}
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
	if(pKiller == pVictim->GetPlayer())
		pVictim->GetPlayer()->DecrementScore(); // suicide or world
	else
	{
		if(IsTeamplay() && pVictim->GetPlayer()->GetTeam() == pKiller->GetTeam())
			pKiller->DecrementScore(); // teamkill
		else
			pKiller->IncrementScore(); // normal kill
	}
	if(Weapon == WEAPON_SELF)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 3.0f;

	// update spectator modes for dead players in survival
	// if(m_GameFlags&GAMEFLAG_SURVIVAL)
	// {
	// 	for(int i = 0; i < MAX_CLIENTS; ++i)
	// 		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_DeadSpecMode)
	// 			GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
	// }

	// selfkill is no kill
	if(pKiller != pVictim->GetPlayer())
		pKiller->m_Kills++;
	// but selfkill is a death
	pVictim->GetPlayer()->m_Deaths++;

	if(pKiller && pVictim)
	{
		if(pKiller->GetCharacter() && pKiller != pVictim->GetPlayer())
			AddSpree(pKiller);
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
		if(Server()->Tick() % Server()->TickSpeed() * 5 == 0)
			GameServer()->PlayerReadyStateBroadcast();

		// checks that only need to happen every second
		// and not every tick
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			CheckForceUnpauseGame();
		}
	}

	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
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
	//call anticamper
	if(g_Config.m_SvAnticamper && !GameServer()->m_World.m_Paused)
		Anticamper();
}

bool CGameControllerPvp::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(Character.m_IsGodmode)
		return true;
	if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCid(), From))
		return false;
	if(g_Config.m_SvOnlyHookKills && From >= 0 && From <= MAX_CLIENTS)
	{
		CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
		if(!pChr || pChr->GetCore().HookedPlayer() != Character.GetPlayer()->GetCid())
			return false;
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

		if(From >= 0 && From != Character.GetPlayer()->GetCid() && GameServer()->m_apPlayers[From])
		{
			CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
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
			GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
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

void CGameControllerPvp::SetSpawnWeapons(class CCharacter *pChr) const
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

	if(g_Config.m_SvKillingspreeKills > 0 && pPlayer->m_Spree % g_Config.m_SvKillingspreeKills == 0)
	{
		static const char aaSpreeMsg[NumMsg][32] = {"is on a killing spree", "is on a rampage", "is dominating", "is unstoppable", "is godlike"};
		int No = pPlayer->m_Spree / g_Config.m_SvKillingspreeKills;

		str_format(aBuf, sizeof(aBuf), "'%s' %s with %d kills!", Server()->ClientName(pPlayer->GetCid()), aaSpreeMsg[(No > NumMsg - 1) ? NumMsg - 1 : No], pPlayer->m_Spree);
		GameServer()->SendChat(-1, TEAM_ALL, aBuf);
	}
}

void CGameControllerPvp::EndSpree(class CPlayer *pPlayer, class CPlayer *pKiller)
{
	if(g_Config.m_SvKillingspreeKills > 0 && pPlayer->m_Spree >= g_Config.m_SvKillingspreeKills)
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
				Server()->ClientName(pPlayer->GetCid()), pPlayer->m_Spree, Server()->ClientName(pKiller->GetCid()));
			GameServer()->SendChat(-1, TEAM_ALL, aBuf);
		}
	}
	// pPlayer->m_GotAward = false;
	pPlayer->m_Spree = 0;
}

void CGameControllerPvp::OnPlayerConnect(CPlayer *pPlayer)
{
	OnPlayerConstruct(pPlayer);
	IGameController::OnPlayerConnect(pPlayer);
	int ClientId = pPlayer->GetCid();
	pPlayer->ResetStats();

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
	pPlayer->SetTeam(Team);
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
	if(g_Config.m_SvGrenadeAmmoRegenResetOnFire)
		Character.m_Core.m_aWeapons[Character.m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
	if(Character.m_Core.m_aWeapons[Character.m_Core.m_ActiveWeapon].m_Ammo > 0) // -1 == unlimited
		Character.m_Core.m_aWeapons[Character.m_Core.m_ActiveWeapon].m_Ammo--;
	return false;
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
		if(pPlayer->m_Spree <= Spree)
			continue;

		ClientId = pPlayer->GetCid();
		Spree = pPlayer->m_Spree;
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
