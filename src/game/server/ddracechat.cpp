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

bool CheckClientID(int ClientID);

void CGameContext::ConCredits(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChatTarget(pResult->m_ClientID,"Teeworlds Team takes most of the credits also");
	pSelf->SendChatTarget(pResult->m_ClientID,"This mod was originally created by \'3DA\'");
	pSelf->SendChatTarget(pResult->m_ClientID,"Now it is maintained & re-coded by:");
	pSelf->SendChatTarget(pResult->m_ClientID,"\'[Egypt]GreYFoX@GTi\' and \'[BlackTee]den\'");
	pSelf->SendChatTarget(pResult->m_ClientID,"Others Helping on the code: \'heinrich5991\', \'ravomavain\', \'Trust o_0 Aeeeh ?!\', \'noother\', \'<3 fisted <3\' & \'LemonFace\'");
	pSelf->SendChatTarget(pResult->m_ClientID,"Documentation: Zeta-Hoernchen, Entities: Fisico");
	pSelf->SendChatTarget(pResult->m_ClientID,"Code (in the past): \'3DA\' and \'Fluxid\'");
	pSelf->SendChatTarget(pResult->m_ClientID,"Please check the changelog on DDRace.info.");
	pSelf->SendChatTarget(pResult->m_ClientID,"Also the commit log on github.com/GreYFoXGTi/DDRace.");
}

void CGameContext::ConInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChatTarget(pResult->m_ClientID,"DDRace Mod. Version: " GAME_VERSION);
#if defined( GIT_SHORTREV_HASH )
	pSelf->SendChatTarget(pResult->m_ClientID,"Git revision hash: " GIT_SHORTREV_HASH);
#endif
	pSelf->SendChatTarget(pResult->m_ClientID,"Official site: DDRace.info");
	pSelf->SendChatTarget(pResult->m_ClientID,"For more Info /cmdlist");
	pSelf->SendChatTarget(pResult->m_ClientID,"Or visit DDRace.info");
}

void CGameContext::ConHelp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments() == 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"/cmdlist will show a list of all chat commands");
		pSelf->SendChatTarget(pResult->m_ClientID,"/help + any command will show you the help for this command");
		pSelf->SendChatTarget(pResult->m_ClientID,"Example /help settings will display the help about ");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo = pSelf->Console()->GetCommandInfo(pArg, CFGFLAG_SERVER, false);
		if(pCmdInfo && pCmdInfo->m_pHelp)
			pSelf->SendChatTarget(pResult->m_ClientID,pCmdInfo->m_pHelp);
		else
				pSelf->SendChatTarget(pResult->m_ClientID,"Command is either unknown or you have given a blank command without any parameters.");
	}
}

void CGameContext::ConSettings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments() == 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"to check a server setting say /settings and setting's name, setting names are:");
		pSelf->SendChatTarget(pResult->m_ClientID,"teams, cheats, collision, hooking, endlesshooking, me, ");
		pSelf->SendChatTarget(pResult->m_ClientID,"hitting, oldlaser, timeout, votes, pause and scores");
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
			pSelf->SendChatTarget(pResult->m_ClientID,aBuf);
		}
		else if(str_comp(pArg, "collision") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,ColTemp?"Players can collide on this server":"Players Can't collide on this server");
		}
		else if(str_comp(pArg, "hooking") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,HookTemp?"Players can hook each other on this server":"Players Can't hook each other on this server");
		}
		else if(str_comp(pArg, "endlesshooking") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvEndlessDrag?"Players can hook time is unlimited":"Players can hook time is limited");
		}
		else if(str_comp(pArg, "hitting") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvHit?"Player's weapons affect each other":"Player's weapons has no affect on each other");
		}
		else if(str_comp(pArg, "oldlaser") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvOldLaser?"Lasers can hit you if you shot them and that they pull you towards the bounce origin (Like DDRace Beta)":"Lasers can't hit you if you shot them, and they pull others towards the shooter");
		}
		else if(str_comp(pArg, "me") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvSlashMe?"Players can use /me commands the famous IRC Command":"Players Can't use the /me command");
		}
		else if(str_comp(pArg, "timeout") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "The Server Timeout is currently set to %d", g_Config.m_ConnTimeout);
			pSelf->SendChatTarget(pResult->m_ClientID,aBuf);
		}
		else if(str_comp(pArg, "votes") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvVoteKick?"Players can use Callvote menu tab to kick offenders":"Players Can't use the Callvote menu tab to kick offenders");
			if(g_Config.m_SvVoteKick)
				str_format(aBuf, sizeof(aBuf), "Players are banned for %d second(s) if they get voted off", g_Config.m_SvVoteKickBantime);
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvVoteKickBantime?aBuf:"Players are just kicked and not banned if they get voted off");
		}
		else if(str_comp(pArg, "pause") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvPauseable?g_Config.m_SvPauseTime?"/pause is available on this server and it pauses your time too":"/pause is available on this server but it doesn't pause your time":"/pause is NOT available on this server");
		}
		else if(str_comp(pArg, "scores") == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvHideScore?"Scores are private on this server":"Scores are public on this server");
		}
	}
}

void CGameContext::ConRules(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	bool Printed = false;
	if(g_Config.m_SvDDRaceRules)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"No blocking.");
		pSelf->SendChatTarget(pResult->m_ClientID,"No insulting / spamming.");
		pSelf->SendChatTarget(pResult->m_ClientID,"No fun voting / vote spamming.");
		pSelf->SendChatTarget(pResult->m_ClientID,"Breaking any of these rules will result in a penalty, decided by server admins.");
		Printed = true;
	}
	if(g_Config.m_SvRulesLine1[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine1);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine2[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine2);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine3[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine3);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine4[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine4);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine5[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine5);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine6[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine6);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine7[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine7);
		Printed = true;
	}
	if(g_Config.m_SvRulesLine8[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine8);
		Printed=true;
	}
	if(g_Config.m_SvRulesLine9[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine9);
		Printed=true;
	}
	if(g_Config.m_SvRulesLine10[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID,g_Config.m_SvRulesLine10);
		Printed = true;
	}
	if(!Printed)
		pSelf->SendChatTarget(pResult->m_ClientID,"No Rules Defined, Kill em all!!");
}

void CGameContext::ConTogglePause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	char aBuf[128];

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
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
				pSelf->SendChatTarget(pResult->m_ClientID,"You can\'t pause that often.");
		}
		else if(pPlayer->GetTeam()==TEAM_SPECTATORS && pPlayer->m_InfoSaved && pPlayer->m_ForcePauseTime == 0)
		{
			pPlayer->m_PauseInfo.m_Respawn = true;
			pPlayer->SetTeam(TEAM_RED);
			pPlayer->m_InfoSaved = false;
			//pPlayer->LoadCharacter();//TODO:Check if this system Works
		}
		else if(pChr)
			pSelf->SendChatTarget(pResult->m_ClientID,pChr->GetWeaponGot(WEAPON_NINJA)?"You can't use /pause while you are a ninja":(!pChr->IsGrounded())?"You can't use /pause while you are a in air":"You can't use /pause while you are moving");
		else if(pPlayer->m_ForcePauseTime > 0)
		{
			str_format(aBuf, sizeof(aBuf), "You have been force-paused. %ds left.", pPlayer->m_ForcePauseTime/pSelf->Server()->TickSpeed());
			pSelf->SendChatTarget(pResult->m_ClientID,aBuf);
		}
		else if(pPlayer->m_ForcePauseTime < 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,"You have been force-paused.");
		}
		else
			pSelf->SendChatTarget(pResult->m_ClientID,"No pause data saved.");
	}
	else
		pSelf->SendChatTarget(pResult->m_ClientID,"Pause isn't allowed on this server.");
}

void CGameContext::ConTop5(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(g_Config.m_SvHideScore)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"Showing the top 5 is not allowed on this server.");
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
	if(!CheckClientID(pResult->m_ClientID)) return;

	if(g_Config.m_SvUseSQL)
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
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
			
		pSelf->SendChatTarget(pResult->m_ClientID,"/times needs 0, 1 or 2 parameter. 1. = name, 2. = start number");
		pSelf->SendChatTarget(pResult->m_ClientID,"Example: /times, /times me, /times Hans, /times \"Papa Smurf\" 5");
		pSelf->SendChatTarget(pResult->m_ClientID,"Bad: /times Papa Smurf 5 # Good: /times \"Papa Smurf\" 5 ");
	}	
}
#endif

void CGameContext::ConRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(pResult->NumArguments() > 0)
		if(!g_Config.m_SvHideScore)
			pSelf->Score()->ShowRank(pResult->m_ClientID, pResult->GetString(0), true);
		else
			pSelf->SendChatTarget(pResult->m_ClientID,"Showing the rank of other players is not allowed on this server.");
	else
		pSelf->Score()->ShowRank(pResult->m_ClientID, pSelf->Server()->ClientName(pResult->m_ClientID));
}

void CGameContext::ConJoinTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	if(pSelf->m_VoteCloseTime && pSelf->m_VoteCreator == pResult->m_ClientID)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"You are running a vote please try again after the vote is done!");
		return;
	}
	else if(g_Config.m_SvTeam == 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"Admin has disabled teams");
		return;
	}
	else if (g_Config.m_SvTeam == 2)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"You must join to any team and play with anybody or you will not play");
	}
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];

	if(pResult->NumArguments() > 0)
	{
		if(pPlayer->GetCharacter() == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID,"You can't change teams while you are dead/a spectator.");
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
					pSelf->SendChatTarget(pResult->m_ClientID,"You can\'t join teams that fast!");
				}
			}
			else
			{
				pSelf->SendChatTarget(pResult->m_ClientID,"You cannot join this team at this time");
			}
		}
	}
	else
	{
		char aBuf[512];
		if(!pPlayer->IsPlaying())
		{
			pSelf->SendChatTarget(pResult->m_ClientID,"You can't check your team while you are dead/a spectator.");
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "You are in team %d", ((CGameControllerDDRace*)pSelf->m_pController)->m_Teams.m_Core.Team(pResult->m_ClientID));
			pSelf->SendChatTarget(pResult->m_ClientID,aBuf);
		}
	}
}

void CGameContext::ConMe(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	char aBuf[256 + 24];

	str_format(aBuf, 256 + 24, "'%s' %s", pSelf->Server()->ClientName(pResult->m_ClientID), pResult->GetString(0));
	if(g_Config.m_SvSlashMe)
		pSelf->SendChat(-2, CGameContext::CHAT_ALL, aBuf, pResult->m_ClientID);
	else
		pSelf->SendChatTarget(pResult->m_ClientID,"/me is disabled on this server, admin can enable it by using sv_slash_me");
}

void CGameContext::ConToggleEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	pChr->m_EyeEmote = !pChr->m_EyeEmote;
	pSelf->SendChatTarget(pResult->m_ClientID,(pChr->m_EyeEmote) ? "You can now use the preset eye emotes." : "You don't have any eye emotes, remember to bind some. (until you die)");
}

void CGameContext::ConEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	if (pResult->NumArguments() == 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID,"Emote commands are: /emote surprise /emote blink /emote close /emote angry /emote happy /emote pain");
		pSelf->SendChatTarget(pResult->m_ClientID,"Example: /emote surprise 10 for 10 seconds or /emote surprise (default 1 second)");
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
				pSelf->SendChatTarget(pResult->m_ClientID,"Unknown emote... Say /emote");

			int Duration = 1;
			if (pResult->NumArguments() > 1)
				Duration = pResult->GetInteger(1);

			pChr->m_DefEmoteReset = pSelf->Server()->Tick() + Duration * pSelf->Server()->TickSpeed();
		}
	}
}

void CGameContext::ConShowOthersChat(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
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
			pSelf->SendChatTarget(pResult->m_ClientID,"Showing players from other teams is only available with DDRace Client, http://DDRace.info");
	}
	else
		pSelf->SendChatTarget(pResult->m_ClientID,"Showing players from other teams is disabled by the server admin");
}

bool CheckClientID(int ClientID)
{
	dbg_assert(ClientID >= 0 || ClientID < MAX_CLIENTS, "The Client ID is wrong");
	if(ClientID < 0 || ClientID >= MAX_CLIENTS) return false;
	return true;
}
