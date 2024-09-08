#include <base/system.h>
#include <game/server/entities/character.h>
#include <game/server/instagib/strhelpers.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_pvp.h"

bool CGameControllerPvp::AllowPublicChat(const CPlayer *pPlayer)
{
	if(!g_Config.m_SvTournamentChat)
		return true;

	if(g_Config.m_SvTournamentChat == 1 && pPlayer->GetTeam() == TEAM_SPECTATORS)
		return false;
	else if(g_Config.m_SvTournamentChat == 2)
		return false;
	return true;
}

bool CGameControllerPvp::ParseChatCmd(char Prefix, int ClientId, const char *pCmdWithArgs)
{
#define MAX_ARG_LEN 256
	char aCmd[MAX_ARG_LEN];
	int i;
	for(i = 0; pCmdWithArgs[i] && i < MAX_ARG_LEN; i++)
	{
		if(pCmdWithArgs[i] == ' ')
			break;
		aCmd[i] = pCmdWithArgs[i];
	}
	aCmd[i] = '\0';
	// int RestOffset = m_pClient->m_ChatHelper.ChatCommandGetROffset(aCmd);
	int RestOffset = -1; // TODO: add params with typed args: s,r,i

// max 8 args of 128 len each
#define MAX_ARGS 16
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
			if(NumArgs == RestOffset)
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

	bool match = OnBangCommand(ClientId, aCmd, NumArgs, (const char **)ppArgs);
	for(int x = 0; x < 8; ++x)
		delete[] ppArgs[x];
	delete[] ppArgs;
	return match;
}

bool CGameControllerPvp::OnBangCommand(int ClientId, const char *pCmd, int NumArgs, const char **ppArgs)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;

	if(!str_comp_nocase(pCmd, "set") || !str_comp_nocase(pCmd, "sett") || !str_comp_nocase(pCmd, "settings") || !str_comp_nocase(pCmd, "config"))
	{
		GameServer()->ShowCurrentInstagibConfigsMotd(ClientId, true);
		return true;
	}

	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		SendChatTarget(ClientId, "Spectators aren't allowed to vote.");
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
		BangCommandVote(ClientId, aCmd, aDesc);
	}
	else if(!str_comp_nocase(pCmd, "restart") || !str_comp_nocase(pCmd, "reload"))
	{
		int Seconds = NumArgs > 0 ? atoi(ppArgs[0]) : 10;
		Seconds = clamp(Seconds, 1, 200);
		char aCmd[512];
		str_format(aCmd, sizeof(aCmd), "restart %d", Seconds);
		char aDesc[512];
		str_format(aDesc, sizeof(aDesc), "restart %d", Seconds);
		BangCommandVote(ClientId, aCmd, aDesc);
	}
	else if(!str_comp_nocase(pCmd, "ready") || !str_comp_nocase(pCmd, "pause"))
	{
		GameServer()->m_pController->OnPlayerReadyChange(pPlayer);
	}
	else if(!str_comp_nocase(pCmd, "shuffle"))
	{
		ComCallShuffleVote(ClientId);
	}
	else if(!str_comp_nocase(pCmd, "swap"))
	{
		ComCallSwapTeamsVote(ClientId);
	}
	else if(!str_comp_nocase(pCmd, "swap_random"))
	{
		ComCallSwapTeamsRandomVote(ClientId);
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
				SendChatTarget(ClientId, "usage: !gamestate [on|off]");
		}
		else
			pPlayer->m_GameStateBroadcast = !pPlayer->m_GameStateBroadcast;
	}
	else
	{
		SendChatTarget(ClientId, "Unknown command. Commands: !restart, !ready, !shuffle, !1on1, !settings");
		return false;
	}
	return true;
}

bool CGameControllerPvp::OnChatMessage(const CNetMsg_Cl_Say *pMsg, int Length, int &Team, CPlayer *pPlayer)
{
	int ClientId = pPlayer->GetCid();

	if(pMsg->m_Team || !AllowPublicChat(pPlayer))
		Team = ((pPlayer->GetTeam() == TEAM_SPECTATORS) ? TEAM_SPECTATORS : pPlayer->GetTeam()); // ddnet-insta
	else
		Team = TEAM_ALL;

	// ddnet-insta warn on ping if cant respond
	if(Team == TEAM_ALL && pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		for(const CPlayer *pSpecPlayer : GameServer()->m_apPlayers)
		{
			if(!pSpecPlayer)
				continue;
			if(pSpecPlayer->GetTeam() != TEAM_SPECTATORS)
				continue;
			if(AllowPublicChat(pSpecPlayer))
				continue;
			if(!str_find_nocase(pMsg->m_pMessage, Server()->ClientName(pSpecPlayer->GetCid())))
				continue;

			char aChatText[256];
			str_format(aChatText, sizeof(aChatText), "Warning: '%s' got pinged in chat but can not respond", Server()->ClientName(pSpecPlayer->GetCid()));
			GameServer()->SendChat(-1, TEAM_ALL, aChatText);
			GameServer()->SendChat(-1, TEAM_ALL, "turn off tournament chat or make sure there are enough in game slots");
			break;
		}
	}

	// ddnet-insta fine grained chat spam control
	if(pMsg->m_pMessage[0] != '/')
	{
		bool RateLimit = false;
		if(g_Config.m_SvChatRatelimitSpectators && pPlayer->GetTeam() == TEAM_SPECTATORS)
		{
			if(g_Config.m_SvChatRatelimitDebug)
				dbg_msg("ratelimit", "m_SvChatRatelimitSpectators %s", pMsg->m_pMessage);
			RateLimit = true;
		}
		if(g_Config.m_SvChatRatelimitPublicChat && Team == TEAM_ALL)
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

	// ddnet-insta bang commands
	// allow sending ! to chat or !!
	// swallow all other ! prefixed chat messages
	if(pMsg->m_pMessage[0] == '!' && pMsg->m_pMessage[1] && pMsg->m_pMessage[1] != '!')
	{
		ParseChatCmd('!', ClientId, pMsg->m_pMessage + 1);
		return true;
	}

	if(pMsg->m_pMessage[0] == '/')
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%d used %s", ClientId, pMsg->m_pMessage);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-command", aBuf);

		if(!str_comp_nocase(pMsg->m_pMessage + 1, "ready") || !str_comp_nocase(pMsg->m_pMessage + 1, "pause")) // ddnet-insta
		{
			GameServer()->m_pController->OnPlayerReadyChange(pPlayer);
			return true;
		}
		else if(!str_comp_nocase(pMsg->m_pMessage + 1, "shuffle")) // ddnet-insta
		{
			ComCallShuffleVote(ClientId);
			return true;
		}
		else if(!str_comp_nocase(pMsg->m_pMessage + 1, "swap")) // ddnet-insta
		{
			ComCallSwapTeamsVote(ClientId);
			return true;
		}
		else if(!str_comp_nocase(pMsg->m_pMessage + 1, "swap_random")) // ddnet-insta
		{
			ComCallSwapTeamsRandomVote(ClientId);
			return true;
		}
		else if(!str_comp_nocase(pMsg->m_pMessage + 1, "drop flag")) // ddnet-insta
		{
			ComDropFlag(ClientId);
			return true;
		}
		else if(str_startswith(pMsg->m_pMessage + 1, "drop")) // ddnet-insta
		{
			SendChatTarget(ClientId, "Did you mean '/drop flag'?");
			return true;
		}
	}
	return false;
}
