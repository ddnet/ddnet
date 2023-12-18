#include <engine/shared/config.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamemodes/DDRace.h"
#include "../gamemodes/gctf.h"
#include "../gamemodes/ictf.h"
#include "../gamemodes/mod.h"
#include "../player.h"

#include "strhelpers.h"

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
