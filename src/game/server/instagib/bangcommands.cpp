#include <engine/shared/config.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamemodes/DDRace.h"
#include "../gamemodes/gctf.h"
#include "../gamemodes/ictf.h"
#include "../gamemodes/mod.h"
#include "../player.h"

#include "strhelpers.h"

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