#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_instagib.h"

CGameControllerInstagib::CGameControllerInstagib(class CGameContext *pGameServer) :
	CGameControllerDDRace(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;

	m_SpawnWeapons = SPAWN_WEAPON_GRENADE;

	GameServer()->Console()->Chain("sv_scorelimit", ConchainGameinfoUpdate, this);
	GameServer()->Console()->Chain("sv_timelimit", ConchainGameinfoUpdate, this);
	GameServer()->Console()->Chain("sv_grenade_ammo_regen", ConchainResetInstasettingTees, this);
	GameServer()->Console()->Chain("sv_spawn_weapons", ConchainSpawnWeapons, this);
	GameServer()->Console()->Chain("sv_tournament_chat_smart", ConchainSmartChat, this);
	GameServer()->Console()->Chain("sv_tournament_chat", ConchainTournamentChat, this);

#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help) GameServer()->Console()->Register(name, params, flags, callback, userdata, help);
#include "instagib/rcon_commands.h"
#undef CONSOLE_COMMAND

	// generate callbacks to trigger insta settings update for all instagib configs
	// when one of the insta configs is changed
	// we update the checkboxes [x] in the vote menu
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	GameServer()->Console()->Chain(#ScriptName, ConchainInstaSettingsUpdate, this);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) // only int checkboxes for now
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) // only int checkboxes for now
#include <engine/shared/variables_insta.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR

	// ugly hack to fix https://github.com/ZillyInsta/ddnet-insta/issues/88
	// we load the autoexec again after the insta controller registered its commands
	// loading the autoexec twice might cause some weird bugs so this hack is far from ideal
	// a clean fix requires refactoring ddnet code upstream
	if(GameServer()->Storage()->FileExists(AUTOEXEC_SERVER_FILE, IStorage::TYPE_ALL))
		GameServer()->Console()->ExecuteFile(AUTOEXEC_SERVER_FILE);
	else // fallback
		GameServer()->Console()->ExecuteFile(AUTOEXEC_FILE);
}

CGameControllerInstagib::~CGameControllerInstagib() = default;

void CGameControllerInstagib::SendChatTarget(int To, const char *pText, int Flags) const
{
	GameServer()->SendChatTarget(To, pText, Flags);
}

void CGameControllerInstagib::SendChat(int ClientID, int Team, const char *pText, int SpamProtectionClientID, int Flags)
{
	GameServer()->SendChat(ClientID, Team, pText, SpamProtectionClientID, Flags);
}

void CGameControllerInstagib::ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
		if(pSelf->GameServer()->m_pController)
			pSelf->GameServer()->m_pController->CheckGameInfo();
	}
}

void CGameControllerInstagib::ConchainResetInstasettingTees(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	if(pResult->NumArguments())
	{
		for(auto *pPlayer : pSelf->GameServer()->m_apPlayers)
		{
			if(!pPlayer)
				continue;
			CCharacter *pChr = pPlayer->GetCharacter();
			if(!pChr)
				continue;
			pChr->ResetInstaSettings();
		}
	}
}

void CGameControllerInstagib::ConchainInstaSettingsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	pSelf->GameServer()->UpdateVoteCheckboxes();
	pSelf->GameServer()->RefreshVotes();
}

void CGameControllerInstagib::ModifyWeapons(IConsole::IResult *pResult, void *pUserData,
	int Weapon, bool Remove)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	CCharacter *pChr = GameServer()->GetPlayerChar(pResult->m_ClientID);
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

void CGameControllerInstagib::ConchainSpawnWeapons(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameControllerInstagib *pThis = static_cast<CGameControllerInstagib *>(pUserData);
		const char *pWeapons = pThis->Config()->m_SvSpawnWeapons;
		if(!str_comp_nocase(pWeapons, "grenade"))
			pThis->m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
		else if(!str_comp_nocase(pWeapons, "laser") || !str_comp_nocase(pWeapons, "rifle"))
			pThis->m_SpawnWeapons = SPAWN_WEAPON_LASER;
		else
		{
			dbg_msg("ddnet-insta", "warning invalid spawn weapon falling back to grenade");
			pThis->m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
		}
	}
}

void CGameControllerInstagib::ConchainSmartChat(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);

	if(!pResult->NumArguments())
		return;

	CGameControllerInstagib *pSelf = static_cast<CGameControllerInstagib *>(pUserData);
	char aBuf[512];
	str_format(
		aBuf,
		sizeof(aBuf),
		"Warning: sv_tournament_chat is currently set to %d you might want to update that too.",
		pSelf->Config()->m_SvTournamentChat);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ddnet-insta", aBuf);
}

void CGameControllerInstagib::ConchainTournamentChat(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);

	CGameControllerInstagib *pSelf = static_cast<CGameControllerInstagib *>(pUserData);
	if(!pSelf->Config()->m_SvTournamentChatSmart)
		return;

	pSelf->Console()->Print(
		IConsole::OUTPUT_LEVEL_STANDARD,
		"ddnet-insta",
		"Warning: this variable will be set automatically on round end because sv_tournament_chat_smart is active.");
}

int CGameControllerInstagib::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
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
		m_aInstaPlayerStats[pKiller->GetCID()].m_Kills++;
	// but selfkill is a death
	m_aInstaPlayerStats[pVictim->GetPlayer()->GetCID()].m_Deaths++;

	if(g_Config.m_SvKillingspreeKills > 0 && pKiller && pVictim)
	{
		if(pKiller->GetCharacter() && pKiller != pVictim->GetPlayer())
			AddSpree(pKiller);
		EndSpree(pVictim->GetPlayer(), pKiller);
	}
	return 0;
}

void CGameControllerInstagib::CheckForceUnpauseGame()
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
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	if(MinutesPaused >= Config()->m_SvForceReadyAll)
	{
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Force unpaused the game because the maximum pause time was reached.");
		SetPlayersReadyState(true);
		CheckReadyStates();
	}
}

void CGameControllerInstagib::Tick()
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
			GameServer()->SendBroadcast(aBuf, pPlayer->GetCID());
		}
	}
}

bool CGameControllerInstagib::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(Character.m_IsGodmode)
	{
		Dmg = 0;
		return false;
	}
	// TODO: ddnet-insta cfg team damage
	// if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCID(), From) && !g_Config.m_SvTeamdamage)
	if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCID(), From))
		return false;
	if(From == Character.GetPlayer()->GetCID())
	{
		Dmg = 0;
		//Give back ammo on grenade self push//Only if not infinite ammo and activated
		if(Weapon == WEAPON_GRENADE && g_Config.m_SvGrenadeAmmoRegen && g_Config.m_SvGrenadeAmmoRegenSpeedNade)
		{
			Character.SetWeaponAmmo(WEAPON_GRENADE, minimum(Character.GetCore().m_aWeapons[WEAPON_GRENADE].m_Ammo + 1, g_Config.m_SvGrenadeAmmoRegenNum));
		}
	}

	if(g_Config.m_SvOnlyHookKills && From >= 0 && From <= MAX_CLIENTS)
	{
		CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
		if(!pChr || pChr->GetCore().HookedPlayer() != Character.GetPlayer()->GetCID())
			Dmg = 0;
	}

	int Health = 10;

	// no self damage
	if(Dmg >= g_Config.m_SvDamageNeededForKill)
		Health = From == Character.GetPlayer()->GetCID() ? Health : 0;

	// check for death
	if(Health <= 0)
	{
		Character.Die(From, Weapon);

		if(From >= 0 && From != Character.GetPlayer()->GetCID() && GameServer()->m_apPlayers[From])
		{
			CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
			if(pChr)
			{
				// set attacker's face to happy (taunt!)
				pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());

				// refill nades
				int RefillNades = 0;
				if(g_Config.m_SvGrenadeAmmoRegenOnKill == 1)
					RefillNades = 1;
				else if(g_Config.m_SvGrenadeAmmoRegenOnKill == 2)
					RefillNades = g_Config.m_SvGrenadeAmmoRegenNum;
				if(RefillNades && g_Config.m_SvGrenadeAmmoRegen && Weapon == WEAPON_GRENADE)
				{
					pChr->SetWeaponAmmo(WEAPON_GRENADE, minimum(pChr->GetCore().m_aWeapons[WEAPON_GRENADE].m_Ammo + RefillNades, g_Config.m_SvGrenadeAmmoRegenNum));
				}
			}

			// do damage Hit sound
			CClientMask Mask = CClientMask().set(From);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorID == From)
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

	if(Dmg)
	{
		Character.SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);
	}

	return false;
}

void CGameControllerInstagib::SetSpawnWeapons(class CCharacter *pChr)
{
	switch(CGameControllerInstagib::GetSpawnWeapons(pChr->GetPlayer()->GetCID()))
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

void CGameControllerInstagib::OnCharacterSpawn(class CCharacter *pChr)
{
	OnCharacterConstruct(pChr);

	pChr->SetTeams(&Teams());
	Teams().OnCharacterSpawn(pChr->GetPlayer()->GetCID());

	// default health
	pChr->IncreaseHealth(10);

	pChr->SetTeleports(&m_TeleOuts, &m_TeleCheckOuts);
}

void CGameControllerInstagib::AddSpree(class CPlayer *pPlayer)
{
	pPlayer->m_Spree++;
	const int NumMsg = 5;
	char aBuf[128];

	if(pPlayer->m_Spree % g_Config.m_SvKillingspreeKills == 0)
	{
		static const char aaSpreeMsg[NumMsg][32] = {"is on a killing spree", "is on a rampage", "is dominating", "is unstoppable", "is godlike"};
		int No = pPlayer->m_Spree / g_Config.m_SvKillingspreeKills;

		str_format(aBuf, sizeof(aBuf), "'%s' %s with %d kills!", Server()->ClientName(pPlayer->GetCID()), aaSpreeMsg[(No > NumMsg - 1) ? NumMsg - 1 : No], pPlayer->m_Spree);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
}

void CGameControllerInstagib::EndSpree(class CPlayer *pPlayer, class CPlayer *pKiller)
{
	if(pPlayer->m_Spree >= g_Config.m_SvKillingspreeKills)
	{
		CCharacter *charac = pPlayer->GetCharacter();

		if(charac)
		{
			GameServer()->CreateSound(charac->m_Pos, SOUND_GRENADE_EXPLODE);
			// GameServer()->CreateExplosion(charac->m_Pos,  pPlayer->GetCID(), WEAPON_GRENADE, true, -1, -1);
			CNetEvent_Explosion *pEvent = GameServer()->m_Events.Create<CNetEvent_Explosion>(CClientMask());
			if(pEvent)
			{
				pEvent->m_X = (int)charac->m_Pos.x;
				pEvent->m_Y = (int)charac->m_Pos.y;
			}

			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "'%s' %d-kills killing spree was ended by %s",
				Server()->ClientName(pPlayer->GetCID()), pPlayer->m_Spree, Server()->ClientName(pKiller->GetCID()));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		}
	}
	// pPlayer->m_GotAward = false;
	pPlayer->m_Spree = 0;
}

void CGameControllerInstagib::OnPlayerConnect(CPlayer *pPlayer)
{
	OnPlayerConstruct(pPlayer);
	IGameController::OnPlayerConnect(pPlayer);
	int ClientID = pPlayer->GetCID();
	m_aInstaPlayerStats[ClientID].Reset();

	// init the player
	Score()->PlayerData(ClientID)->Reset();

	// Can't set score here as LoadScore() is threaded, run it in
	// LoadScoreThreaded() instead
	Score()->LoadPlayerData(ClientID);

	if(!Server()->ClientPrevIngame(ClientID))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()));
		if(!g_Config.m_SvTournamentJoinMsgs || pPlayer->GetTeam() != TEAM_SPECTATORS)
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);
		else if(g_Config.m_SvTournamentJoinMsgs == 2)
			SendChatSpectators(aBuf, CGameContext::CHAT_SIX);

		GameServer()->SendChatTarget(ClientID, "DDNet-insta https://github.com/ZillyInsta/ddnet-insta/");
		GameServer()->SendChatTarget(ClientID, "DDraceNetwork Mod. Version: " GAME_VERSION);

		GameServer()->AlertOnSpecialInstagibConfigs(ClientID);
		GameServer()->ShowCurrentInstagibConfigsMotd(ClientID);
	}

	CheckReadyStates(); // ddnet-insta
}

void CGameControllerInstagib::SendChatSpectators(const char *pMessage, int Flags)
{
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(pPlayer->GetTeam() != TEAM_SPECTATORS)
			continue;
		bool Send = (Server()->IsSixup(pPlayer->GetCID()) && (Flags & CGameContext::CHAT_SIXUP)) ||
			    (!Server()->IsSixup(pPlayer->GetCID()) && (Flags & CGameContext::CHAT_SIX));
		if(!Send)
			continue;

		GameServer()->SendChat(pPlayer->GetCID(), CGameContext::CHAT_ALL, pMessage, -1, Flags);
	}
}

void CGameControllerInstagib::OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason)
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
		if(!g_Config.m_SvTournamentJoinMsgs || pPlayer->GetTeam() != TEAM_SPECTATORS)
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);
		else if(g_Config.m_SvTournamentJoinMsgs == 2)
			SendChatSpectators(aBuf, CGameContext::CHAT_SIX);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	// ddnet-insta
	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		--m_aTeamSize[pPlayer->GetTeam()];
	}
}

void CGameControllerInstagib::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	int OldTeam = pPlayer->GetTeam(); // ddnet-insta
	pPlayer->SetTeam(Team);
	int ClientID = pPlayer->GetCID();

	char aBuf[128];
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, CGameContext::CHAT_SIX);
	}

	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", ClientID, Server()->ClientName(ClientID), Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// OnPlayerInfoChange(pPlayer);

	// ddnet-insta

	if(OldTeam == TEAM_SPECTATORS)
	{
		GameServer()->AlertOnSpecialInstagibConfigs(pPlayer->GetCID());
		GameServer()->ShowCurrentInstagibConfigsMotd(pPlayer->GetCID());
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
