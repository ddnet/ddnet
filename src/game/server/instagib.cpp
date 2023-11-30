// do we need some gctf system.h?

#include <engine/shared/config.h>

#include "entities/character.h"
#include "gamecontext.h"
#include "gamemodes/DDRace.h"
#include "gamemodes/gctf.h"
#include "gamemodes/ictf.h"
#include "gamemodes/mod.h"
#include "player.h"

#include "instagib.h"

// gctf
void CGameContext::ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		if(pSelf->m_pController)
			pSelf->m_pController->CheckGameInfo();
	}
}

// gctf
void CGameContext::ConchainResetInstasettingTees(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		for(auto *pPlayer : pSelf->m_apPlayers)
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

// gctf
void CGameContext::ConchainInstaSettingsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->UpdateVoteCheckboxes();
	pSelf->RefreshVotes();
}

// gctf
void CGameContext::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->m_pController->IsTeamplay())
		return;

	int rnd = 0;
	int PlayerTeam = 0;
	int aPlayer[MAX_CLIENTS];

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			aPlayer[PlayerTeam++] = i;

	// pSelf->SendGameMsg(GAMEMSG_TEAM_SHUFFLE, -1);

	//creating random permutation
	for(int i = PlayerTeam; i > 1; i--)
	{
		rnd = rand() % i;
		int tmp = aPlayer[rnd];
		aPlayer[rnd] = aPlayer[i - 1];
		aPlayer[i - 1] = tmp;
	}
	//uneven Number of Players?
	rnd = PlayerTeam % 2 ? rand() % 2 : 0;

	for(int i = 0; i < PlayerTeam; i++)
		pSelf->m_pController->DoTeamChange(pSelf->m_apPlayers[aPlayer[i]], i < (PlayerTeam + rnd) / 2 ? TEAM_RED : TEAM_BLUE, false);
}

// gctf
void CGameContext::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SwapTeams();
}

void CGameContext::ConSwapTeamsRandom(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(rand() % 2)
		pSelf->SwapTeams();
	else
		dbg_msg("swap", "did not swap due to random chance");
}

void CGameContext::SwapTeams()
{
	if(!m_pController->IsTeamplay())
		return;

	SendGameMsg(protocol7::GAMEMSG_TEAM_SWAP, -1);

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			m_pController->DoTeamChange(m_apPlayers[i], m_apPlayers[i]->GetTeam()^1, false);
	}

	m_pController->SwapTeamscore();
}

bool CGameContext::OnInstaChatMessage(const CNetMsg_Cl_Say *pMsg, int Length, int &Team, CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();

	if(pMsg->m_Team || !AllowPublicChat(pPlayer))
		Team = ((pPlayer->GetTeam() == TEAM_SPECTATORS) ? CHAT_SPEC : pPlayer->GetTeam()); // gctf
	else
		Team = CHAT_ALL;

	// gctf warn on ping if cant respond
	if(Team == CHAT_ALL && pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		for(const CPlayer *pSpecPlayer : m_apPlayers)
		{
			if(!pSpecPlayer)
				continue;
			if(pSpecPlayer->GetTeam() != TEAM_SPECTATORS)
				continue;
			if(AllowPublicChat(pSpecPlayer))
				continue;
			if(!str_find_nocase(pMsg->m_pMessage, Server()->ClientName(pSpecPlayer->GetCID())))
				continue;

			char aChatText[256];
			str_format(aChatText, sizeof(aChatText), "Warning: '%s' got pinged in chat but can not respond", Server()->ClientName(pSpecPlayer->GetCID()));
			SendChat(-1, CGameContext::CHAT_ALL, aChatText);
			SendChat(-1, CGameContext::CHAT_ALL, "turn off tournament chat or make sure there are enough in game slots");
			break;
		}
	}

	// gctf fine grained chat spam control
	if(pMsg->m_pMessage[0] != '/')
	{
		bool RateLimit = false;
		if(g_Config.m_SvChatRatelimitSpectators && pPlayer->GetTeam() == TEAM_SPECTATORS)
		{
			if(g_Config.m_SvChatRatelimitDebug)
				dbg_msg("ratelimit", "m_SvChatRatelimitSpectators %s", pMsg->m_pMessage);
			RateLimit = true;
		}
		if(g_Config.m_SvChatRatelimitPublicChat && Team == CHAT_ALL)
		{
			if(g_Config.m_SvChatRatelimitDebug)
				dbg_msg("ratelimit", "m_SvChatRatelimitPublicChat %s", pMsg->m_pMessage);
			RateLimit = true;
		}
		if(g_Config.m_SvChatRatelimitLongMessages && Length >= 12)
		{
			if(g_Config.m_SvChatRatelimitDebug)
				dbg_msg("ratelimit", "m_SvChatRatelimitLongMessages %s", pMsg->m_pMessage);
			RateLimit = true;
		}
		if(g_Config.m_SvChatRatelimitNonCalls)
		{
			// grep -r teamchat | cut -d: -f8- | sort | uniq -c | sort -nr
			const char aaCalls[][24] = {
				"help",
				"mid",
				"top",
				"bot",
				"Back!!!",
				"back",
				"Enemy Base!",
				"enemy_base",
				"mid!",
				"TOP !",
				"Mid",
				"back! / help!",
				"top!",
				"Top",
				"MID !",
				"BACK !",
				"bottom",
				"Ready!!",
				"back!",
				"BOT !",
				"help",
				"Back",
				"ENNEMIE BASE !",
				"Backkk",
				"back!!",
				"left",
				"READY !",
				"Double Attack!!",
				"Mid!",
				"Help! / Back!",
				"bot!",
				"Back!",
				"Top!",
				"🅷🅴🅻🅿",
				"right",
				"enemy base",
				"b",
				"Bottom",
				"Ready!",
				"middle",
				"bottom!",
				"Bot!",
				"Bot",
				"right down",
				"pos?",
				"atk",
				"ready!",
				"DoubleAttack!",
				"Our base!!!",
				"HELP !",
				"def",
				"pos",
				"Back!Help!",
				"right top",
				"pos ?",
				"Take weapons",
				"go",
				"enemy base!",
				"deff",
				"BACK!!!",
				"safe",
			};
			bool IsCall = false;
			for(const char *aCall : aaCalls)
			{
				if(!str_comp_nocase(aCall, pMsg->m_pMessage))
				{
					IsCall = true;
					break;
				}
			}
			if(!IsCall)
			{
				if(g_Config.m_SvChatRatelimitDebug)
					dbg_msg("ratelimit", "m_SvChatRatelimitNonCalls %s", pMsg->m_pMessage);
				RateLimit = true;
			}
		}
		if(g_Config.m_SvChatRatelimitSpam)
		{
			char aaSpams[][24] = {
				"discord.gg",
				"fuck",
				"idiot",
				"http://",
				"https://",
				"www.",
				".com",
				".de",
				".io"};
			for(const char *aSpam : aaSpams)
			{
				if(str_find_nocase(aSpam, pMsg->m_pMessage))
				{
					if(g_Config.m_SvChatRatelimitDebug)
						dbg_msg("ratelimit", "m_SvChatRatelimitSpam (bad word) %s", pMsg->m_pMessage);
					RateLimit = true;
					break;
				}
			}
			if(str_contains_ip(pMsg->m_pMessage))
			{
				if(g_Config.m_SvChatRatelimitDebug)
					dbg_msg("ratelimit", "m_SvChatRatelimitSpam (ip) %s", pMsg->m_pMessage);
				RateLimit = true;
			}
		}
		if(RateLimit && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() * ((31 + Length) / 32) > Server()->Tick())
			return true;
	}

	// gctf bang commands
	// allow sending ! to chat or !!
	// swallow all other ! prefixed chat messages
	if(pMsg->m_pMessage[0] == '!' && pMsg->m_pMessage[1] && pMsg->m_pMessage[1] != '!')
	{
		ParseChatCmd('!', ClientID, pMsg->m_pMessage + 1);
		return true;
	}

	if(pMsg->m_pMessage[0] == '/')
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%d used %s", ClientID, pMsg->m_pMessage);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-command", aBuf);

		if(!str_comp_nocase(pMsg->m_pMessage + 1, "ready") || !str_comp_nocase(pMsg->m_pMessage + 1, "pause")) // gctf
		{
			m_pController->OnPlayerReadyChange(pPlayer);
			return true;
		}
		else if(!str_comp_nocase(pMsg->m_pMessage + 1, "shuffle")) // gctf
		{
			ComCallShuffleVote(ClientID);
			return true;
		}
		else if(!str_comp_nocase(pMsg->m_pMessage + 1, "swap")) // gctf
		{
			ComCallSwapTeamsVote(ClientID);
			return true;
		}
		else if(!str_comp_nocase(pMsg->m_pMessage + 1, "swap_random")) // gctf
		{
			ComCallSwapTeamsRandomVote(ClientID);
			return true;
		}
	}
	return false;
}

// gctf

void CGameContext::BangCommandVote(int ClientID, const char *pCommand, const char *pDesc)
{
	char aChatmsg[1024];
	str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s'", Server()->ClientName(ClientID), pDesc);
	CallVote(ClientID, pDesc, pCommand, "chat cmd", aChatmsg);
}

void CGameContext::ComCallShuffleVote(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;
	// not needed for bang command but for slash command
	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		SendChatTarget(ClientID, "Spectators aren't allowed to vote.");
		return;
	}
	BangCommandVote(ClientID, "shuffle_teams", "shuffle teams");
}

void CGameContext::ComCallSwapTeamsVote(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;
	// not needed for bang command but for slash command
	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		SendChatTarget(ClientID, "Spectators aren't allowed to vote.");
		return;
	}
	BangCommandVote(ClientID, "swap_teams", "swap teams");
}

void CGameContext::ComCallSwapTeamsRandomVote(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;
	// not needed for bang command but for slash command
	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		SendChatTarget(ClientID, "Spectators aren't allowed to vote.");
		return;
	}
	BangCommandVote(ClientID, "swap_teams_random", "swap teams (random)");
}

bool CGameContext::ParseChatCmd(char Prefix, int ClientID, const char *pCmdWithArgs)
{
	const int MAX_ARG_LEN = 256;
	char aCmd[MAX_ARG_LEN];
	int i;
	for(i = 0; pCmdWithArgs[i] && i < MAX_ARG_LEN; i++)
	{
		if(pCmdWithArgs[i] == ' ')
			break;
		aCmd[i] = pCmdWithArgs[i];
	}
	aCmd[i] = '\0';
	// int ROffset = m_pClient->m_ChatHelper.ChatCommandGetROffset(aCmd);
	int ROffset = -1; // TODO: add params with typed args: s,r,i

	// max 8 args of 128 len each
	const int MAX_ARGS = 16;
	char **ppArgs = new char *[MAX_ARGS];
	for(int x = 0; x < MAX_ARGS; ++x)
	{
		ppArgs[x] = new char[MAX_ARG_LEN];
		ppArgs[x][0] = '\0';
	}
	int NumArgs = 0;
	int k = 0;
	while(pCmdWithArgs[i])
	{
		if(k + 1 >= MAX_ARG_LEN)
		{
			dbg_msg("ddnet-insta", "ERROR: chat command has too long arg");
			break;
		}
		if(NumArgs + 1 >= MAX_ARGS)
		{
			dbg_msg("ddnet-insta", "ERROR: chat command has too many args");
			break;
		}
		if(pCmdWithArgs[i] == ' ')
		{
			// do not delimit on space
			// if we reached the r parameter
			if(NumArgs == ROffset)
			{
				// strip spaces from the beggining
				// add spaces in the middle and end
				if(ppArgs[NumArgs][0])
				{
					ppArgs[NumArgs][k] = pCmdWithArgs[i];
					k++;
					i++;
					continue;
				}
			}
			else if(ppArgs[NumArgs][0])
			{
				ppArgs[NumArgs][k] = '\0';
				k = 0;
				NumArgs++;
			}
			i++;
			continue;
		}
		ppArgs[NumArgs][k] = pCmdWithArgs[i];
		k++;
		i++;
	}
	if(ppArgs[NumArgs][0])
	{
		ppArgs[NumArgs][k] = '\0';
		NumArgs++;
	}

	char aArgsStr[128];
	aArgsStr[0] = '\0';
	for(i = 0; i < NumArgs; i++)
	{
		if(aArgsStr[0] != '\0')
			str_append(aArgsStr, ", ", sizeof(aArgsStr));
		str_append(aArgsStr, ppArgs[i], sizeof(aArgsStr));
	}

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "got cmd '%s' with %d args: %s", aCmd, NumArgs, aArgsStr);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bang-command", aBuf);

	bool match = OnBangCommand(ClientID, aCmd, NumArgs, (const char **)ppArgs);
	for(int x = 0; x < 8; ++x)
		delete[] ppArgs[x];
	delete[] ppArgs;
	return match;
}

bool CGameContext::AllowPublicChat(const CPlayer *pPlayer)
{
	if(!g_Config.m_SvTournamentChat)
		return true;

	if(g_Config.m_SvTournamentChat == 1 && pPlayer->GetTeam() == TEAM_SPECTATORS)
		return false;
	else if(g_Config.m_SvTournamentChat == 2)
		return false;
	return true;
}

bool CGameContext::OnBangCommand(int ClientID, const char *pCmd, int NumArgs, const char **ppArgs)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return false;

	if(!str_comp_nocase(pCmd, "set") || !str_comp_nocase(pCmd, "sett") || !str_comp_nocase(pCmd, "settings") || !str_comp_nocase(pCmd, "config"))
	{
		ShowCurrentInstagibConfigsMotd(ClientID, true);
		return true;
	}

	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		SendChatTarget(ClientID, "Spectators aren't allowed to vote.");
		return false;
	}

	int SetSlots = -1;
	const char aaVs[][16] = {"on", "n", "vs", "v"};
	for(const auto *pVs : aaVs)
	{
		for(int i = 1; i <= 8; i++)
		{
			char a1on1[32];
			str_format(a1on1, sizeof(a1on1), "%d%s%d", i, pVs, i);
			if(!str_comp_nocase(pCmd, a1on1))
			{
				SetSlots = i;
				break;
			}
			str_format(a1on1, sizeof(a1on1), "%s%d", pVs, i);
			if(!str_comp_nocase(pCmd, a1on1))
			{
				SetSlots = i;
				break;
			}
		}
	}

	if(SetSlots != -1)
	{
		char aCmd[512];
		str_format(aCmd, sizeof(aCmd), "sv_spectator_slots %d", MAX_CLIENTS - SetSlots * 2);
		char aDesc[512];
		str_format(aDesc, sizeof(aDesc), "%dvs%d", SetSlots, SetSlots);
		BangCommandVote(ClientID, aCmd, aDesc);
	}
	else if(!str_comp_nocase(pCmd, "restart") || !str_comp_nocase(pCmd, "reload"))
	{
		int Seconds = NumArgs > 0 ? atoi(ppArgs[0]) : 10;
		Seconds = clamp(Seconds, 1, 200);
		char aCmd[512];
		str_format(aCmd, sizeof(aCmd), "restart %d", Seconds);
		char aDesc[512];
		str_format(aDesc, sizeof(aDesc), "restart %d", Seconds);
		BangCommandVote(ClientID, aCmd, aDesc);
	}
	else if(!str_comp_nocase(pCmd, "ready") || !str_comp_nocase(pCmd, "pause"))
	{
		m_pController->OnPlayerReadyChange(pPlayer);
	}
	else if(!str_comp_nocase(pCmd, "shuffle"))
	{
		ComCallShuffleVote(ClientID);
	}
	else if(!str_comp_nocase(pCmd, "swap"))
	{
		ComCallSwapTeamsVote(ClientID);
	}
	else if(!str_comp_nocase(pCmd, "swap_random"))
	{
		ComCallSwapTeamsRandomVote(ClientID);
	}
	else if(!str_comp_nocase(pCmd, "gamestate"))
	{
		if(NumArgs > 0)
		{
			if(!str_comp_nocase(ppArgs[0], "on"))
				pPlayer->m_GameStateBroadcast = true;
			else if(!str_comp_nocase(ppArgs[0], "off"))
				pPlayer->m_GameStateBroadcast = false;
			else
				SendChatTarget(ClientID, "usage: !gamestate [on|off]");
		}
		else
			pPlayer->m_GameStateBroadcast = !pPlayer->m_GameStateBroadcast;
	}
	else
	{
		SendChatTarget(ClientID, "Unknown command. Commands: !restart, !ready, !shuffle, !1on1, !settings");
		return false;
	}
	return true;
}

void CGameContext::AlertOnSpecialInstagibConfigs(int ClientID)
{
	if(g_Config.m_SvTournament)
	{
		SendChatTarget(ClientID, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		SendChatTarget(ClientID, "THERE IS A TOURNAMENT IN PROGRESS RIGHT NOW");
		SendChatTarget(ClientID, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		if(g_Config.m_SvTournamentWelcomeChat[0])
			SendChatTarget(ClientID, g_Config.m_SvTournamentWelcomeChat);
	}
	if(g_Config.m_SvOnlyHookKills)
		SendChatTarget(ClientID, "WARNING: only hooked enemies can be killed");
	if(g_Config.m_SvKillHook)
		SendChatTarget(ClientID, "WARNING: the hook kills");
}

void CGameContext::ShowCurrentInstagibConfigsMotd(int ClientID, bool Force)
{
	if(!g_Config.m_SvShowSettingsMotd && !Force)
		return;

	char aMotd[2048];
	char aBuf[512];
	aMotd[0] = '\0';

	if(g_Config.m_SvTournament)
		str_append(aMotd, "!!! ACTIVE TOURNAMENT RUNNING !!!");

	str_format(aBuf, sizeof(aBuf), "* ready mode: %s\n", g_Config.m_SvPlayerReadyMode ? "on" : "off");
	str_append(aMotd, aBuf);

	str_format(aBuf, sizeof(aBuf), "* damage needed for kill: %d\n", g_Config.m_SvDamageNeededForKill);
	str_append(aMotd, aBuf);

	str_format(aBuf, sizeof(aBuf), "* allow spec public chat: %s\n", g_Config.m_SvTournamentChat ? "no" : "yes");
	str_append(aMotd, aBuf);

	if(!str_comp_nocase(g_Config.m_SvGametype, "gctf"))
	{
		str_format(aBuf, sizeof(aBuf), "* spray protection: %s\n", g_Config.m_SvSprayprotection ? "on" : "off");
		str_append(aMotd, aBuf);
		str_format(aBuf, sizeof(aBuf), "* spam protection: %s\n", g_Config.m_SvGrenadeAmmoRegen ? "on" : "off");
		str_append(aMotd, aBuf);
		if(g_Config.m_SvGrenadeAmmoRegen)
		{
			const char *pMode = "off";
			if(g_Config.m_SvGrenadeAmmoRegenOnKill == 1)
				pMode = "one";
			else if(g_Config.m_SvGrenadeAmmoRegenOnKill == 2)
				pMode = "all";
			str_format(aBuf, sizeof(aBuf), "  - refill nades on kill: %s\n", pMode);
			str_append(aMotd, aBuf);
		}
	}
	else

		if(g_Config.m_SvOnlyHookKills)
		str_append(aMotd, "! WARNING: only hooked enemies can be killed\n");
	if(g_Config.m_SvKillHook)
		str_append(aMotd, "! WARNING: the hook kills\n");

	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = aMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::UpdateVoteCheckboxes()
{
	CVoteOptionServer *pCurrent = m_pVoteOptionFirst;

	while(pCurrent != NULL)
	{
		if(str_startswith(pCurrent->m_aDescription, "[ ]") || str_startswith(pCurrent->m_aDescription, "[x]"))
		{
			bool Checked = false;
			bool Found = true;

#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	for(int val = Min; val <= Max; val++) \
	{ \
		char aBuf[512]; \
		str_format(aBuf, sizeof(aBuf), "%s %d", #ScriptName, val); \
		if(!str_comp(pCurrent->m_aCommand, aBuf)) \
		{ \
			Checked = g_Config.m_##Name == val; \
			Found = true; \
		} \
		/* \
		votes can directly match the command or have other commands \
		or only start with it but then they should be delimited with a semicolon \
		this allows to detect config option votes that also run additonal commands on vote pass \
		*/ \
		str_format(aBuf, sizeof(aBuf), "%s %d;", #ScriptName, val); \
		if(str_startswith(pCurrent->m_aCommand, aBuf)) \
		{ \
			Checked = g_Config.m_##Name == val; \
			Found = true; \
		} \
	}
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) // only int checkboxes for now
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) // only int checkboxes for now
#include <engine/shared/variables_insta.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR

			if(Found)
				pCurrent->m_aDescription[1] = Checked ? 'x' : ' ';
		}
		pCurrent = pCurrent->m_pNext;
	}
	ShowCurrentInstagibConfigsMotd();
}

void CGameContext::RefreshVotes()
{
	// start reloading vote option list
	// clear vote options
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);

	// reset sending of vote options
	for(auto &pPlayer : m_apPlayers)
		if(pPlayer)
			pPlayer->m_SendVoteIndex = 0;
}

void CGameContext::SendBroadcastSix(const char *pText, bool Important)
{
	for(CPlayer *pPlayer : m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(Server()->IsSixup(pPlayer->GetCID()))
			continue;

		// not very nice but the best hack that comes to my mind
		// the broadcast is not rendered if the scoreboard is shown
		// the client shows the scorebaord if he is dead
		// and if there is sv_warmup(igs countdown) in the beginning of a round
		// the chracter did not spawn yet. And thus the server forces a scoreboard
		// on the client. And 0.6 clients just get stuck in that screen without
		// even noticing that a sv_warmup(igs countdown) is happenin.
		//
		// could also abuse the 0.6 warmup timer for that
		// but this causes confusion since a sv_warmup(igs countdown) is not a warmup
		// it is a countdown until the game begins and is a different thing already
		if(!pPlayer->m_HasGhostCharInGame && pPlayer->GetTeam() != TEAM_SPECTATORS)
			SendChatTarget(pPlayer->GetCID(), pText);
		SendBroadcast(pText, pPlayer->GetCID(), Important);
	}
}

void CGameContext::PlayerReadyStateBroadcast()
{
	if(!Config()->m_SvPlayerReadyMode)
		return;
	// if someone presses ready change during countdown
	// we ignore it
	if(m_pController->IsGameCountdown())
		return;

	int NumUnready = 0;
	m_pController->GetPlayersReadyState(-1, &NumUnready);

	if(!NumUnready)
		return;

	// TODO: create Sendbroadcast sixup
	//       better pr a sixup flag upstream for sendbroadcast
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "%d players not ready", NumUnready);
	SendBroadcastSix(aBuf, false);
}

void CGameContext::SendGameMsg(int GameMsgID, int ClientID)
{
	CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG, false, true);
	Msg.AddInt(GameMsgID);
	if(ClientID != -1 && Server()->IsSixup(ClientID))
	{
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
		return;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Server()->IsSixup(i))
		{
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, i);
			continue;
		}
		// TODO: 0.6
	}
}

void CGameContext::SendGameMsg(int GameMsgID, int ParaI1, int ClientID)
{
	CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG, false, true);
	Msg.AddInt(GameMsgID);
	Msg.AddInt(ParaI1);
	if(ClientID != -1 && Server()->IsSixup(ClientID))
	{
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
		return;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Server()->IsSixup(i))
		{
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, i);
			continue;
		}
		if(GameMsgID == protocol7::GAMEMSG_GAME_PAUSED)
		{
			char aBuf[512];
			int PauseID = clamp(ParaI1, 0, MAX_CLIENTS - 1);
			str_format(aBuf, sizeof(aBuf), "'%s' initiated a pause. If you are ready do /ready", Server()->ClientName(PauseID));
			SendChatTarget(i, aBuf);
		}
	}
}

void CGameContext::SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID)
{
	CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG, false, true);
	Msg.AddInt(GameMsgID);
	Msg.AddInt(ParaI1);
	Msg.AddInt(ParaI2);
	Msg.AddInt(ParaI3);
	if(ClientID != -1)
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Server()->IsSixup(i))
		{
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, i);
			continue;
		}
		// TODO: 0.6
	}
}

void CGameContext::InstagibUnstackChatMessage(char *pUnstacked, const char *pMessage, int Size)
{
	bool Match = false;
	for(const char *aLastChatMessage : m_aaLastChatMessages)
	{
		if(!str_comp(aLastChatMessage, pMessage))
		{
			Match = true;
			break;
		}
	}
	if(Match)
	{
		char aaInvisibleUnicodes[][8] = {
			"", // no unicode is also different than unicode :D
			"‌", // U+200C ZERO WIDTH NON-JOINER
			"‍", // U+200D ZERO WIDTH JOINER
			"‌", // U+200E LEFT-TO-RIGHT MARK
			"‎", // U+200F RIGHT-TO-LEFT MARK
			"⁠", // U+2060 WORD JOINER
			"⁡", // U+2061 FUNCTION APPLICATION
			"⁢", // U+2062 INVISIBLE TIMES
			"⁣", // U+2063 INVISIBLE SEPARATOR
			"⁤" // U+2064 INVISIBLE PLUS
		};
		m_UnstackHackCharacterOffset++;
		if(m_UnstackHackCharacterOffset >= (int)(sizeof(aaInvisibleUnicodes) / 8))
			m_UnstackHackCharacterOffset = 0;
		str_format(pUnstacked, Size, "%s%s", aaInvisibleUnicodes[m_UnstackHackCharacterOffset], pMessage);
	}
	for(int i = MAX_LINES - 1; i > 0; i--)
	{
		str_copy(m_aaLastChatMessages[i], m_aaLastChatMessages[i - 1]);
	}
	str_copy(m_aaLastChatMessages[0], pMessage);
}

int CGameContext::GetDDNetInstaWeapon()
{
	if(str_comp_nocase(g_Config.m_SvGametype, "gctf") == 0)
		return WEAPON_GRENADE;
	if(str_comp_nocase(g_Config.m_SvGametype, "ictf") == 0)
		return WEAPON_LASER;
	return WEAPON_GRENADE;
}

const char *str_find_digit(const char *haystack)
{
	char aaDigits[][2] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

	const char *first = NULL;
	for(const char *aDigit : aaDigits)
	{
		const char *s = str_find(haystack, aDigit);
		if(s && (!first || first > s))
			first = s;
	}
	return first;
}

bool str_contains_ip(const char *pStr)
{
	const char *s = str_find_digit(pStr);
	if(!s)
		return false;
	// dbg_msg("str_contains_ip", "s=%s", s);
	for(int byte = 0; byte < 4; byte++)
	{
		int i;
		for(i = 0; i < 3; i++)
		{
			// dbg_msg("str_contains_ip", "b=%d s=%c", byte, s[0]);
			if(!s && !s[0])
			{
				// dbg_msg("str_contains_ip", "EOL");
				return false;
			}
			if(i > 0 && s[0] == '.')
			{
				// dbg_msg("str_contains_ip", "break in byte %d at i=%d because got dot", byte, i);
				break;
			}
			if(!isdigit(s[0]))
			{
				if(i > 0)
				{
					// dbg_msg("str_contains_ip", "we got ip with less than 3 digits in the end");
					return true;
				}
				// dbg_msg("str_contains_ip", "non digit char");
				return false;
			}
			s++;
		}
		if(byte == 3 && i > 1)
		{
			// dbg_msg("str_contains_ip", "we got ip with 3 digits in the end");
			return true;
		}
		if(s[0] == '.')
			s++;
		else
		{
			// dbg_msg("str_contains_ip", "expected dot got no dot!!");
			return false;
		}
	}
	return false;
}

// int test_thing()
// {
// 	char aMsg[512];
// 	strncpy(aMsg, "join  my server 127.0.0.1 omg it best", sizeof(aMsg));

//     dbg_msg("test", "contains %d", str_contains_ip(aMsg));

// 	return 0;
// }
