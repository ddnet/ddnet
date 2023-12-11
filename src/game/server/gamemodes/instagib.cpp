#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "instagib.h"

CGameControllerInstagib::CGameControllerInstagib(class CGameContext *pGameServer) :
	CGameControllerDDRace(pGameServer)
{
	m_pGameType = "instagib";

	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;
}

CGameControllerInstagib::~CGameControllerInstagib() = default;

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
	return 0;
}

void CGameControllerInstagib::Tick()
{
	CGameControllerDDRace::Tick();

	if(Config()->m_SvPlayerReadyMode && GameServer()->m_World.m_Paused)
		if(Server()->Tick() % Server()->TickSpeed() * 5 == 0)
			GameServer()->PlayerReadyStateBroadcast();

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
	// TODO: gctf cfg team damage
	// if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCID(), From) && !g_Config.m_SvTeamdamage)
	if(!GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCID(), From))
	{
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
	}

	return false;
}

void CGameControllerInstagib::OnCharacterSpawn(class CCharacter *pChr)
{
	pChr->SetTeams(&Teams());
	Teams().OnCharacterSpawn(pChr->GetPlayer()->GetCID());

	// default health
	pChr->IncreaseHealth(10);

	pChr->SetTeleports(&m_TeleOuts, &m_TeleCheckOuts);
}

int CGameControllerInstagib::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(g_Config.m_SvKillingspreeKills > 0 && pKiller && pVictim)
	{
		if(pKiller->GetCharacter() && pKiller != pVictim->GetPlayer())
			AddSpree(pKiller);
		EndSpree(pVictim->GetPlayer(), pKiller);
	}

	return 0;
}

void CGameControllerInstagib::AddSpree(class CPlayer * pPlayer)
{
	pPlayer->m_Spree++;
	const int NumMsg = 5;
	char aBuf[128];

	if(pPlayer->m_Spree % g_Config.m_SvKillingspreeKills == 0)
	{
		static const char aaSpreeMsg[NumMsg][32] = { "is on a killing spree", "is on a rampage", "is dominating", "is unstoppable", "is godlike"};
		int No = pPlayer->m_Spree/g_Config.m_SvKillingspreeKills;

		str_format(aBuf, sizeof(aBuf), "'%s' %s with %d kills!", Server()->ClientName(pPlayer->GetCID()), aaSpreeMsg[(No > NumMsg-1) ? NumMsg-1 : No], pPlayer->m_Spree);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
}

void CGameControllerInstagib::EndSpree(class CPlayer * pPlayer, class CPlayer *pKiller)
{
	if(pPlayer->m_Spree >= g_Config.m_SvKillingspreeKills)
	{
		CCharacter * charac = pPlayer->GetCharacter();

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
	IGameController::OnPlayerConnect(pPlayer);
	int ClientID = pPlayer->GetCID();

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

	CheckReadyStates(); // gctf
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

	// gctf
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

	int OldTeam = pPlayer->GetTeam(); // gctf
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

	// gctf

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
