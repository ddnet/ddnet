#include <base/system.h>
#include <engine/shared/config.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../player.h"

#include <game/server/gamecontroller.h>

// TODO: this is not game logic
//       do some cleanup of the file structuring
//       rcon_commands.cpp rcon_configs.cpp and gamelogic.cpp
//       all implement CGameContext mehtods
void CGameContext::OnInitInstagib()
{
	UpdateVoteCheckboxes(); // ddnet-insta
	AlertOnSpecialInstagibConfigs(); // ddnet-insta
	ShowCurrentInstagibConfigsMotd(); // ddnet-insta

	m_pHttp = Kernel()->RequestInterface<IHttp>();
}

void CGameContext::AlertOnSpecialInstagibConfigs(int ClientId) const
{
	if(g_Config.m_SvTournament)
	{
		SendChatTarget(ClientId, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		SendChatTarget(ClientId, "THERE IS A TOURNAMENT IN PROGRESS RIGHT NOW");
		SendChatTarget(ClientId, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		if(g_Config.m_SvTournamentWelcomeChat[0])
			SendChatTarget(ClientId, g_Config.m_SvTournamentWelcomeChat);
	}
	if(g_Config.m_SvOnlyHookKills)
		SendChatTarget(ClientId, "WARNING: only hooked enemies can be killed");
	if(g_Config.m_SvKillHook)
		SendChatTarget(ClientId, "WARNING: the hook kills");
}

void CGameContext::ShowCurrentInstagibConfigsMotd(int ClientId, bool Force) const
{
	if(!g_Config.m_SvShowSettingsMotd && !Force)
		return;

	char aMotd[2048];
	char aBuf[512];
	aMotd[0] = '\0';

	if(g_Config.m_SvTournament)
		str_append(aMotd, "!!! ACTIVE TOURNAMENT RUNNING !!!");

	if(g_Config.m_SvPlayerReadyMode)
	{
		str_append(aMotd, "* ready mode: on\n");
		str_format(aBuf, sizeof(aBuf), "  - max pause time: %d\n", g_Config.m_SvForceReadyAll);
		str_append(aMotd, aBuf);
	}
	else
		str_append(aMotd, "* ready mode: off\n");

	if(g_Config.m_SvDropFlagOnVote || g_Config.m_SvDropFlagOnSelfkill)
	{
		str_append(aMotd, "* dropping the flag is on '/drop flag'\n");
		if(g_Config.m_SvDropFlagOnSelfkill)
			str_append(aMotd, "  - selfkill drops the flag\n");
		if(g_Config.m_SvDropFlagOnVote)
			str_append(aMotd, "  - vote yes drops the flag\n");
	}

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

	if(g_Config.m_SvAllowZoom)
		str_append(aMotd, "! WARNING: using zoom is allowed\n");
	if(g_Config.m_SvOnlyHookKills)
		str_append(aMotd, "! WARNING: only hooked enemies can be killed\n");
	if(g_Config.m_SvKillHook)
		str_append(aMotd, "! WARNING: the hook kills\n");

	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = aMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::UpdateVoteCheckboxes() const
{
	if(!g_Config.m_SvVoteCheckboxes)
		return;

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
		if(Server()->IsSixup(pPlayer->GetCid()))
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
		//
		// this hack can be put behind a ddnet client version check if
		// https://github.com/ddnet/ddnet/pull/8892
		// is merged. If it is merged for long enough the entire ghost char hack can be removed
		// and we just drop broadcast support for old clients in that weird state
		if(!pPlayer->m_HasGhostCharInGame && pPlayer->GetTeam() != TEAM_SPECTATORS)
			SendChatTarget(pPlayer->GetCid(), pText);
		SendBroadcast(pText, pPlayer->GetCid(), Important);
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

void CGameContext::SendGameMsg(int GameMsgId, int ClientId) const
{
	CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG, false, true);
	Msg.AddInt(GameMsgId);
	if(ClientId != -1 && Server()->IsSixup(ClientId))
	{
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
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

void CGameContext::SendGameMsg(int GameMsgId, int ParaI1, int ClientId) const
{
	CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG, false, true);
	Msg.AddInt(GameMsgId);
	Msg.AddInt(ParaI1);
	if(ClientId != -1 && Server()->IsSixup(ClientId))
	{
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
		return;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Server()->IsSixup(i))
		{
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, i);
			continue;
		}
		if(GameMsgId == protocol7::GAMEMSG_GAME_PAUSED)
		{
			char aBuf[512];
			int PauseId = clamp(ParaI1, 0, MAX_CLIENTS - 1);
			str_format(aBuf, sizeof(aBuf), "'%s' initiated a pause. If you are ready do /ready", Server()->ClientName(PauseId));
			SendChatTarget(i, aBuf);
		}
	}
}

void CGameContext::SendGameMsg(int GameMsgId, int ParaI1, int ParaI2, int ParaI3, int ClientId) const
{
	CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG, false, true);
	Msg.AddInt(GameMsgId);
	Msg.AddInt(ParaI1);
	Msg.AddInt(ParaI2);
	Msg.AddInt(ParaI3);
	if(ClientId != -1)
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
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
