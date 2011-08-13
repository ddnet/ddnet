/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/shared/config.h>
#include <engine/server/server.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/generated/nethash.cpp>
#if defined(CONF_SQL)
	#include <game/server/score/sql_score.h>
#endif

void CGameContext::ConGoLeft(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	pSelf->MoveCharacter(pSelf->m_ClientID, -1, 0);
}

void CGameContext::ConGoRight(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	pSelf->MoveCharacter(pSelf->m_ClientID, 1, 0);
}

void CGameContext::ConGoDown(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	pSelf->MoveCharacter(pSelf->m_ClientID, 0, 1);
}

void CGameContext::ConGoUp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	pSelf->MoveCharacter(pSelf->m_ClientID, 0, -1);
}

void CGameContext::ConMove(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	pSelf->MoveCharacter(pSelf->m_ClientID, pResult->GetInteger(0), pResult->GetInteger(1));
}

void CGameContext::ConMoveRaw(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	pSelf->MoveCharacter(pSelf->m_ClientID, pResult->GetInteger(0), pResult->GetInteger(1), true);
}

void CGameContext::MoveCharacter(int ClientID, int X, int Y, bool Raw)
{
	CCharacter* pChr = GetPlayerChar(ClientID);

	if(!pChr)
		return;

	pChr->Core()->m_Pos.x += ((Raw) ? 1 : 32) * X;
	pChr->Core()->m_Pos.y += ((Raw) ? 1 : 32) * Y;
	pChr->m_DDRaceState = DDRACE_CHEAT;
}


void CGameContext::ConKillPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	int Victim = pResult->GetInteger(0);

	if(pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->KillCharacter(WEAPON_GAME);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s was killed by %s", pSelf->Server()->ClientName(Victim), pSelf->Server()->ClientName(pSelf->m_ClientID));
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
}

void CGameContext::ConNinja(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_NINJA, false);
}

void CGameContext::ConSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	CCharacter* pChr = pSelf->GetPlayerChar(pSelf->m_ClientID);
	if(pChr && !pChr->m_Super)
	{
		pChr->m_Super = true;
		pChr->UnFreeze();
		pChr->m_TeamBeforeSuper = pChr->Team();
		pChr->Teams()->SetCharacterTeam(pSelf->m_ClientID, TEAM_SUPER);
		pChr->m_DDRaceState = DDRACE_CHEAT;
	}
}

void CGameContext::ConUnSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	CCharacter* pChr = pSelf->GetPlayerChar(pSelf->m_ClientID);
	if(pChr && pChr->m_Super)
	{
		pChr->m_Super = false;
		pChr->Teams()->SetForceCharacterTeam(pSelf->m_ClientID, pChr->m_TeamBeforeSuper);
	}
}

void CGameContext::ConShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, false);
}

void CGameContext::ConGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, false);
}

void CGameContext::ConRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_RIFLE, false);
}

void CGameContext::ConWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, false);
}

void CGameContext::ConUnShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, true);
}

void CGameContext::ConUnGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, true);
}

void CGameContext::ConUnRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_RIFLE, true);
}

void CGameContext::ConUnWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, true);
}

void CGameContext::ConAddWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), false);
}

void CGameContext::ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), true);
}

void CGameContext::ModifyWeapons(IConsole::IResult *pResult, void *pUserData, int Weapon, bool Remove)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	int ClientID = pSelf->m_ClientID;
	if(clamp(Weapon, -1, NUM_WEAPONS - 1) != Weapon)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "invalid weapon id");
		return;
	}

	CCharacter* pChr = GetPlayerChar(ClientID);
	if(!pChr)
		return;

	if(Weapon == -1)
	{
		if(Remove && (pChr->GetActiveWeapon() == WEAPON_SHOTGUN || pChr->GetActiveWeapon() == WEAPON_GRENADE || pChr->GetActiveWeapon() == WEAPON_RIFLE))
			pChr->SetActiveWeapon(WEAPON_GUN);

		if(Remove)
		{
			pChr->SetWeaponGot(WEAPON_SHOTGUN, false);
			pChr->SetWeaponGot(WEAPON_GRENADE, false);
			pChr->SetWeaponGot(WEAPON_RIFLE, false);
		}
		else
			pChr->GiveAllWeapons();
	}
	else if(Weapon != WEAPON_NINJA)
	{
		if(Remove && pChr->GetActiveWeapon() == Weapon)
			pChr->SetActiveWeapon(WEAPON_GUN);

		if(Remove)
			pChr->SetWeaponGot(Weapon, false);
		else
			pChr->GiveWeapon(Weapon, -1);
	}
	else
	{
		if(Remove)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "you can't remove ninja");
			return;
		}

		pChr->GiveNinja();
	}

	pChr->m_DDRaceState =	DDRACE_CHEAT;
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	int TeleTo = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[TeleTo])
	{
		{
			CCharacter* pChr = pSelf->GetPlayerChar(pSelf->m_ClientID);
			if(pChr)
			{
				pChr->Core()->m_Pos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
				pChr->m_DDRaceState = DDRACE_CHEAT;
			}
		}
	}
}

void CGameContext::ConCredits(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Teeworlds Team takes most of the credits also");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "This mod was originally created by \'3DA\'");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Now it is maintained & re-coded by:");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "\'[Egypt]GreYFoX@GTi\' and \'[BlackTee]den\'");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Others Helping on the code: \'heinrich5991\', \'ravomavain\', \'Trust o_0 Aeeeh ?!\', \'noother\', \'<3 fisted <3\' & \'LemonFace\'");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Documentation: Zeta-Hoernchen, Entities: Fisico");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Code (in the past): \'3DA\' and \'Fluxid\'");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Please check the changelog on DDRace.info.");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Also the commit log on github.com/GreYFoXGTi/DDRace.");
}

void CGameContext::ConInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "DDRace Mod. Version: " GAME_VERSION);
#if defined( GIT_SHORTREV_HASH )
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Git revision hash: " GIT_SHORTREV_HASH);
#endif
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Official site: DDRace.info");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "For more Info /cmdlist");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Or visit DDRace.info");
}

void CGameContext::ConHelp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "/cmdlist will show a list of all chat commands");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "/help + any command will show you the help for this command");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Example /help settings will display the help about ");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo = pSelf->Console()->GetCommandInfo(pArg, CFGFLAG_SERVER, false);
		if(pCmdInfo && pCmdInfo->m_pHelp)
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", pCmdInfo->m_pHelp);
		else
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Command is either unknown or you have given a blank command without any parameters.");
	}
}

void CGameContext::ConSettings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "to check a server setting say /settings and setting's name, setting names are:");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "teams, cheats, collision, hooking, endlesshooking, me, ");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "hitting, oldlaser, timeout, votes, pause and scores");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		char aBuf[256];
		float ColTemp;
		float HookTemp;
		pSelf->m_Tuning.Get("player_collision", &ColTemp);
		pSelf->m_Tuning.Get("player_hooking", &HookTemp);
		if(str_comp(pArg, "teams") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "%s %s", g_Config.m_SvTeam == 1 ? "Teams are available on this server" : !g_Config.m_SvTeam ? "Teams are not available on this server" : "You have to be in a team to play on this server", /*g_Config.m_SvTeamStrict ? "and if you die in a team all of you die" : */"and if you die in a team only you die");
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
		}
		else if(str_comp(pArg, "collision") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", ColTemp?"Players can collide on this server":"Players Can't collide on this server");
		}
		else if(str_comp(pArg, "hooking") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", HookTemp?"Players can hook each other on this server":"Players Can't hook each other on this server");
		}
		else if(str_comp(pArg, "endlesshooking") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvEndlessDrag?"Players can hook time is unlimited":"Players can hook time is limited");
		}
		else if(str_comp(pArg, "hitting") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvHit?"Player's weapons affect each other":"Player's weapons has no affect on each other");
		}
		else if(str_comp(pArg, "oldlaser") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvOldLaser?"Lasers can hit you if you shot them and that they pull you towards the bounce origin (Like DDRace Beta)":"Lasers can't hit you if you shot them, and they pull others towards the shooter");
		}
		else if(str_comp(pArg, "me") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvSlashMe?"Players can use /me commands the famous IRC Command":"Players Can't use the /me command");
		}
		else if(str_comp(pArg, "timeout") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "The Server Timeout is currently set to %d", g_Config.m_ConnTimeout);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
		}
		else if(str_comp(pArg, "votes") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvVoteKick?"Players can use Callvote menu tab to kick offenders":"Players Can't use the Callvote menu tab to kick offenders");
			if(g_Config.m_SvVoteKick)
				str_format(aBuf, sizeof(aBuf), "Players are banned for %d second(s) if they get voted off", g_Config.m_SvVoteKickBantime);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvVoteKickBantime?aBuf:"Players are just kicked and not banned if they get voted off");
		}
		else if(str_comp(pArg, "pause") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvPauseable?g_Config.m_SvPauseTime?"/pause is available on this server and it pauses your time too":"/pause is available on this server but it doesn't pause your time":"/pause is NOT available on this server");
		}
		else if(str_comp(pArg, "scores") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvHideScore?"Scores are private on this server":"Scores are public on this server");
		}
	}
}

void CGameContext::ConRules(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	bool Printed = false;
	if(g_Config.m_SvDDRaceRules)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "No blocking.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "No insulting / spamming.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "No fun voting / vote spamming.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Breaking any of these rules will result in a penalty, decided by server admins.");
		Printed = true;
	}
	if(g_Config.m_SvRulesLine1[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine1);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine2[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine2);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine3[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine3);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine4[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine4);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine5[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine5);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine6[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine6);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine7[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine7);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine8[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine8);
		Printed=true;
	}
	if(g_Config.m_SvRulesLine9[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine9);
		Printed=true;
	}
	if(g_Config.m_SvRulesLine10[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", g_Config.m_SvRulesLine10);
		Printed = true;
	}
	if(!Printed)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "No Rules Defined, Kill em all!!");
}

void CGameContext::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];

	if(!pPlayer || (pPlayer->m_LastKill && pPlayer->m_LastKill + pSelf->Server()->TickSpeed() * g_Config.m_SvKillDelay > pSelf->Server()->Tick()))
		return;

	pPlayer->m_LastKill = pSelf->Server()->Tick();
	pPlayer->KillCharacter(WEAPON_SELF);
	//pPlayer->m_RespawnTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * g_Config.m_SvSuicidePenalty;
}

void CGameContext::ConTogglePause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	char aBuf[128];

	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
	if(!pPlayer)
		return;

	if(g_Config.m_SvPauseable)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(!pPlayer->GetTeam() && pChr && (!pChr->GetWeaponGot(WEAPON_NINJA) || pChr->m_FreezeTime) && pChr->IsGrounded() && pChr->m_Pos==pChr->m_PrevPos && !pPlayer->m_InfoSaved)
		{
			if(pPlayer->m_LastSetTeam + pSelf->Server()->TickSpeed() * g_Config.m_SvPauseFrequency <= pSelf->Server()->Tick())
			{
				pPlayer->SaveCharacter();
				pPlayer->m_InfoSaved = true;
				pPlayer->SetTeam(TEAM_SPECTATORS);
			}
			else
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You can\'t pause that often.");
		}
		else if(pPlayer->GetTeam()==TEAM_SPECTATORS && pPlayer->m_InfoSaved && pPlayer->m_ForcePauseTime == 0)
		{
			pPlayer->m_PauseInfo.m_Respawn = true;
			pPlayer->SetTeam(TEAM_RED);
			pPlayer->m_InfoSaved = false;
			//pPlayer->LoadCharacter();//TODO:Check if this system Works
		}
		else if(pChr)
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", pChr->GetWeaponGot(WEAPON_NINJA)?"You can't use /pause while you are a ninja":(!pChr->IsGrounded())?"You can't use /pause while you are a in air":"You can't use /pause while you are moving");
		else if(pPlayer->m_ForcePauseTime > 0)
		{
			str_format(aBuf, sizeof(aBuf), "You have been force-paused. %ds left.", pPlayer->m_ForcePauseTime/pSelf->Server()->TickSpeed());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
		}
		else if(pPlayer->m_ForcePauseTime < 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You have been force-paused.");
		}
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "No pause data saved.");
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Pause isn't allowed on this server.");
}

void CGameContext::ConForcePause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;
	CServer* pServ = (CServer*)pSelf->Server();
	int Victim = pResult->GetInteger(0);
	int Seconds = 0;
	char aBuf[128];

	if(pResult->NumArguments() > 0 && pSelf->m_ClientID < 0)
		Seconds = clamp(pResult->GetInteger(0), 0, 15);

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if(!pPlayer || (!Seconds && pSelf->m_ClientID >= 0))
		return;

	CCharacter* pChr = pPlayer->GetCharacter();
	if(!pPlayer->GetTeam() && pChr && !pPlayer->m_InfoSaved && pSelf->m_ClientID < 0)
	{
		pPlayer->SaveCharacter();
		pPlayer->m_InfoSaved = true;
		pPlayer->SetTeam(TEAM_SPECTATORS);
		pPlayer->m_ForcePauseTime = Seconds*pServ->TickSpeed();
	}
	else
	{
		pPlayer->m_ForcePauseTime = Seconds*pServ->TickSpeed();
	}
	if(pSelf->m_ClientID < 0)
		str_format(aBuf, sizeof(aBuf), "'%s' has been force-paused for %d seconds", pServ->ClientName(Victim), Seconds);
	else
		str_format(aBuf, sizeof(aBuf), "Force-pause of '%s' have been removed by '%s'", pServ->ClientName(Victim), pServ->ClientName(pSelf->m_ClientID));
	pSelf->SendChat(-1, CHAT_ALL, aBuf);
}

void CGameContext::ConTop5(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
	if(!pPlayer)
		return;

	if(g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Showing the top 5 is not allowed on this server.");
		return;
	}

		if(pResult->NumArguments() > 0)
			pSelf->Score()->ShowTop5(pResult, pPlayer->GetCID(), pUserData, pResult->GetInteger(0));
		else
			pSelf->Score()->ShowTop5(pResult, pPlayer->GetCID(), pUserData);
}
#if defined(CONF_SQL)
void CGameContext::ConTimes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	if(g_Config.m_SvUseSQL)
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		CSqlScore *pScore = (CSqlScore *)pSelf->Score();
		CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
		if(!pPlayer)
			return;

		if(pResult->NumArguments() == 0)
		{
			pScore->ShowTimes(pPlayer->GetCID(),1);
			return;
		}

		else if(pResult->NumArguments() < 3)
		{
			if (pResult->NumArguments() == 1)
			{
				if(pResult->GetInteger(0) != 0)
					pScore->ShowTimes(pPlayer->GetCID(),pResult->GetInteger(0));
				else
					pScore->ShowTimes(pPlayer->GetCID(), (str_comp(pResult->GetString(0), "me") == 0) ? pSelf->Server()->ClientName(pSelf->m_ClientID) : pResult->GetString(0),1);
				return;
			}
			else if (pResult->GetInteger(1) != 0)
			{
				pScore->ShowTimes(pPlayer->GetCID(), (str_comp(pResult->GetString(0), "me") == 0) ? pSelf->Server()->ClientName(pSelf->m_ClientID) : pResult->GetString(0),pResult->GetInteger(1));
				return;
			}
		}
			
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "/times needs 0, 1 or 2 parameter. 1. = name, 2. = start number");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Example: /times, /times me, /times Hans, /times \"Papa Smurf\" 5");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Bad: /times Papa Smurf 5 # Good: /times \"Papa Smurf\" 5 ");
	}	
}
#endif

void CGameContext::ConRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
	if(!pPlayer)
		return;

	if(pResult->NumArguments() > 0)
		if(!g_Config.m_SvHideScore)
			pSelf->Score()->ShowRank(pSelf->m_ClientID, pResult->GetString(0), true);
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Showing the rank of other players is not allowed on this server.");
	else
		pSelf->Score()->ShowRank(pSelf->m_ClientID, pSelf->Server()->ClientName(pSelf->m_ClientID));
}

void CGameContext::ConJoinTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	if(pSelf->m_VoteCloseTime && pSelf->m_VoteCreator == pSelf->m_ClientID)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You are running a vote please try again after the vote is done!");
		return;
	}
	else if(g_Config.m_SvTeam == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Admin has disabled teams");
		return;
	}
	else if (g_Config.m_SvTeam == 2)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You must join to any team and play with anybody or you will not play");
	}
	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];

	if(pResult->NumArguments() > 0)
	{
		if(pPlayer->GetCharacter() == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You can't change teams while you are dead/a spectator.");
		}
		else
		{
			if(((CGameControllerDDRace*)pSelf->m_pController)->m_Teams.SetCharacterTeam(pPlayer->GetCID(), pResult->GetInteger(0)))
			{
				if(pPlayer->m_Last_Team + pSelf->Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay <= pSelf->Server()->Tick())
				{
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "%s joined team %d", pSelf->Server()->ClientName(pPlayer->GetCID()), pResult->GetInteger(0));
					pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
					pPlayer->m_Last_Team = pSelf->Server()->Tick();
				}
				else
				{
					pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You can\'t join teams that fast!");
				}
			}
			else
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You cannot join this team at this time");
			}
		}
	}
	else
	{
		char aBuf[512];
		if(!pPlayer->IsPlaying())
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "You can't check your team while you are dead/a spectator.");
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "You are in team %d", ((CGameControllerDDRace*)pSelf->m_pController)->m_Teams.m_Core.Team(pSelf->m_ClientID));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
		}
	}
}

void CGameContext::ConMe(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	char aBuf[256 + 24];

	str_format(aBuf, 256 + 24, "'%s' %s", pSelf->Server()->ClientName(pSelf->m_ClientID), pResult->GetString(0));
	if(g_Config.m_SvSlashMe)
		pSelf->SendChat(-2, CGameContext::CHAT_ALL, aBuf, pSelf->m_ClientID);
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "/me is disabled on this server, admin can enable it by using sv_slash_me");
}

void CGameContext::ConToggleEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
	if(!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	pChr->m_EyeEmote = !pChr->m_EyeEmote;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", (pChr->m_EyeEmote) ? "You can now use the preset eye emotes." : "You don't have any eye emotes, remember to bind some. (until you die)");
}

void CGameContext::ConToggleBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
	if(!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	pChr->m_BroadCast = !pChr->m_BroadCast;
}

void CGameContext::ConEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
	if(!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Emote commands are: /emote surprise /emote blink /emote close /emote angry /emote happy /emote pain");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Example: /emote surprise 10 for 10 seconds or /emote surprise (default 1 second)");
	}
	else
	{
		if (pChr)
		{
			if (!str_comp(pResult->GetString(0), "angry"))
				pChr->m_DefEmote = EMOTE_ANGRY;
			else if (!str_comp(pResult->GetString(0), "blink"))
				pChr->m_DefEmote = EMOTE_BLINK;
			else if (!str_comp(pResult->GetString(0), "close"))
				pChr->m_DefEmote = EMOTE_BLINK;
			else if (!str_comp(pResult->GetString(0), "happy"))
				pChr->m_DefEmote = EMOTE_HAPPY;
			else if (!str_comp(pResult->GetString(0), "pain"))
				pChr->m_DefEmote = EMOTE_PAIN;
			else if (!str_comp(pResult->GetString(0), "surprise"))
				pChr->m_DefEmote = EMOTE_SURPRISE;
			else if (!str_comp(pResult->GetString(0), "normal"))
				pChr->m_DefEmote = EMOTE_NORMAL;
			else
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Unknown emote... Say /emote");

			int Duration = 1;
			if (pResult->NumArguments() > 1)
				Duration = pResult->GetInteger(1);

			pChr->m_DefEmoteReset = pSelf->Server()->Tick() + Duration * pSelf->Server()->TickSpeed();
		}
	}
}

void CGameContext::ConShowOthers(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_ClientID < 0 || pSelf->m_ClientID >= MAX_CLIENTS) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pSelf->m_ClientID];
	if(!pPlayer)
		return;
	if(g_Config.m_SvShowOthers)
	{
		if(pPlayer->m_IsUsingDDRaceClient)
		{
			if(pResult->NumArguments())
				pPlayer->m_ShowOthers = pResult->GetInteger(0);
			else
				pPlayer->m_ShowOthers = !pPlayer->m_ShowOthers;
		}
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Showing players from other teams is only available with DDRace Client, http://DDRace.info");
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Showing players from other teams is disabled by the server admin");
}

void CGameContext::Mute(IConsole::IResult *pResult, NETADDR *Addr, int Secs, const char *pDisplayName)
{
	char aBuf[128];
	int Found = 0;
	// find a matching mute for this ip, update expiration time if found
	for(int i = 0; i < m_NumMutes; i++)
	{
		if(net_addr_comp(&m_aMutes[i].m_Addr, Addr) == 0)
		{
			m_aMutes[i].m_Expire = Server()->Tick() + Secs * Server()->TickSpeed();
			Found = 1;
		}
	}

	if(!Found) // nothing found so far, find a free slot..
	{
		if(m_NumMutes < MAX_MUTES)
		{
			m_aMutes[m_NumMutes].m_Addr = *Addr;
			m_aMutes[m_NumMutes].m_Expire = Server()->Tick() + Secs * Server()->TickSpeed();
			m_NumMutes++;
			Found = 1;
		}
	}
	if(Found)
	{
		str_format(aBuf, sizeof aBuf, "'%s' has been muted for %d seconds.", pDisplayName, Secs);
		SendChat(-1, CHAT_ALL, aBuf);
	}
	else if(pResult)// no free slot found
		// pResult->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "mute array is full");
		// Todo(Shereef Marzouk: Fix this
		dbg_msg("mutes", "mute array is full");
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "Use either 'muteid <client_id> <seconds>' or 'muteip <ip> <seconds>'");
}

// mute through client id
void CGameContext::ConMuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetInteger(0);

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(0), 1, 86400), pSelf->Server()->ClientName(Victim));
}

// mute through ip, arguments reversed to workaround parsing
void CGameContext::ConMuteIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "Invalid network address to mute");
	}
	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(1), 1, 86400), pResult->GetString(0));
}

// unmute by mute list index
void CGameContext::ConUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aIpBuf[64];
	char aBuf[64];
	int Victim = pResult->GetInteger(0);

	if(Victim < 0 || Victim >= pSelf->m_NumMutes)
		return;
	
	pSelf->m_NumMutes--;
	pSelf->m_aMutes[Victim] = pSelf->m_aMutes[pSelf->m_NumMutes];

	net_addr_str(&pSelf->m_aMutes[Victim].m_Addr, aIpBuf, sizeof(aIpBuf));
	str_format(aBuf, sizeof(aBuf), "Unmuted %s" , aIpBuf);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
}

// list mutes
void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aIpBuf[64];
	char aBuf[128];
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "Active mutes:");
	for(int i = 0; i < pSelf->m_NumMutes; i++)
	{
		net_addr_str(&pSelf->m_aMutes[i].m_Addr, aIpBuf, sizeof(aIpBuf));
		str_format(aBuf, sizeof aBuf, "%d: \"%s\", %d seconds left", i, aIpBuf, (pSelf->m_aMutes[i].m_Expire - pSelf->Server()->Tick()) / pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
	}
}
