#include <engine/shared/config.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamemodes/DDRace.h"
#include "../gamemodes/gctf.h"
#include "../gamemodes/ictf.h"
#include "../gamemodes/mod.h"
#include "../player.h"

#include "strhelpers.h"

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