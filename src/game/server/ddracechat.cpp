/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/server/server.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/generated/nethash.cpp>
#if defined(CONF_SQL)
#include <game/server/score/sql_score.h>
#endif

bool CheckClientID(int ClientID);

void CGameContext::ConCredits(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credit",
		"DDRaceNetwork is maintained by deen.");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credit",
		"Many ideas from the great community,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credit",
		"64 player support from eeeee's ddrace64.");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credit",
		"Lots of awesome help and code by HMH and CookiMichal.");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credit",
		"Based on DDRace by the DDRace developers,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credit",
		"which is a mod of Teeworlds by the Teeworlds developers.");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credit",
		"Check the changes on ddnet.tw");
}

void CGameContext::ConInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"DDraceNetwork Mod. Version: " GAME_VERSION);
#if defined( GIT_SHORTREV_HASH )
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"Git revision hash: " GIT_SHORTREV_HASH);
#endif
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"Official site: ddnet.tw");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"For more Info /cmdlist");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"Or visit ddnet.tw");
}

void CGameContext::ConHelp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				"/cmdlist will show a list of all chat commands");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				"/help + any command will show you the help for this command");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				"Example /help settings will display the help about ");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo =
				pSelf->Console()->GetCommandInfo(pArg, CFGFLAG_SERVER, false);
		if (pCmdInfo && pCmdInfo->m_pHelp)
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
					pCmdInfo->m_pHelp);
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"help",
					"Command is either unknown or you have given a blank command without any parameters.");
	}
}

void CGameContext::ConSettings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"setting",
				"to check a server setting say /settings and setting's name, setting names are:");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "setting",
				"teams, cheats, collision, hooking, endlesshooking, me, ");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "setting",
				"hitting, oldlaser, timeout, votes, pause and scores");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		char aBuf[256];
		float ColTemp;
		float HookTemp;
		pSelf->m_Tuning.Get("player_collision", &ColTemp);
		pSelf->m_Tuning.Get("player_hooking", &HookTemp);
		if (str_comp(pArg, "teams") == 0)
		{
			str_format(
					aBuf,
					sizeof(aBuf),
					"%s %s",
					g_Config.m_SvTeam == 1 ?
							"Teams are available on this server" :
							(g_Config.m_SvTeam == 0 || g_Config.m_SvTeam == 3) ?
									"Teams are not available on this server" :
									"You have to be in a team to play on this server", /*g_Config.m_SvTeamStrict ? "and if you die in a team all of you die" : */
									"and if you die in a team only you die");
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
					aBuf);
		}
		else if (str_comp(pArg, "collision") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					ColTemp ?
							"Players can collide on this server" :
							"Players Can't collide on this server");
		}
		else if (str_comp(pArg, "hooking") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					HookTemp ?
							"Players can hook each other on this server" :
							"Players Can't hook each other on this server");
		}
		else if (str_comp(pArg, "endlesshooking") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvEndlessDrag ?
							"Players can hook time is unlimited" :
							"Players can hook time is limited");
		}
		else if (str_comp(pArg, "hitting") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvHit ?
							"Player's weapons affect each other" :
							"Player's weapons has no affect on each other");
		}
		else if (str_comp(pArg, "oldlaser") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvOldLaser ?
							"Lasers can hit you if you shot them and that they pull you towards the bounce origin (Like DDRace Beta)" :
							"Lasers can't hit you if you shot them, and they pull others towards the shooter");
		}
		else if (str_comp(pArg, "me") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvSlashMe ?
							"Players can use /me commands the famous IRC Command" :
							"Players Can't use the /me command");
		}
		else if (str_comp(pArg, "timeout") == 0)
		{
			str_format(aBuf, sizeof(aBuf),
					"The Server Timeout is currently set to %d seconds",
					g_Config.m_ConnTimeout);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
					aBuf);
		}
		else if (str_comp(pArg, "votes") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvVoteKick ?
							"Players can use Callvote menu tab to kick offenders" :
							"Players Can't use the Callvote menu tab to kick offenders");
			if (g_Config.m_SvVoteKick)
				str_format(
						aBuf,
						sizeof(aBuf),
						"Players are banned for %d second(s) if they get voted off",
						g_Config.m_SvVoteKickBantime);
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvVoteKickBantime ?
							aBuf :
							"Players are just kicked and not banned if they get voted off");
		}
		else if (str_comp(pArg, "pause") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvPauseable ?
							g_Config.m_SvPauseTime ?
									"/pause is available on this server and it pauses your time too" :
									"/pause is available on this server but it doesn't pause your time"
									:"/pause is NOT available on this server");
		}
		else if (str_comp(pArg, "scores") == 0)
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"settings",
					g_Config.m_SvHideScore ?
							"Scores are private on this server" :
							"Scores are public on this server");
		}
	}
}

void CGameContext::ConRules(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	bool Printed = false;
	if (g_Config.m_SvDDRaceRules)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				"Be nice.");
		Printed = true;
	}
	if (g_Config.m_SvRulesLine1[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine1);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine2[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine2);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine3[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine3);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine4[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine4);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine5[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine5);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine6[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine6);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine7[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine7);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine8[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine8);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine9[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine9);
		Printed = true;
	}
	if (g_Config.m_SvRulesLine10[0])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				g_Config.m_SvRulesLine10);
		Printed = true;
	}
	if (!Printed)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
				"No Rules Defined, Kill em all!!");
}

void CGameContext::ConTogglePause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	char aBuf[128];

	if(!g_Config.m_SvPauseable)
	{
		ConToggleSpec(pResult, pUserData);
		return;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pPlayer->GetCharacter() == 0)
	{
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pause",
	"You can't pause while you are dead/a spectator.");
	return;
	}
	if (pPlayer->m_Paused == CPlayer::PAUSED_SPEC && g_Config.m_SvPauseable)
	{
		ConToggleSpec(pResult, pUserData);
		return;
	}

	if (pPlayer->m_Paused == CPlayer::PAUSED_FORCE)
	{
		str_format(aBuf, sizeof(aBuf), "You are force-paused. %ds left.", pPlayer->m_ForcePauseTime/pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pause", aBuf);
		return;
	}

	pPlayer->m_Paused = (pPlayer->m_Paused == CPlayer::PAUSED_PAUSED) ? CPlayer::PAUSED_NONE : CPlayer::PAUSED_PAUSED;
}

void CGameContext::ConToggleSpec(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	char aBuf[128];

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if (pPlayer->GetCharacter() == 0)
	{
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "spec",
	"You can't spec while you are dead/a spectator.");
	return;
	}

	if(pPlayer->m_Paused == CPlayer::PAUSED_FORCE)
	{
		str_format(aBuf, sizeof(aBuf), "You are force-paused. %ds left.", pPlayer->m_ForcePauseTime/pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "spec", aBuf);
		return;
	}

	pPlayer->m_Paused = (pPlayer->m_Paused == CPlayer::PAUSED_SPEC) ? CPlayer::PAUSED_NONE : CPlayer::PAUSED_SPEC;
}

void CGameContext::ConTeamTop5(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "teamtop5",
				"Showing the team top 5 is not allowed on this server.");
		return;
	}

	if (pResult->NumArguments() > 0 && pResult->GetInteger(0) >= 0)
		pSelf->Score()->ShowTeamTop5(pResult, pResult->m_ClientID, pUserData,
				pResult->GetInteger(0));
	else
		pSelf->Score()->ShowTeamTop5(pResult, pResult->m_ClientID, pUserData);

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConTop5(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "top5",
				"Showing the top 5 is not allowed on this server.");
		return;
	}

	if (pResult->NumArguments() > 0 && pResult->GetInteger(0) >= 0)
		pSelf->Score()->ShowTop5(pResult, pResult->m_ClientID, pUserData,
				pResult->GetInteger(0));
	else
		pSelf->Score()->ShowTop5(pResult, pResult->m_ClientID, pUserData);

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

#if defined(CONF_SQL)
void CGameContext::ConTimes(IConsole::IResult *pResult, void *pUserData)
{
	if(!CheckClientID(pResult->m_ClientID)) return;
	CGameContext *pSelf = (CGameContext *)pUserData;

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if(g_Config.m_SvUseSQL)
	{
		CSqlScore *pScore = (CSqlScore *)pSelf->Score();
		CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
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
					pScore->ShowTimes(pPlayer->GetCID(), (str_comp(pResult->GetString(0), "me") == 0) ? pSelf->Server()->ClientName(pResult->m_ClientID) : pResult->GetString(0),1);
				return;
			}
			else if (pResult->GetInteger(1) != 0)
			{
				pScore->ShowTimes(pPlayer->GetCID(), (str_comp(pResult->GetString(0), "me") == 0) ? pSelf->Server()->ClientName(pResult->m_ClientID) : pResult->GetString(0),pResult->GetInteger(1));
				return;
			}
		}

		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "times", "/times needs 0, 1 or 2 parameter. 1. = name, 2. = start number");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "times", "Example: /times, /times me, /times Hans, /times \"Papa Smurf\" 5");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "times", "Bad: /times Papa Smurf 5 # Good: /times \"Papa Smurf\" 5 ");

#if defined(CONF_SQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
			pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
	}
}
#endif

void CGameContext::ConDND(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(pPlayer->m_DND)
	{
		pPlayer->m_DND = false;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnd", "You will receive global chat and server messages");
	}
	else
	{
		pPlayer->m_DND = true;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnd", "You will not receive any further global chat and server messages");
	}
}

void CGameContext::ConMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	if (g_Config.m_SvMapVote == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "map",
				"Admin has disabled /map");
		return;
	}

	if (pResult->NumArguments() <= 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "map", "Example: /map adr3 to call vote for Adrenaline 3. This means that the map name must start with 'a' and contain the characters 'd', 'r' and '3' in that order");
		return;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	pSelf->Score()->MapVote(pResult->m_ClientID, pResult->GetString(0));

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConMapPoints(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (pResult->NumArguments() > 0)
		pSelf->Score()->MapPoints(pResult->m_ClientID, pResult->GetString(0));
	else
		pSelf->Score()->MapPoints(pResult->m_ClientID, g_Config.m_SvMap);

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConTimeout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	for(int i = 0; i < pSelf->Server()->MaxClients(); i++)
	{
		if (i == pResult->m_ClientID) continue;
		if (!pSelf->m_apPlayers[i]) continue;
		if (str_comp(pSelf->m_apPlayers[i]->m_TimeoutCode, pResult->GetString(0))) continue;
		if (((CServer *)pSelf->Server())->m_NetServer.SetTimedOut(i, pResult->m_ClientID))
		{
			((CServer *)pSelf->Server())->DelClientCallback(pResult->m_ClientID, "Timeout Protection used", ((CServer *)pSelf->Server()));
			if (pSelf->m_apPlayers[i]->GetCharacter())
				((CGameContext *)(((CServer *)pSelf->Server())->GameServer()))->SendTuningParams(i, pSelf->m_apPlayers[i]->GetCharacter()->m_TuneZone);
			return;
		}
	}

	((CServer *)pSelf->Server())->m_NetServer.SetTimeoutProtected(pResult->m_ClientID);
	str_copy(pPlayer->m_TimeoutCode, pResult->GetString(0), sizeof(pPlayer->m_TimeoutCode));
}

void CGameContext::ConSave(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(!g_Config.m_SvSaveGames)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Save-function is disabled on this server");
		return;
	}

	if(g_Config.m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	int Team = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(pResult->m_ClientID);
	pSelf->Score()->SaveTeam(Team, pResult->GetString(0), pResult->m_ClientID);

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConLoad(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(!g_Config.m_SvSaveGames)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Save-function is disabled on this server");
		return;
	}

	if(g_Config.m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (pResult->NumArguments() > 0)
		pSelf->Score()->LoadTeam(pResult->GetString(0), pResult->m_ClientID);
	else
		return;

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConTeamRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (pResult->NumArguments() > 0)
		if (!g_Config.m_SvHideScore)
			pSelf->Score()->ShowTeamRank(pResult->m_ClientID, pResult->GetString(0),
					true);
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"teamrank",
					"Showing the team rank of other players is not allowed on this server.");
	else
		pSelf->Score()->ShowTeamRank(pResult->m_ClientID,
				pSelf->Server()->ClientName(pResult->m_ClientID));

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (pResult->NumArguments() > 0)
		if (!g_Config.m_SvHideScore)
			pSelf->Score()->ShowRank(pResult->m_ClientID, pResult->GetString(0),
					true);
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"rank",
					"Showing the rank of other players is not allowed on this server.");
	else
		pSelf->Score()->ShowRank(pResult->m_ClientID,
				pSelf->Server()->ClientName(pResult->m_ClientID));

#if defined(CONF_SQL)
	if(g_Config.m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConLockTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	int Team = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(pResult->m_ClientID);

	bool Lock = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.TeamLocked(Team);

	if (pResult->NumArguments() > 0)
		Lock = !pResult->GetInteger(0);

	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		char aBuf[512];
		if(Lock)
		{
			((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetTeamLock(Team, false);

			str_format(aBuf, sizeof(aBuf), "'%s' unlocked your team.", pSelf->Server()->ClientName(pResult->m_ClientID));

			for (int i = 0; i < MAX_CLIENTS; i++)
				if (((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(i) == Team)
					pSelf->SendChatTarget(i, aBuf);
		}
		else
		{
			((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetTeamLock(Team, true);

			str_format(aBuf, sizeof(aBuf), "'%s' locked your team. After the race started killing will kill everyone in your team.", pSelf->Server()->ClientName(pResult->m_ClientID));

			for (int i = 0; i < MAX_CLIENTS; i++)
				if (((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(i) == Team)
					pSelf->SendChatTarget(i, aBuf);
		}
	}
	else
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				"This team can't be locked");
}

void CGameContext::ConJoinTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pSelf->m_VoteCloseTime && pSelf->m_VoteCreator == pResult->m_ClientID)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"join",
				"You are running a vote please try again after the vote is done!");
		return;
	}
	else if (g_Config.m_SvTeam == 0 || g_Config.m_SvTeam == 3)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
				"Admin has disabled teams");
		return;
	}
	else if (g_Config.m_SvTeam == 2 && pResult->GetInteger(0) == 0 && pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_LastStartWarning < pSelf->Server()->Tick() - 3 * pSelf->Server()->TickSpeed())
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"join",
				"You must join a team and play with somebody or else you can\'t play");
		pPlayer->GetCharacter()->m_LastStartWarning = pSelf->Server()->Tick();
	}

	if (pResult->NumArguments() > 0)
	{
		if (pPlayer->GetCharacter() == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					"You can't change teams while you are dead/a spectator.");
		}
		else
		{
			if (pPlayer->m_Last_Team
					+ pSelf->Server()->TickSpeed()
					* g_Config.m_SvTeamChangeDelay
					> pSelf->Server()->Tick())
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
						"You can\'t change teams that fast!");
			}
			else if(pResult->GetInteger(0) > 0 && pResult->GetInteger(0) < MAX_CLIENTS && ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.TeamLocked(pResult->GetInteger(0)))
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
						"This team is locked using /lock. Only members of the team can unlock it using /lock.");
			}
			else if(pResult->GetInteger(0) > 0 && pResult->GetInteger(0) < MAX_CLIENTS && ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.Count(pResult->GetInteger(0)) >= g_Config.m_SvTeamMaxSize)
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "This team already has the maximum allowed size of %d players", g_Config.m_SvTeamMaxSize);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join", aBuf);
			}
			else if (((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetCharacterTeam(
					pPlayer->GetCID(), pResult->GetInteger(0)))
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s joined team %d",
						pSelf->Server()->ClientName(pPlayer->GetCID()),
						pResult->GetInteger(0));
				pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
				pPlayer->m_Last_Team = pSelf->Server()->Tick();
			}
			else
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
						"You cannot join this team at this time");
			}
		}
	}
	else
	{
		char aBuf[512];
		if (!pPlayer->IsPlaying())
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"join",
					"You can't check your team while you are dead/a spectator.");
		}
		else
		{
			str_format(
					aBuf,
					sizeof(aBuf),
					"You are in team %d",
					((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(
							pResult->m_ClientID));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					aBuf);
		}
	}
}

void CGameContext::ConMe(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	char aBuf[256 + 24];

	str_format(aBuf, 256 + 24, "'%s' %s",
			pSelf->Server()->ClientName(pResult->m_ClientID),
			pResult->GetString(0));
	if (g_Config.m_SvSlashMe)
		pSelf->SendChat(-2, CGameContext::CHAT_ALL, aBuf, pResult->m_ClientID);
	else
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"me",
				"/me is disabled on this server, admin can enable it by using sv_slash_me");
}

void CGameContext::ConConverse(IConsole::IResult *pResult, void *pUserData)
{
	// This will never be called
}

void CGameContext::ConWhisper(IConsole::IResult *pResult, void *pUserData)
{
	// This will never be called
}

void CGameContext::ConSetEyeEmote(IConsole::IResult *pResult,
		void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	if(pResult->NumArguments() == 0) {
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"emote",
				(pPlayer->m_EyeEmote) ?
						"You can now use the preset eye emotes." :
						"You don't have any eye emotes, remember to bind some. (until you die)");
		return;
	}
	else if(str_comp_nocase(pResult->GetString(0), "on") == 0)
		pPlayer->m_EyeEmote = true;
	else if(str_comp_nocase(pResult->GetString(0), "off") == 0)
		pPlayer->m_EyeEmote = false;
	else if(str_comp_nocase(pResult->GetString(0), "toggle") == 0)
		pPlayer->m_EyeEmote = !pPlayer->m_EyeEmote;
	pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"emote",
			(pPlayer->m_EyeEmote) ?
					"You can now use the preset eye emotes." :
					"You don't have any eye emotes, remember to bind some. (until you die)");
}

void CGameContext::ConEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (g_Config.m_SvEmotionalTees == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "emote",
				"Server admin disabled emotes.");
		return;
	}

	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"emote",
				"Emote commands are: /emote surprise /emote blink /emote close /emote angry /emote happy /emote pain");
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"emote",
				"Example: /emote surprise 10 for 10 seconds or /emote surprise (default 1 second)");
	}
	else
	{
			if(pPlayer->m_LastEyeEmote + g_Config.m_SvEyeEmoteChangeDelay * pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
				return;

			if (!str_comp(pResult->GetString(0), "angry"))
				pPlayer->m_DefEmote = EMOTE_ANGRY;
			else if (!str_comp(pResult->GetString(0), "blink"))
				pPlayer->m_DefEmote = EMOTE_BLINK;
			else if (!str_comp(pResult->GetString(0), "close"))
				pPlayer->m_DefEmote = EMOTE_BLINK;
			else if (!str_comp(pResult->GetString(0), "happy"))
				pPlayer->m_DefEmote = EMOTE_HAPPY;
			else if (!str_comp(pResult->GetString(0), "pain"))
				pPlayer->m_DefEmote = EMOTE_PAIN;
			else if (!str_comp(pResult->GetString(0), "surprise"))
				pPlayer->m_DefEmote = EMOTE_SURPRISE;
			else if (!str_comp(pResult->GetString(0), "normal"))
				pPlayer->m_DefEmote = EMOTE_NORMAL;
			else
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
						"emote", "Unknown emote... Say /emote");

			int Duration = 1;
			if (pResult->NumArguments() > 1)
				Duration = pResult->GetInteger(1);

			pPlayer->m_DefEmoteReset = pSelf->Server()->Tick()
							+ Duration * pSelf->Server()->TickSpeed();
			pPlayer->m_LastEyeEmote = pSelf->Server()->Tick();
	}
}

void CGameContext::ConNinjaJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	if (pResult->NumArguments())
		pPlayer->m_NinjaJetpack = pResult->GetInteger(0);
	else
		pPlayer->m_NinjaJetpack = !pPlayer->m_NinjaJetpack;
}

void CGameContext::ConShowOthers(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	if (g_Config.m_SvShowOthers)
	{
		if (pResult->NumArguments())
			pPlayer->m_ShowOthers = pResult->GetInteger(0);
		else
			pPlayer->m_ShowOthers = !pPlayer->m_ShowOthers;
	}
	else
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"showotherschat",
				"Showing players from other teams is disabled by the server admin");
}

void CGameContext::ConShowAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
		pPlayer->m_ShowAll = pResult->GetInteger(0);
	else
		pPlayer->m_ShowAll = !pPlayer->m_ShowAll;
}

void CGameContext::ConSpecTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
		pPlayer->m_SpecTeam = pResult->GetInteger(0);
	else
		pPlayer->m_SpecTeam = !pPlayer->m_SpecTeam;
}

bool CheckClientID(int ClientID)
{
	dbg_assert(ClientID >= 0 || ClientID < MAX_CLIENTS,
			"The Client ID is wrong");
	if (ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	return true;
}

void CGameContext::ConSayTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	int ClientID;
	char aBufname[MAX_NAME_LENGTH];

	if (pResult->NumArguments() > 0)
	{
		for(ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if (str_comp(pResult->GetString(0), pSelf->Server()->ClientName(ClientID)) == 0)
				break;

		if(ClientID == MAX_CLIENTS)
			return;

		str_format(aBufname, sizeof(aBufname), "%s's", pSelf->Server()->ClientName(ClientID));
	}
	else
	{
		str_copy(aBufname, "Your", sizeof(aBufname));
		ClientID = pResult->m_ClientID;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;
	if(pChr->m_DDRaceState != DDRACE_STARTED)
		return;

	char aBuftime[64];
	int IntTime = (int) ((float) (pSelf->Server()->Tick() - pChr->m_StartTime)
			/ ((float) pSelf->Server()->TickSpeed()));
	str_format(aBuftime, sizeof(aBuftime), "%s time is %s%d:%s%d",
			aBufname,
			((IntTime / 60) > 9) ? "" : "0", IntTime / 60,
			((IntTime % 60) > 9) ? "" : "0", IntTime % 60);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "time", aBuftime);
}

void CGameContext::ConSayTimeAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;
	if(pChr->m_DDRaceState != DDRACE_STARTED)
		return;

	char aBuftime[64];
	int IntTime = (int) ((float) (pSelf->Server()->Tick() - pChr->m_StartTime)
			/ ((float) pSelf->Server()->TickSpeed()));
	str_format(aBuftime, sizeof(aBuftime),
			"%s\'s current race time is %s%d:%s%d",
			pSelf->Server()->ClientName(pResult->m_ClientID),
			((IntTime / 60) > 9) ? "" : "0", IntTime / 60,
			((IntTime % 60) > 9) ? "" : "0", IntTime % 60);
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuftime, pResult->m_ClientID);
}

void CGameContext::ConTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;

	char aBuftime[64];
	int IntTime = (int) ((float) (pSelf->Server()->Tick() - pChr->m_StartTime)
			/ ((float) pSelf->Server()->TickSpeed()));
	str_format(aBuftime, sizeof(aBuftime), "Your time is %s%d:%s%d",
				((IntTime / 60) > 9) ? "" : "0", IntTime / 60,
				((IntTime % 60) > 9) ? "" : "0", IntTime % 60);
	pSelf->SendBroadcast(aBuftime, pResult->m_ClientID);
}

void CGameContext::ConSetTimerType(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	const char msg[3][128] = {"game/round timer.", "broadcast.", "both game/round timer and broadcast."};
	char aBuf[128];
	if(pPlayer->m_TimerType <= 2 && pPlayer->m_TimerType >= 0)
		str_format(aBuf, sizeof(aBuf), "Timer is displayed in", msg[pPlayer->m_TimerType]);
	else if(pPlayer->m_TimerType == 3)
		str_format(aBuf, sizeof(aBuf), "Timer isn't displayed.");

	if(pResult->NumArguments() == 0) {
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,"timer",aBuf);
		return;
	}
	else if(str_comp_nocase(pResult->GetString(0), "gametimer") == 0) {
		pSelf->SendBroadcast("", pResult->m_ClientID);
		pPlayer->m_TimerType = 0;
	}
	else if(str_comp_nocase(pResult->GetString(0), "broadcast") == 0)
			pPlayer->m_TimerType = 1;
	else if(str_comp_nocase(pResult->GetString(0), "both") == 0)
			pPlayer->m_TimerType = 2;
	else if(str_comp_nocase(pResult->GetString(0), "none") == 0)
			pPlayer->m_TimerType = 3;
	else if(str_comp_nocase(pResult->GetString(0), "cycle") == 0) {
		if(pPlayer->m_TimerType < 3)
			pPlayer->m_TimerType++;
		else if(pPlayer->m_TimerType == 3)
			pPlayer->m_TimerType = 0;
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,"timer",aBuf);
}

#if defined(CONF_SQL)
void CGameContext::ConPoints(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() > 0)
		if (!g_Config.m_SvHideScore)
			pSelf->Score()->ShowPoints(pResult->m_ClientID, pResult->GetString(0),
					true);
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"points",
					"Showing the global points of other players is not allowed on this server.");
	else
		pSelf->Score()->ShowPoints(pResult->m_ClientID,
				pSelf->Server()->ClientName(pResult->m_ClientID));

	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery = pSelf->Server()->Tick();
}
#endif

#if defined(CONF_SQL)
void CGameContext::ConTopPoints(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;

	if (g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "toppoints",
				"Showing the global top points is not allowed on this server.");
		return;
	}

	if (pResult->NumArguments() > 0 && pResult->GetInteger(0) >= 0)
		pSelf->Score()->ShowTopPoints(pResult, pResult->m_ClientID, pUserData,
				pResult->GetInteger(0));
	else
		pSelf->Score()->ShowTopPoints(pResult, pResult->m_ClientID, pUserData);

	if(pSelf->m_apPlayers[pResult->m_ClientID] && g_Config.m_SvUseSQL)
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery = pSelf->Server()->Tick();
}
#endif
