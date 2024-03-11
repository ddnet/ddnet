#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "../base_instagib.h"

void CGameControllerInstagib::BangCommandVote(int ClientId, const char *pCommand, const char *pDesc)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	// not needed for bang command but for slash command
	if(pPlayer->GetTeam() == TEAM_SPECTATORS && !g_Config.m_SvSpectatorVotes)
	{
		SendChatTarget(ClientId, "Spectators aren't allowed to vote.");
		return;
	}
	char aChatmsg[1024];
	str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s'", Server()->ClientName(ClientId), pDesc);
	GameServer()->m_VoteType = GameServer()->VOTE_TYPE_OPTION;
	GameServer()->CallVote(ClientId, pDesc, pCommand, "chat cmd", aChatmsg);
}

void CGameControllerInstagib::ComCallShuffleVote(int ClientId)
{
	BangCommandVote(ClientId, "shuffle_teams", "shuffle teams");
}

void CGameControllerInstagib::ComCallSwapTeamsVote(int ClientId)
{
	BangCommandVote(ClientId, "swap_teams", "swap teams");
}

void CGameControllerInstagib::ComCallSwapTeamsRandomVote(int ClientId)
{
	BangCommandVote(ClientId, "swap_teams_random", "swap teams (random)");
}
