/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <game/mapitems.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/teams.h>
#include <game/version.h>

#include "entities/character.h"
#include "player.h"
#include "score.h"

#include <optional>

bool CheckClientId(int ClientId);

void CGameContext::ConCredits(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"DDNet is run by the DDNet staff (DDNet.org/staff)");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Great maps and many ideas from the great community");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Help and code by eeeee, HMH, east, CookieMichal, Learath2,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Savander, laxa, Tobii, BeaR, Wohoo, nuborn, timakro, Shiki,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"trml, Soreu, hi_leute_gll, Lady Saavik, Chairn, heinrich5991,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"swick, oy, necropotame, Ryozuki, Redix, d3fault, marcelherd,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"BannZay, ACTom, SiuFuWong, PathosEthosLogos, TsFreddie,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Jupeyy, noby, ChillerDragon, ZombieToad, weez15, z6zzz,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Piepow, QingGo, RafaelFF, sctt, jao, daverck, fokkonaut,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Bojidar, FallenKN, ardadem, archimede67, sirius1242, Aerll,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"trafilaw, Zwelf, Patiga, Konsti, ElXreno, MikiGamer,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Fireball, Banana090, axblk, yangfl, Kaffeine, Zodiac,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"c0d3d3v, GiuCcc, Ravie, Robyt3, simpygirl, sjrc6, Cellegen,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"srdante, Nouaa, Voxel, luk51, Vy0x2, Avolicious, louis,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Marmare314, hus3h, ArijanJ, tarunsamanta2k20, Possseidon,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"M0REKZ, Teero, furo, dobrykafe, Moiman, JSaurusRex,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Steinchen, ewancg, gerdoe-jr, BlaiZephyr, KebsCS, bencie,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"DynamoFox, MilkeeyCat, iMilchshake, SchrodingerZhu,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"catseyenebulous, Rei-Tw, Matodor, Emilcha, art0007i, SollyBunny,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"0xfaulty & others");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Based on DDRace by the DDRace developers,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"which is a mod of Teeworlds by the Teeworlds developers.");
}

void CGameContext::ConInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"DDraceNetwork Mod. Version: " GAME_VERSION);
	if(GIT_SHORTREV_HASH)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Git revision hash: %s", GIT_SHORTREV_HASH);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Official site: DDNet.org");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"For more info: /cmdlist");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"Or visit DDNet.org");
}

void CGameContext::ConList(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientId = pResult->m_ClientId;
	if(!CheckClientId(ClientId))
		return;

	char zerochar = 0;
	if(pResult->NumArguments() > 0)
		pSelf->List(ClientId, pResult->GetString(0));
	else
		pSelf->List(ClientId, &zerochar);
}

void CGameContext::ConHelp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"/cmdlist will show a list of all chat commands");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"/help + any command will show you the help for this command");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Example /help settings will display the help about /settings");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo =
			pSelf->Console()->GetCommandInfo(pArg, CFGFLAG_SERVER | CFGFLAG_CHAT, false);
		if(pCmdInfo)
		{
			if(pCmdInfo->m_pParams)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Usage: %s %s", pCmdInfo->m_pName, pCmdInfo->m_pParams);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
			}

			if(pCmdInfo->m_pHelp)
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", pCmdInfo->m_pHelp);
		}
		else
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Unknown command %s", pArg);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
		}
	}
}

void CGameContext::ConSettings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"to check a server setting say /settings and setting's name, setting names are:");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"teams, cheats, collision, hooking, endlesshooking, me, ");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
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
		if(str_comp_nocase(pArg, "teams") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "%s %s",
				g_Config.m_SvTeam == SV_TEAM_ALLOWED ?
					"Teams are available on this server" :
					(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO) ?
					"Teams are not available on this server" :
					"You have to be in a team to play on this server", /*g_Config.m_SvTeamStrict ? "and if you die in a team all of you die" : */
				"and all of your team will die if the team is locked");
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
		}
		else if(str_comp_nocase(pArg, "cheats") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvTestingCommands ?
					"Cheats are enabled on this server" :
					"Cheats are disabled on this server");
		}
		else if(str_comp_nocase(pArg, "collision") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				ColTemp ?
					"Players can collide on this server" :
					"Players can't collide on this server");
		}
		else if(str_comp_nocase(pArg, "hooking") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				HookTemp ?
					"Players can hook each other on this server" :
					"Players can't hook each other on this server");
		}
		else if(str_comp_nocase(pArg, "endlesshooking") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvEndlessDrag ?
					"Players hook time is unlimited" :
					"Players hook time is limited");
		}
		else if(str_comp_nocase(pArg, "hitting") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvHit ?
					"Players weapons affect others" :
					"Players weapons has no affect on others");
		}
		else if(str_comp_nocase(pArg, "oldlaser") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvOldLaser ?
					"Lasers can hit you if you shot them and they pull you towards the bounce origin (Like DDRace Beta)" :
					"Lasers can't hit you if you shot them, and they pull others towards the shooter");
		}
		else if(str_comp_nocase(pArg, "me") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvSlashMe ?
					"Players can use /me commands the famous IRC Command" :
					"Players can't use the /me command");
		}
		else if(str_comp_nocase(pArg, "timeout") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "The Server Timeout is currently set to %d seconds", g_Config.m_ConnTimeout);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
		}
		else if(str_comp_nocase(pArg, "votes") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvVoteKick ?
					"Players can use Callvote menu tab to kick offenders" :
					"Players can't use the Callvote menu tab to kick offenders");
			if(g_Config.m_SvVoteKick)
			{
				str_format(aBuf, sizeof(aBuf),
					"Players are banned for %d minute(s) if they get voted off", g_Config.m_SvVoteKickBantime);

				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
					g_Config.m_SvVoteKickBantime ?
						aBuf :
						"Players are just kicked and not banned if they get voted off");
			}
		}
		else if(str_comp_nocase(pArg, "pause") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvPauseable ?
					"/spec will pause you and your tee will vanish" :
					"/spec will pause you but your tee will not vanish");
		}
		else if(str_comp_nocase(pArg, "scores") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvHideScore ?
					"Scores are private on this server" :
					"Scores are public on this server");
		}
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				"no matching settings found, type /settings to view them");
		}
	}
}

void CGameContext::ConRules(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	bool Printed = false;
	if(g_Config.m_SvDDRaceRules)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Be nice.");
		Printed = true;
	}
	char *apRuleLines[] = {
		g_Config.m_SvRulesLine1,
		g_Config.m_SvRulesLine2,
		g_Config.m_SvRulesLine3,
		g_Config.m_SvRulesLine4,
		g_Config.m_SvRulesLine5,
		g_Config.m_SvRulesLine6,
		g_Config.m_SvRulesLine7,
		g_Config.m_SvRulesLine8,
		g_Config.m_SvRulesLine9,
		g_Config.m_SvRulesLine10,
	};
	for(auto &pRuleLine : apRuleLines)
	{
		if(pRuleLine[0])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
				"chatresp", pRuleLine);
			Printed = true;
		}
	}
	if(!Printed)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"No Rules Defined, Kill em all!!");
	}
}

void ToggleSpecPause(IConsole::IResult *pResult, void *pUserData, int PauseType)
{
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CGameContext *pSelf = (CGameContext *)pUserData;
	IServer *pServ = pSelf->Server();
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	int PauseState = pPlayer->IsPaused();
	if(PauseState > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "You are force-paused for %d seconds.", (PauseState - pServ->Tick()) / pServ->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
	}
	else if(pResult->NumArguments() > 0)
	{
		if(-PauseState == PauseType && pPlayer->m_SpectatorId != pResult->m_ClientId && pServ->ClientIngame(pPlayer->m_SpectatorId) && !str_comp(pServ->ClientName(pPlayer->m_SpectatorId), pResult->GetString(0)))
		{
			pPlayer->Pause(CPlayer::PAUSE_NONE, false);
		}
		else
		{
			pPlayer->Pause(PauseType, false);
			pPlayer->SpectatePlayerName(pResult->GetString(0));
		}
	}
	else if(-PauseState != CPlayer::PAUSE_NONE && PauseType != CPlayer::PAUSE_NONE)
	{
		pPlayer->Pause(CPlayer::PAUSE_NONE, false);
	}
	else if(-PauseState != PauseType)
	{
		pPlayer->Pause(PauseType, false);
	}
}

void ToggleSpecPauseVoted(IConsole::IResult *pResult, void *pUserData, int PauseType)
{
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	int PauseState = pPlayer->IsPaused();
	if(PauseState > 0)
	{
		IServer *pServ = pSelf->Server();
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "You are force-paused for %d seconds.", (PauseState - pServ->Tick()) / pServ->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
		return;
	}

	bool IsPlayerBeingVoted = pSelf->m_VoteCloseTime &&
				  (pSelf->IsKickVote() || pSelf->IsSpecVote()) &&
				  pResult->m_ClientId != pSelf->m_VoteVictim;
	if((!IsPlayerBeingVoted && -PauseState == PauseType) ||
		(IsPlayerBeingVoted && PauseState && pPlayer->m_SpectatorId == pSelf->m_VoteVictim))
	{
		pPlayer->Pause(CPlayer::PAUSE_NONE, false);
	}
	else
	{
		pPlayer->Pause(PauseType, false);
		if(IsPlayerBeingVoted)
			pPlayer->m_SpectatorId = pSelf->m_VoteVictim;
	}
}

void CGameContext::ConToggleSpec(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPause(pResult, pUserData, g_Config.m_SvPauseable ? CPlayer::PAUSE_SPEC : CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConToggleSpecVoted(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPauseVoted(pResult, pUserData, g_Config.m_SvPauseable ? CPlayer::PAUSE_SPEC : CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConTogglePause(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPause(pResult, pUserData, CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConTogglePauseVoted(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPauseVoted(pResult, pUserData, CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConTeamTop5(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Showing the team top 5 is not allowed on this server.");
		return;
	}

	if(pResult->NumArguments() == 0)
	{
		pSelf->Score()->ShowTeamTop5(pResult->m_ClientId, 1);
	}
	else if(pResult->NumArguments() == 1)
	{
		if(pResult->GetInteger(0) != 0)
		{
			pSelf->Score()->ShowTeamTop5(pResult->m_ClientId, pResult->GetInteger(0));
		}
		else
		{
			const char *pRequestedName = (str_comp_nocase(pResult->GetString(0), "me") == 0) ?
							     pSelf->Server()->ClientName(pResult->m_ClientId) :
							     pResult->GetString(0);
			pSelf->Score()->ShowPlayerTeamTop5(pResult->m_ClientId, pRequestedName, 0);
		}
	}
	else if(pResult->NumArguments() == 2 && pResult->GetInteger(1) != 0)
	{
		const char *pRequestedName = (str_comp_nocase(pResult->GetString(0), "me") == 0) ?
						     pSelf->Server()->ClientName(pResult->m_ClientId) :
						     pResult->GetString(0);
		pSelf->Score()->ShowPlayerTeamTop5(pResult->m_ClientId, pRequestedName, pResult->GetInteger(1));
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "/top5team needs 0, 1 or 2 parameter. 1. = name, 2. = start number");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Example: /top5team, /top5team me, /top5team Hans, /top5team \"Papa Smurf\" 5");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Bad: /top5team Papa Smurf 5 # Good: /top5team \"Papa Smurf\" 5 ");
	}
}

void CGameContext::ConTop(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Showing the top is not allowed on this server.");
		return;
	}

	if(pResult->NumArguments() > 0)
		pSelf->Score()->ShowTop(pResult->m_ClientId, pResult->GetInteger(0));
	else
		pSelf->Score()->ShowTop(pResult->m_ClientId);
}

void CGameContext::ConTimes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Score()->ShowTimes(pResult->m_ClientId, 1);
	}
	else if(pResult->NumArguments() == 1)
	{
		if(pResult->GetInteger(0) != 0)
		{
			pSelf->Score()->ShowTimes(pResult->m_ClientId, pResult->GetInteger(0));
		}
		else
		{
			const char *pRequestedName = (str_comp_nocase(pResult->GetString(0), "me") == 0) ?
							     pSelf->Server()->ClientName(pResult->m_ClientId) :
							     pResult->GetString(0);
			pSelf->Score()->ShowTimes(pResult->m_ClientId, pRequestedName, pResult->GetInteger(1));
		}
	}
	else if(pResult->NumArguments() == 2 && pResult->GetInteger(1) != 0)
	{
		const char *pRequestedName = (str_comp_nocase(pResult->GetString(0), "me") == 0) ?
						     pSelf->Server()->ClientName(pResult->m_ClientId) :
						     pResult->GetString(0);
		pSelf->Score()->ShowTimes(pResult->m_ClientId, pRequestedName, pResult->GetInteger(1));
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "/times needs 0, 1 or 2 parameter. 1. = name, 2. = start number");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Example: /times, /times me, /times Hans, /times \"Papa Smurf\" 5");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Bad: /times Papa Smurf 5 # Good: /times \"Papa Smurf\" 5 ");
	}
}

void CGameContext::ConDND(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	pPlayer->m_DND = pResult->NumArguments() == 0 ? !pPlayer->m_DND : pResult->GetInteger(0);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", pPlayer->m_DND ? "You will not receive any further global chat and server messages" : "You will receive global chat and server messages");
}

void CGameContext::ConWhispers(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	pPlayer->m_Whispers = pResult->NumArguments() == 0 ? !pPlayer->m_Whispers : pResult->GetInteger(0);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", pPlayer->m_Whispers ? "You will receive whispers" : "You will not receive any further whispers");
}

void CGameContext::ConMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvMapVote == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"/map is disabled");
		return;
	}

	if(pResult->NumArguments() <= 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Example: /map adr3 to call vote for Adrenaline 3. This means that the map name must start with 'a' and contain the characters 'd', 'r' and '3' in that order");
		return;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(pSelf->RateLimitPlayerVote(pResult->m_ClientId) || pSelf->RateLimitPlayerMapVote(pResult->m_ClientId))
		return;

	pSelf->Score()->MapVote(pResult->m_ClientId, pResult->GetString(0));
}

void CGameContext::ConMapInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(pResult->NumArguments() > 0)
		pSelf->Score()->MapInfo(pResult->m_ClientId, pResult->GetString(0));
	else
		pSelf->Score()->MapInfo(pResult->m_ClientId, pSelf->Server()->GetMapName());
}

void CGameContext::ConTimeout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	const char *pTimeout = pResult->NumArguments() > 0 ? pResult->GetString(0) : pPlayer->m_aTimeoutCode;

	if(!pSelf->Server()->IsSixup(pResult->m_ClientId))
	{
		for(int i = 0; i < pSelf->Server()->MaxClients(); i++)
		{
			if(i == pResult->m_ClientId)
				continue;
			if(!pSelf->m_apPlayers[i])
				continue;
			if(str_comp(pSelf->m_apPlayers[i]->m_aTimeoutCode, pTimeout))
				continue;
			if(pSelf->Server()->SetTimedOut(i, pResult->m_ClientId))
			{
				if(pSelf->m_apPlayers[i]->GetCharacter())
					pSelf->SendTuningParams(i, pSelf->m_apPlayers[i]->GetCharacter()->m_TuneZone);
				return;
			}
		}
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Your timeout code has been set. 0.7 clients can not reclaim their tees on timeout; however, a 0.6 client can claim your tee ");
	}

	pSelf->Server()->SetTimeoutProtected(pResult->m_ClientId);
	str_copy(pPlayer->m_aTimeoutCode, pResult->GetString(0), sizeof(pPlayer->m_aTimeoutCode));
}

void CGameContext::ConPractice(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(pSelf->ProcessSpamProtection(pResult->m_ClientId, false))
		return;

	if(!g_Config.m_SvPractice)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Practice mode is disabled");
		return;
	}

	CGameTeams &Teams = pSelf->m_pController->Teams();

	int Team = Teams.m_Core.Team(pResult->m_ClientId);

	if(Team < TEAM_FLOCK || (Team == TEAM_FLOCK && g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO) || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Join a team to enable practice mode, which means you can use /r, but can't earn a rank.");
		return;
	}

	if(Teams.TeamFlock(Team))
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Practice mode can't be enabled in team 0 mode.");
		return;
	}

	if(Teams.GetSaving(Team))
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Practice mode can't be enabled while team save or load is in progress");
		return;
	}

	if(Teams.IsPractice(Team))
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Team is already in practice mode");
		return;
	}

	bool VotedForPractice = pResult->NumArguments() == 0 || pResult->GetInteger(0);

	if(VotedForPractice == pPlayer->m_VotedForPractice)
		return;

	pPlayer->m_VotedForPractice = VotedForPractice;

	int NumCurrentVotes = 0;
	int TeamSize = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Teams.m_Core.Team(i) == Team)
		{
			CPlayer *pPlayer2 = pSelf->m_apPlayers[i];
			if(pPlayer2 && pPlayer2->m_VotedForPractice)
				NumCurrentVotes++;
			TeamSize++;
		}
	}

	int NumRequiredVotes = TeamSize / 2 + 1;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' voted to %s /practice mode for your team, which means you can use practice commands, but you can't earn a rank. Type /practice to vote (%d/%d required votes)", pSelf->Server()->ClientName(pResult->m_ClientId), VotedForPractice ? "enable" : "disable", NumCurrentVotes, NumRequiredVotes);
	pSelf->SendChatTeam(Team, aBuf);

	if(NumCurrentVotes >= NumRequiredVotes)
	{
		Teams.SetPractice(Team, true);
		pSelf->SendChatTeam(Team, "Practice mode enabled for your team, happy practicing!");
		pSelf->SendChatTeam(Team, "See /practicecmdlist for a list of all avaliable practice commands. Most commonly used ones are /telecursor, /lasttp and /rescue");
	}
}

void CGameContext::ConPracticeCmdList(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	char aPracticeCommands[256];
	mem_zero(aPracticeCommands, sizeof(aPracticeCommands));
	str_append(aPracticeCommands, "Available practice commands: ");
	for(const IConsole::CCommandInfo *pCmd = pSelf->Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_USER, CMDFLAG_PRACTICE);
		pCmd; pCmd = pCmd->NextCommandInfo(IConsole::ACCESS_LEVEL_USER, CMDFLAG_PRACTICE))
	{
		char aCommand[64];

		str_format(aCommand, sizeof(aCommand), "/%s%s", pCmd->m_pName, pCmd->NextCommandInfo(IConsole::ACCESS_LEVEL_USER, CMDFLAG_PRACTICE) ? ", " : "");

		if(str_length(aCommand) + str_length(aPracticeCommands) > 255)
		{
			pSelf->SendChatTarget(pResult->m_ClientId, aPracticeCommands);
			mem_zero(aPracticeCommands, sizeof(aPracticeCommands));
		}
		str_append(aPracticeCommands, aCommand);
	}
	pSelf->SendChatTarget(pResult->m_ClientId, aPracticeCommands);
}

void CGameContext::ConSwap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);

	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(!g_Config.m_SvSwap)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Swap is disabled on this server.");
		return;
	}

	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Swap is not available on forced solo servers.");
		return;
	}

	CGameTeams &Teams = pSelf->m_pController->Teams();

	int Team = Teams.m_Core.Team(pResult->m_ClientId);

	if(Team < TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Join a team to use swap feature, which means you can swap positions with each other.");
		return;
	}

	int TargetClientId = -1;
	if(pResult->NumArguments() == 1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(pSelf->m_apPlayers[i] && !str_comp(pName, pSelf->Server()->ClientName(i)))
			{
				TargetClientId = i;
				break;
			}
		}
	}
	else
	{
		int TeamSize = 1;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(pSelf->m_apPlayers[i] && Teams.m_Core.Team(i) == Team && i != pResult->m_ClientId)
			{
				TargetClientId = i;
				TeamSize++;
			}
		}
		if(TeamSize != 2)
			TargetClientId = -1;
	}

	if(TargetClientId < 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Player not found");
		return;
	}

	if(TargetClientId == pResult->m_ClientId)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Can't swap with yourself");
		return;
	}

	int TargetTeam = Teams.m_Core.Team(TargetClientId);
	if(TargetTeam != Team)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Player is on a different team");
		return;
	}

	CPlayer *pSwapPlayer = pSelf->m_apPlayers[TargetClientId];
	if(Team == TEAM_FLOCK || Teams.TeamFlock(Team))
	{
		CCharacter *pChr = pPlayer->GetCharacter();
		CCharacter *pSwapChr = pSwapPlayer->GetCharacter();
		if(!pChr || !pSwapChr || pChr->m_DDRaceState != DDRACE_STARTED || pSwapChr->m_DDRaceState != DDRACE_STARTED)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "You and other player need to have started the map");
			return;
		}
	}
	else if(!Teams.IsStarted(Team) && !Teams.TeamFlock(Team))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Need to have started the map to swap with a player.");
		return;
	}
	if(pSelf->m_World.m_Core.m_apCharacters[pResult->m_ClientId] == nullptr || pSelf->m_World.m_Core.m_apCharacters[TargetClientId] == nullptr)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "You and the other player must not be paused.");
		return;
	}

	bool SwapPending = pSwapPlayer->m_SwapTargetsClientId != pResult->m_ClientId;
	if(SwapPending)
	{
		if(pSelf->ProcessSpamProtection(pResult->m_ClientId))
			return;

		Teams.RequestTeamSwap(pPlayer, pSwapPlayer, Team);
		return;
	}

	Teams.SwapTeamCharacters(pPlayer, pSwapPlayer, Team);
}

void CGameContext::ConCancelSwap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(!g_Config.m_SvSwap)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Swap is disabled on this server.");
		return;
	}

	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Swap is not available on forced solo servers.");
		return;
	}

	CGameTeams &Teams = pSelf->m_pController->Teams();

	int Team = Teams.m_Core.Team(pResult->m_ClientId);

	if(Team < TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Join a team to use swap feature, which means you can swap positions with each other.");
		return;
	}

	bool SwapPending = pPlayer->m_SwapTargetsClientId != -1 && !pSelf->Server()->ClientSlotEmpty(pPlayer->m_SwapTargetsClientId);

	if(!SwapPending)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"You do not have a pending swap request.");
		return;
	}

	Teams.CancelTeamSwap(pPlayer, Team);
}

void CGameContext::ConSave(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(!g_Config.m_SvSaveGames)
	{
		pSelf->SendChatTarget(pResult->m_ClientId, "Save-function is disabled on this server");
		return;
	}

	const char *pCode = "";
	if(pResult->NumArguments() > 0)
		pCode = pResult->GetString(0);

	pSelf->Score()->SaveTeam(pResult->m_ClientId, pCode, g_Config.m_SvSqlServerName);
}

void CGameContext::ConLoad(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(!g_Config.m_SvSaveGames)
	{
		pSelf->SendChatTarget(pResult->m_ClientId, "Save-function is disabled on this server");
		return;
	}

	if(pResult->NumArguments() > 0)
		pSelf->Score()->LoadTeam(pResult->GetString(0), pResult->m_ClientId);
	else
		pSelf->Score()->GetSaves(pResult->m_ClientId);
}

void CGameContext::ConTeamRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(pResult->NumArguments() > 0)
	{
		if(!g_Config.m_SvHideScore)
			pSelf->Score()->ShowTeamRank(pResult->m_ClientId, pResult->GetString(0));
		else
			pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"chatresp",
				"Showing the team rank of other players is not allowed on this server.");
	}
	else
		pSelf->Score()->ShowTeamRank(pResult->m_ClientId,
			pSelf->Server()->ClientName(pResult->m_ClientId));
}

void CGameContext::ConRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(pResult->NumArguments() > 0)
	{
		if(!g_Config.m_SvHideScore)
			pSelf->Score()->ShowRank(pResult->m_ClientId, pResult->GetString(0));
		else
			pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"chatresp",
				"Showing the rank of other players is not allowed on this server.");
	}
	else
		pSelf->Score()->ShowRank(pResult->m_ClientId,
			pSelf->Server()->ClientName(pResult->m_ClientId));
}

void CGameContext::ConLock(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Teams are disabled");
		return;
	}

	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);

	bool Lock = pSelf->m_pController->Teams().TeamLocked(Team);

	if(pResult->NumArguments() > 0)
		Lock = !pResult->GetInteger(0);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"This team can't be locked");
		return;
	}

	if(pSelf->ProcessSpamProtection(pResult->m_ClientId, false))
		return;

	char aBuf[512];
	if(Lock)
	{
		pSelf->UnlockTeam(pResult->m_ClientId, Team);
	}
	else
	{
		pSelf->m_pController->Teams().SetTeamLock(Team, true);

		if(pSelf->m_pController->Teams().TeamFlock(Team))
			str_format(aBuf, sizeof(aBuf), "'%s' locked your team.", pSelf->Server()->ClientName(pResult->m_ClientId));
		else
			str_format(aBuf, sizeof(aBuf), "'%s' locked your team. After the race starts, killing will kill everyone in your team.", pSelf->Server()->ClientName(pResult->m_ClientId));
		pSelf->SendChatTeam(Team, aBuf);
	}
}

void CGameContext::ConUnlock(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Teams are disabled");
		return;
	}

	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
		return;

	if(pSelf->ProcessSpamProtection(pResult->m_ClientId, false))
		return;

	pSelf->UnlockTeam(pResult->m_ClientId, Team);
}

void CGameContext::UnlockTeam(int ClientId, int Team) const
{
	m_pController->Teams().SetTeamLock(Team, false);

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' unlocked your team.", Server()->ClientName(ClientId));
	SendChatTeam(Team, aBuf);
}

void CGameContext::AttemptJoinTeam(int ClientId, int Team)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	if(m_VoteCloseTime && m_VoteCreator == ClientId && (IsKickVote() || IsSpecVote()))
	{
		Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"You are running a vote please try again after the vote is done!");
		return;
	}
	else if(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Teams are disabled");
		return;
	}
	else if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && Team == 0 && pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_LastStartWarning < Server()->Tick() - 3 * Server()->TickSpeed())
	{
		Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"You must join a team and play with somebody or else you can\'t play");
		pPlayer->GetCharacter()->m_LastStartWarning = Server()->Tick();
	}

	if(pPlayer->GetCharacter() == nullptr)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"You can't change teams while you are dead/a spectator.");
	}
	else
	{
		if(Team < 0 || Team >= MAX_CLIENTS)
			Team = m_pController->Teams().GetFirstEmptyTeam();

		if(pPlayer->m_Last_Team + (int64_t)Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay > Server()->Tick())
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				"You can\'t change teams that fast!");
		}
		else if(Team > 0 && Team < MAX_CLIENTS && m_pController->Teams().TeamLocked(Team) && !m_pController->Teams().IsInvited(Team, ClientId))
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
				g_Config.m_SvInvite ?
					"This team is locked using /lock. Only members of the team can unlock it using /lock." :
					"This team is locked using /lock. Only members of the team can invite you or unlock it using /lock.");
		}
		else if(Team > 0 && Team < MAX_CLIENTS && m_pController->Teams().Count(Team) >= g_Config.m_SvMaxTeamSize && !m_pController->Teams().TeamFlock(Team) && !m_pController->Teams().IsPractice(Team))
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "This team already has the maximum allowed size of %d players", g_Config.m_SvMaxTeamSize);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
		}
		else if(const char *pError = m_pController->Teams().SetCharacterTeam(pPlayer->GetCid(), Team))
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", pError);
		}
		else
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "'%s' joined team %d",
				Server()->ClientName(pPlayer->GetCid()),
				Team);
			SendChat(-1, TEAM_ALL, aBuf);
			pPlayer->m_Last_Team = Server()->Tick();

			if(m_pController->Teams().IsPractice(Team))
				SendChatTarget(pPlayer->GetCid(), "Practice mode enabled for your team, happy practicing!");

			if(m_pController->Teams().TeamFlock(Team))
				SendChatTarget(pPlayer->GetCid(), "Team 0 mode enabled for your team. This will make your team behave like team 0.");
		}
	}
}

void CGameContext::ConInvite(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	auto *pController = pSelf->m_pController;
	const char *pName = pResult->GetString(0);

	if(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Teams are disabled");
		return;
	}

	if(!g_Config.m_SvInvite)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Invites are disabled");
		return;
	}

	int Team = pController->Teams().m_Core.Team(pResult->m_ClientId);
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		int Target = -1;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!str_comp(pName, pSelf->Server()->ClientName(i)))
			{
				Target = i;
				break;
			}
		}

		if(Target < 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Player not found");
			return;
		}

		if(pController->Teams().IsInvited(Team, Target))
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Player already invited");
			return;
		}

		if(pSelf->m_apPlayers[pResult->m_ClientId] && pSelf->m_apPlayers[pResult->m_ClientId]->m_LastInvited + g_Config.m_SvInviteFrequency * pSelf->Server()->TickSpeed() > pSelf->Server()->Tick())
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Can't invite this quickly");
			return;
		}

		pController->Teams().SetClientInvited(Team, Target, true);
		pSelf->m_apPlayers[pResult->m_ClientId]->m_LastInvited = pSelf->Server()->Tick();

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' invited you to team %d. Use /team %d to join.", pSelf->Server()->ClientName(pResult->m_ClientId), Team, Team);
		pSelf->SendChatTarget(Target, aBuf);

		str_format(aBuf, sizeof(aBuf), "'%s' invited '%s' to your team.", pSelf->Server()->ClientName(pResult->m_ClientId), pSelf->Server()->ClientName(Target));
		pSelf->SendChatTeam(Team, aBuf);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Can't invite players to this team");
}

void CGameContext::ConTeam0Mode(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	auto *pController = pSelf->m_pController;

	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO || g_Config.m_SvTeam == SV_TEAM_MANDATORY)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Team mode change disabled");
		return;
	}

	if(!g_Config.m_SvTeam0Mode)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Team mode change is disabled on this server.");
		return;
	}

	int Team = pController->Teams().m_Core.Team(pResult->m_ClientId);
	bool Mode = pController->Teams().TeamFlock(Team);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"This team can't have the mode changed");
		return;
	}

	if(pController->Teams().GetTeamState(Team) != CGameTeams::TEAMSTATE_OPEN)
	{
		pSelf->SendChatTarget(pResult->m_ClientId, "Team mode can't be changed while racing");
		return;
	}

	if(pResult->NumArguments() > 0)
		Mode = !pResult->GetInteger(0);

	if(pSelf->ProcessSpamProtection(pResult->m_ClientId, false))
		return;

	char aBuf[512];
	if(Mode)
	{
		if(pController->Teams().Count(Team) > g_Config.m_SvMaxTeamSize)
		{
			str_format(aBuf, sizeof(aBuf), "Can't disable team 0 mode. This team exceeds the maximum allowed size of %d players for regular team", g_Config.m_SvMaxTeamSize);
			pSelf->SendChatTarget(pResult->m_ClientId, aBuf);
		}
		else
		{
			pController->Teams().SetTeamFlock(Team, false);

			str_format(aBuf, sizeof(aBuf), "'%s' disabled team 0 mode.", pSelf->Server()->ClientName(pResult->m_ClientId));
			pSelf->SendChatTeam(Team, aBuf);
		}
	}
	else
	{
		if(pController->Teams().IsPractice(Team))
		{
			pSelf->SendChatTarget(pResult->m_ClientId, "Can't enable team 0 mode with practice mode on.");
		}
		else
		{
			pController->Teams().SetTeamFlock(Team, true);

			str_format(aBuf, sizeof(aBuf), "'%s' enabled team 0 mode. This will make your team behave like team 0.", pSelf->Server()->ClientName(pResult->m_ClientId));
			pSelf->SendChatTeam(Team, aBuf);
		}
	}
}

void CGameContext::ConTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(pResult->NumArguments() > 0)
	{
		pSelf->AttemptJoinTeam(pResult->m_ClientId, pResult->GetInteger(0));
	}
	else
	{
		char aBuf[512];
		if(!pPlayer->IsPlaying())
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "You can't check your team while you are dead/a spectator.");
		}
		else
		{
			int TeamSize = 0;
			const int PlayerTeam = pSelf->GetDDRaceTeam(pResult->m_ClientId);

			// Count players in team
			for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
			{
				const CPlayer *pOtherPlayer = pSelf->m_apPlayers[ClientId];
				if(!pOtherPlayer || !pOtherPlayer->IsPlaying())
					continue;

				if(pSelf->GetDDRaceTeam(ClientId) == PlayerTeam)
					TeamSize++;
			}

			str_format(aBuf, sizeof(aBuf), "You are in team %d having %d %s", PlayerTeam, TeamSize, TeamSize > 1 ? "players" : "player");
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
		}
	}
}

void CGameContext::ConJoin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	int Target = -1;
	const char *pName = pResult->GetString(0);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!str_comp(pName, pSelf->Server()->ClientName(i)))
		{
			Target = i;
			break;
		}
	}

	if(Target == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Player not found");
		return;
	}

	int Team = pSelf->GetDDRaceTeam(Target);
	if(pSelf->ProcessSpamProtection(pResult->m_ClientId, false))
		return;

	pSelf->AttemptJoinTeam(pResult->m_ClientId, Team);
}

void CGameContext::ConMe(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	char aBuf[256 + 24];

	str_format(aBuf, 256 + 24, "'%s' %s",
		pSelf->Server()->ClientName(pResult->m_ClientId),
		pResult->GetString(0));
	if(g_Config.m_SvSlashMe)
		pSelf->SendChat(-2, TEAM_ALL, aBuf, pResult->m_ClientId);
	else
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"/me is disabled on this server");
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
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			(pPlayer->m_EyeEmoteEnabled) ?
				"You can now use the preset eye emotes." :
				"You don't have any eye emotes, remember to bind some.");
		return;
	}
	else if(str_comp_nocase(pResult->GetString(0), "on") == 0)
		pPlayer->m_EyeEmoteEnabled = true;
	else if(str_comp_nocase(pResult->GetString(0), "off") == 0)
		pPlayer->m_EyeEmoteEnabled = false;
	else if(str_comp_nocase(pResult->GetString(0), "toggle") == 0)
		pPlayer->m_EyeEmoteEnabled = !pPlayer->m_EyeEmoteEnabled;
	pSelf->Console()->Print(
		IConsole::OUTPUT_LEVEL_STANDARD,
		"chatresp",
		(pPlayer->m_EyeEmoteEnabled) ?
			"You can now use the preset eye emotes." :
			"You don't have any eye emotes, remember to bind some.");
}

void CGameContext::ConEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(g_Config.m_SvEmotionalTees == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Emotes are disabled.");
		return;
	}

	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Emote commands are: /emote surprise /emote blink /emote close /emote angry /emote happy /emote pain /emote normal");
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Example: /emote surprise 10 for 10 seconds or /emote surprise (default 1 second)");
	}
	else
	{
		if(!pPlayer->CanOverrideDefaultEmote())
			return;

		int EmoteType = 0;
		if(!str_comp_nocase(pResult->GetString(0), "angry"))
			EmoteType = EMOTE_ANGRY;
		else if(!str_comp_nocase(pResult->GetString(0), "blink"))
			EmoteType = EMOTE_BLINK;
		else if(!str_comp_nocase(pResult->GetString(0), "close"))
			EmoteType = EMOTE_BLINK;
		else if(!str_comp_nocase(pResult->GetString(0), "happy"))
			EmoteType = EMOTE_HAPPY;
		else if(!str_comp_nocase(pResult->GetString(0), "pain"))
			EmoteType = EMOTE_PAIN;
		else if(!str_comp_nocase(pResult->GetString(0), "surprise"))
			EmoteType = EMOTE_SURPRISE;
		else if(!str_comp_nocase(pResult->GetString(0), "normal"))
			EmoteType = EMOTE_NORMAL;
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
				"chatresp", "Unknown emote... Say /emote");
			return;
		}

		int Duration = 1;
		if(pResult->NumArguments() > 1)
			Duration = clamp(pResult->GetInteger(1), 1, 86400);

		pPlayer->OverrideDefaultEmote(EmoteType, pSelf->Server()->Tick() + Duration * pSelf->Server()->TickSpeed());
	}
}

void CGameContext::ConNinjaJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	if(pResult->NumArguments())
		pPlayer->m_NinjaJetpack = pResult->GetInteger(0);
	else
		pPlayer->m_NinjaJetpack = !pPlayer->m_NinjaJetpack;
}

void CGameContext::ConShowOthers(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	if(g_Config.m_SvShowOthers)
	{
		if(pResult->NumArguments())
			pPlayer->m_ShowOthers = pResult->GetInteger(0);
		else
			pPlayer->m_ShowOthers = !pPlayer->m_ShowOthers;
	}
	else
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"chatresp",
			"Showing players from other teams is disabled");
}

void CGameContext::ConShowAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(pResult->NumArguments())
	{
		if(pPlayer->m_ShowAll == (bool)pResult->GetInteger(0))
			return;

		pPlayer->m_ShowAll = pResult->GetInteger(0);
	}
	else
	{
		pPlayer->m_ShowAll = !pPlayer->m_ShowAll;
	}

	if(pPlayer->m_ShowAll)
		pSelf->SendChatTarget(pResult->m_ClientId, "You will now see all tees on this server, no matter the distance");
	else
		pSelf->SendChatTarget(pResult->m_ClientId, "You will no longer see all tees on this server");
}

void CGameContext::ConSpecTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	if(pResult->NumArguments())
		pPlayer->m_SpecTeam = pResult->GetInteger(0);
	else
		pPlayer->m_SpecTeam = !pPlayer->m_SpecTeam;
}

bool CheckClientId(int ClientId)
{
	return ClientId >= 0 && ClientId < MAX_CLIENTS;
}

void CGameContext::ConSayTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	int ClientId;
	char aBufName[MAX_NAME_LENGTH];

	if(pResult->NumArguments() > 0)
	{
		for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
			if(str_comp(pResult->GetString(0), pSelf->Server()->ClientName(ClientId)) == 0)
				break;

		if(ClientId == MAX_CLIENTS)
			return;

		str_format(aBufName, sizeof(aBufName), "%s's", pSelf->Server()->ClientName(ClientId));
	}
	else
	{
		str_copy(aBufName, "Your", sizeof(aBufName));
		ClientId = pResult->m_ClientId;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;
	if(pChr->m_DDRaceState != DDRACE_STARTED)
		return;

	char aBufTime[32];
	char aBuf[64];
	int64_t Time = (int64_t)100 * (float)(pSelf->Server()->Tick() - pChr->m_StartTime) / ((float)pSelf->Server()->TickSpeed());
	str_time(Time, TIME_HOURS, aBufTime, sizeof(aBufTime));
	str_format(aBuf, sizeof(aBuf), "%s current race time is %s", aBufName, aBufTime);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
}

void CGameContext::ConSayTimeAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;
	if(pChr->m_DDRaceState != DDRACE_STARTED)
		return;

	char aBufTime[32];
	char aBuf[64];
	int64_t Time = (int64_t)100 * (float)(pSelf->Server()->Tick() - pChr->m_StartTime) / ((float)pSelf->Server()->TickSpeed());
	const char *pName = pSelf->Server()->ClientName(pResult->m_ClientId);
	str_time(Time, TIME_HOURS, aBufTime, sizeof(aBufTime));
	str_format(aBuf, sizeof(aBuf), "%s\'s current race time is %s", pName, aBufTime);
	pSelf->SendChat(-1, TEAM_ALL, aBuf, pResult->m_ClientId);
}

void CGameContext::ConTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	char aBufTime[32];
	char aBuf[64];
	int64_t Time = (int64_t)100 * (float)(pSelf->Server()->Tick() - pChr->m_StartTime) / ((float)pSelf->Server()->TickSpeed());
	str_time(Time, TIME_HOURS, aBufTime, sizeof(aBufTime));
	str_format(aBuf, sizeof(aBuf), "Your time is %s", aBufTime);
	pSelf->SendBroadcast(aBuf, pResult->m_ClientId);
}

static const char s_aaMsg[4][128] = {"game/round timer.", "broadcast.", "both game/round timer and broadcast.", "racetime."};

void CGameContext::ConSetTimerType(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(!CheckClientId(pResult->m_ClientId))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	char aBuf[128];

	if(pResult->NumArguments() > 0)
	{
		int OldType = pPlayer->m_TimerType;
		bool Result = false;

		if(str_comp_nocase(pResult->GetString(0), "default") == 0)
			Result = pPlayer->SetTimerType(CPlayer::TIMERTYPE_DEFAULT);
		else if(str_comp_nocase(pResult->GetString(0), "gametimer") == 0)
			Result = pPlayer->SetTimerType(CPlayer::TIMERTYPE_GAMETIMER);
		else if(str_comp_nocase(pResult->GetString(0), "broadcast") == 0)
			Result = pPlayer->SetTimerType(CPlayer::TIMERTYPE_BROADCAST);
		else if(str_comp_nocase(pResult->GetString(0), "both") == 0)
			Result = pPlayer->SetTimerType(CPlayer::TIMERTYPE_GAMETIMER_AND_BROADCAST);
		else if(str_comp_nocase(pResult->GetString(0), "none") == 0)
			Result = pPlayer->SetTimerType(CPlayer::TIMERTYPE_NONE);
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Unknown parameter. Accepted values: default, gametimer, broadcast, both, none");
			return;
		}

		if(!Result)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", "Selected timertype is not supported by your client");
			return;
		}

		if((OldType == CPlayer::TIMERTYPE_BROADCAST || OldType == CPlayer::TIMERTYPE_GAMETIMER_AND_BROADCAST) && (pPlayer->m_TimerType == CPlayer::TIMERTYPE_GAMETIMER || pPlayer->m_TimerType == CPlayer::TIMERTYPE_NONE))
			pSelf->SendBroadcast("", pResult->m_ClientId);
	}

	if(pPlayer->m_TimerType <= CPlayer::TIMERTYPE_SIXUP && pPlayer->m_TimerType >= CPlayer::TIMERTYPE_GAMETIMER)
		str_format(aBuf, sizeof(aBuf), "Timer is displayed in %s", s_aaMsg[pPlayer->m_TimerType]);
	else if(pPlayer->m_TimerType == CPlayer::TIMERTYPE_NONE)
		str_copy(aBuf, "Timer isn't displayed.");

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
}

void CGameContext::ConRescue(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!g_Config.m_SvRescue && !Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "Rescue is not enabled on this server and you're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}

	bool GoRescue = true;

	if(pPlayer->m_RescueMode == RESCUEMODE_MANUAL)
	{
		// if character can't set his rescue state then we should rescue him instead
		GoRescue = !pChr->TrySetRescue(RESCUEMODE_MANUAL);
	}

	if(GoRescue)
	{
		pChr->Rescue();
		pChr->UnFreeze();
	}
}

void CGameContext::ConRescueMode(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!g_Config.m_SvRescue && !Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "Rescue is not enabled on this server and you're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}

	if(str_comp_nocase(pResult->GetString(0), "auto") == 0)
	{
		if(pPlayer->m_RescueMode != RESCUEMODE_AUTO)
		{
			pPlayer->m_RescueMode = RESCUEMODE_AUTO;

			pSelf->SendChatTarget(pPlayer->GetCid(), "Rescue mode changed to auto.");
		}

		return;
	}

	if(str_comp_nocase(pResult->GetString(0), "manual") == 0)
	{
		if(pPlayer->m_RescueMode != RESCUEMODE_MANUAL)
		{
			pPlayer->m_RescueMode = RESCUEMODE_MANUAL;

			pSelf->SendChatTarget(pPlayer->GetCid(), "Rescue mode changed to manual.");
		}

		return;
	}

	if(str_comp_nocase(pResult->GetString(0), "list") == 0)
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "Available rescue modes: auto, manual");
	}
	else if(str_comp_nocase(pResult->GetString(0), "") == 0)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Current rescue mode: %s.", pPlayer->m_RescueMode == RESCUEMODE_MANUAL ? "manual" : "auto");
		pSelf->SendChatTarget(pPlayer->GetCid(), aBuf);
	}
	else
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "Unknown argument. Check '/rescuemode list'");
	}
}

void CGameContext::ConTeleTo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pCallingPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pCallingPlayer)
		return;
	CCharacter *pCallingCharacter = pCallingPlayer->GetCharacter();
	if(!pCallingCharacter)
		return;

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pCallingPlayer->GetCid(), "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}

	vec2 Pos = {};

	if(pResult->NumArguments() == 0)
	{
		// Set calling tee's position to the origin of its spectating viewport
		Pos = pCallingPlayer->m_ViewPos;
	}
	else
	{
		// Search for player with this name
		int ClientId;
		for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			if(str_comp(pResult->GetString(0), pSelf->Server()->ClientName(ClientId)) == 0)
				break;
		}
		if(ClientId == MAX_CLIENTS)
		{
			pSelf->SendChatTarget(pCallingPlayer->GetCid(), "No player with this name found.");
			return;
		}

		CPlayer *pDestPlayer = pSelf->m_apPlayers[ClientId];
		if(!pDestPlayer)
			return;
		CCharacter *pDestCharacter = pDestPlayer->GetCharacter();
		if(!pDestCharacter)
			return;

		// Set calling tee's position to that of the destination tee
		Pos = pDestCharacter->m_Pos;
	}

	// Teleport tee
	pSelf->Teleport(pCallingCharacter, Pos);
	pCallingCharacter->ResetJumps();
	pCallingCharacter->UnFreeze();
	pCallingCharacter->ResetVelocity();
	pCallingPlayer->m_LastTeleTee.Save(pCallingCharacter);
}

void CGameContext::ConTeleXY(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pCallingPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pCallingPlayer)
		return;
	CCharacter *pCallingCharacter = pCallingPlayer->GetCharacter();
	if(!pCallingCharacter)
		return;

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pCallingPlayer->GetCid(), "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}

	vec2 Pos = {};

	if(pResult->NumArguments() != 2)
	{
		pSelf->SendChatTarget(pCallingPlayer->GetCid(), "Can't recognize specified arguments. Usage: /tpxy x y, e.g. /tpxy 9 3.");
		return;
	}
	else
	{
		float BaseX = 0.f, BaseY = 0.f;

		CMapItemLayerTilemap *pGameLayer = pSelf->m_Layers.GameLayer();
		constexpr float OuterKillTileBoundaryDistance = 201 * 32.f;
		float MapWidth = (pGameLayer->m_Width * 32) + (OuterKillTileBoundaryDistance * 2.f), MapHeight = (pGameLayer->m_Height * 32) + (OuterKillTileBoundaryDistance * 2.f);

		const auto DetermineCoordinateRelativity = [](const char *pInString, const float AbsoluteDefaultValue, float &OutFloat) -> bool {
			// mode 0 = abs, 1 = sub, 2 = add

			// Relative?
			const char *pStrDelta = str_startswith(pInString, "~");

			float d;
			if(!str_tofloat(pStrDelta ? pStrDelta : pInString, &d))
				return false;

			// Is the number valid?
			if(std::isnan(d) || std::isinf(d))
				return false;

			// Convert our gleaned 'display' coordinate to an actual map coordinate
			d *= 32.f;

			OutFloat = (pStrDelta ? AbsoluteDefaultValue : 0) + d;
			return true;
		};

		if(!DetermineCoordinateRelativity(pResult->GetString(0), pCallingPlayer->m_ViewPos.x, BaseX))
		{
			pSelf->SendChatTarget(pCallingPlayer->GetCid(), "Invalid X coordinate.");
			return;
		}
		if(!DetermineCoordinateRelativity(pResult->GetString(1), pCallingPlayer->m_ViewPos.y, BaseY))
		{
			pSelf->SendChatTarget(pCallingPlayer->GetCid(), "Invalid Y coordinate.");
			return;
		}

		Pos = {std::clamp(BaseX, (-OuterKillTileBoundaryDistance) + 1.f, (-OuterKillTileBoundaryDistance) + MapWidth - 1.f), std::clamp(BaseY, (-OuterKillTileBoundaryDistance) + 1.f, (-OuterKillTileBoundaryDistance) + MapHeight - 1.f)};
	}

	// Teleport tee
	pSelf->Teleport(pCallingCharacter, Pos);
	pCallingCharacter->ResetJumps();
	pCallingCharacter->UnFreeze();
	pCallingCharacter->ResetVelocity();
	pCallingPlayer->m_LastTeleTee.Save(pCallingCharacter);
}

void CGameContext::ConTeleCursor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}

	// default to view pos when character is not available
	vec2 Pos = pPlayer->m_ViewPos;
	if(pResult->NumArguments() == 0 && !pPlayer->IsPaused() && pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
	{
		vec2 Target = vec2(pChr->Core()->m_Input.m_TargetX, pChr->Core()->m_Input.m_TargetY);
		Pos = pPlayer->m_CameraInfo.ConvertTargetToWorld(pPlayer->GetCharacter()->GetPos(), Target);
	}
	else if(pResult->NumArguments() > 0)
	{
		int ClientId;
		for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			if(str_comp(pResult->GetString(0), pSelf->Server()->ClientName(ClientId)) == 0)
				break;
		}
		if(ClientId == MAX_CLIENTS)
		{
			pSelf->SendChatTarget(pPlayer->GetCid(), "No player with this name found.");
			return;
		}
		CPlayer *pPlayerTo = pSelf->m_apPlayers[ClientId];
		if(!pPlayerTo)
			return;
		CCharacter *pChrTo = pPlayerTo->GetCharacter();
		if(!pChrTo)
			return;
		Pos = pChrTo->m_Pos;
	}
	pSelf->Teleport(pChr, Pos);
	pChr->ResetJumps();
	pChr->UnFreeze();
	pChr->ResetVelocity();
	pPlayer->m_LastTeleTee.Save(pChr);
}

void CGameContext::ConLastTele(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}
	if(!pPlayer->m_LastTeleTee.GetPos().x)
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "You haven't previously teleported. Use /tp before using this command.");
		return;
	}
	pPlayer->m_LastTeleTee.Load(pChr, pChr->Team(), true);
	pPlayer->Pause(CPlayer::PAUSE_NONE, true);
}

CCharacter *CGameContext::GetPracticeCharacter(IConsole::IResult *pResult)
{
	if(!CheckClientId(pResult->m_ClientId))
		return nullptr;
	CPlayer *pPlayer = m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return nullptr;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return nullptr;

	CGameTeams &Teams = m_pController->Teams();
	int Team = GetDDRaceTeam(pResult->m_ClientId);
	if(!Teams.IsPractice(Team))
	{
		SendChatTarget(pPlayer->GetCid(), "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return nullptr;
	}
	return pChr;
}

void CGameContext::ConPracticeToTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPracticeCharacter(pResult);
	if(pChr)
	{
		if(pSelf->Collision()->TeleOuts(pResult->GetInteger(0) - 1).empty())
		{
			pSelf->SendChatTarget(pChr->GetPlayer()->GetCid(), "There is no teleporter with that index on the map.");
			return;
		}

		ConToTeleporter(pResult, pUserData);
		pChr->ResetJumps();
		pChr->UnFreeze();
		pChr->ResetVelocity();
		pChr->GetPlayer()->m_LastTeleTee.Save(pChr);
	}
}

void CGameContext::ConPracticeToCheckTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPracticeCharacter(pResult);
	if(pChr)
	{
		if(pSelf->Collision()->TeleCheckOuts(pResult->GetInteger(0) - 1).empty())
		{
			pSelf->SendChatTarget(pChr->GetPlayer()->GetCid(), "There is no checkpoint teleporter with that index on the map.");
			return;
		}

		ConToCheckTeleporter(pResult, pUserData);
		pChr->ResetJumps();
		pChr->UnFreeze();
		pChr->ResetVelocity();
		pChr->GetPlayer()->m_LastTeleTee.Save(pChr);
	}
}

void CGameContext::ConPracticeUnSolo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	if(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "Command is not available on solo servers");
		return;
	}

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}
	pChr->SetSolo(false);
}

void CGameContext::ConPracticeSolo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	if(g_Config.m_SvTeam == SV_TEAM_FORBIDDEN || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "Command is not available on solo servers");
		return;
	}

	CGameTeams &Teams = pSelf->m_pController->Teams();
	int Team = pSelf->GetDDRaceTeam(pResult->m_ClientId);
	if(!Teams.IsPractice(Team))
	{
		pSelf->SendChatTarget(pPlayer->GetCid(), "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
		return;
	}
	pChr->SetSolo(true);
}

void CGameContext::ConPracticeUnDeep(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	auto *pChr = pSelf->GetPracticeCharacter(pResult);
	if(!pChr)
		return;

	pChr->SetDeepFrozen(false);
	pChr->UnFreeze();
}

void CGameContext::ConPracticeDeep(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	auto *pChr = pSelf->GetPracticeCharacter(pResult);
	if(!pChr)
		return;

	pChr->SetDeepFrozen(true);
}

void CGameContext::ConPracticeUnLiveFreeze(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	auto *pChr = pSelf->GetPracticeCharacter(pResult);
	if(!pChr)
		return;

	pChr->SetLiveFrozen(false);
}

void CGameContext::ConPracticeLiveFreeze(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	auto *pChr = pSelf->GetPracticeCharacter(pResult);
	if(!pChr)
		return;

	pChr->SetLiveFrozen(true);
}

void CGameContext::ConPracticeShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConShotgun(pResult, pUserData);
}

void CGameContext::ConPracticeGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConGrenade(pResult, pUserData);
}

void CGameContext::ConPracticeLaser(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConLaser(pResult, pUserData);
}

void CGameContext::ConPracticeJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConJetpack(pResult, pUserData);
}

void CGameContext::ConPracticeEndlessJump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConEndlessJump(pResult, pUserData);
}

void CGameContext::ConPracticeSetJumps(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConSetJumps(pResult, pUserData);
}

void CGameContext::ConPracticeWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConWeapons(pResult, pUserData);
}

void CGameContext::ConPracticeUnShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnShotgun(pResult, pUserData);
}

void CGameContext::ConPracticeUnGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnGrenade(pResult, pUserData);
}

void CGameContext::ConPracticeUnLaser(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnLaser(pResult, pUserData);
}

void CGameContext::ConPracticeUnJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnJetpack(pResult, pUserData);
}

void CGameContext::ConPracticeUnEndlessJump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnEndlessJump(pResult, pUserData);
}

void CGameContext::ConPracticeUnWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnWeapons(pResult, pUserData);
}

void CGameContext::ConPracticeNinja(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConNinja(pResult, pUserData);
}

void CGameContext::ConPracticeUnNinja(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnNinja(pResult, pUserData);
}

void CGameContext::ConPracticeEndlessHook(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConEndlessHook(pResult, pUserData);
}

void CGameContext::ConPracticeUnEndlessHook(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConUnEndlessHook(pResult, pUserData);
}

void CGameContext::ConPracticeToggleInvincible(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConToggleInvincible(pResult, pUserData);
}

void CGameContext::ConPracticeAddWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConAddWeapon(pResult, pUserData);
}

void CGameContext::ConPracticeRemoveWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->GetPracticeCharacter(pResult))
		ConRemoveWeapon(pResult, pUserData);
}

void CGameContext::ConProtectedKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	int CurrTime = (pSelf->Server()->Tick() - pChr->m_StartTime) / pSelf->Server()->TickSpeed();
	if(g_Config.m_SvKillProtection != 0 && CurrTime >= (60 * g_Config.m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
	{
		pPlayer->KillCharacter(WEAPON_SELF);
	}
}

void CGameContext::ConPoints(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(pResult->NumArguments() > 0)
	{
		if(!g_Config.m_SvHideScore)
			pSelf->Score()->ShowPoints(pResult->m_ClientId, pResult->GetString(0));
		else
			pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"chatresp",
				"Showing the global points of other players is not allowed on this server.");
	}
	else
		pSelf->Score()->ShowPoints(pResult->m_ClientId,
			pSelf->Server()->ClientName(pResult->m_ClientId));
}

void CGameContext::ConTopPoints(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Showing the global top points is not allowed on this server.");
		return;
	}

	if(pResult->NumArguments() > 0)
		pSelf->Score()->ShowTopPoints(pResult->m_ClientId, pResult->GetInteger(0));
	else
		pSelf->Score()->ShowTopPoints(pResult->m_ClientId);
}

void CGameContext::ConTimeCP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(g_Config.m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
			"Showing the checkpoint times is not allowed on this server.");
		return;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	const char *pName = pResult->GetString(0);
	pSelf->Score()->LoadPlayerTimeCp(pResult->m_ClientId, pName);
}
