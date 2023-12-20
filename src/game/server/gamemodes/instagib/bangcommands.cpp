#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "../base_instagib.h"

void CGameControllerInstagib::BangCommandVote(int ClientID, const char *pCommand, const char *pDesc)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if(!pPlayer)
		return;
	// not needed for bang command but for slash command
	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		SendChatTarget(ClientID, "Spectators aren't allowed to vote.");
		return;
	}
	char aChatmsg[1024];
	str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s'", Server()->ClientName(ClientID), pDesc);
	GameServer()->CallVote(ClientID, pDesc, pCommand, "chat cmd", aChatmsg);
}

void CGameControllerInstagib::ComCallShuffleVote(int ClientID)
{
	BangCommandVote(ClientID, "shuffle_teams", "shuffle teams");
}

void CGameControllerInstagib::ComCallSwapTeamsVote(int ClientID)
{
	BangCommandVote(ClientID, "swap_teams", "swap teams");
}

void CGameControllerInstagib::ComCallSwapTeamsRandomVote(int ClientID)
{
	BangCommandVote(ClientID, "swap_teams_random", "swap teams (random)");
}
