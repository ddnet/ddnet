#include "votemenu.h"
#include "../gamecontext.h"
#include "../player.h"
#include <base/system.h>
#include <engine/message.h>
#include <engine/server.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/voting.h>
#include <engine/shared/config.h>
#include <vector>
#include <string>

// Font: https://fsymbols.com/generators/smallcaps/
constexpr const char *EMPTY_DESC = " ";
constexpr const char *FANCY_LINES_DESC = "═─═─═─═─═─═─═─═─═─═─═";

IServer *CVoteMenu::Server() const { return GameServer()->Server(); }

void CVoteMenu::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	str_copy(m_aPages[Pages::VOTES], "Vᴏᴛᴇs");
	str_copy(m_aPages[Pages::SETTINGS], "Sᴇᴛᴛɪɴɢs");
	str_copy(m_aPages[Pages::ACCOUNT], "Aᴄᴄᴏᴜɴᴛ");
	str_copy(m_aPages[Pages::SHOP], "Sʜᴏᴘ");
	str_copy(m_aPages[Pages::INVENTORY], "Iɴᴠᴇɴᴛᴏʀʏ");
	str_copy(m_aPages[Pages::ADMIN], "Aᴅᴍɪɴ");
}

int CVoteMenu::GetPage(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return Pages::VOTES;
	return m_aClientData[ClientId].m_Page;
}

void CVoteMenu::SetPage(int ClientId, int Page)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(Page < 0 || Page >= Pages::NUM_PAGES)
		return;

	m_aClientData[ClientId].m_Page = Page;
	GameServer()->ClearVotes(ClientId);

	if(Page == Pages::VOTES)
	{
		GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex = 0;
		return;
	}
}

void CVoteMenu::AddHeader(int ClientId)
{
	int NumVotesToSend = Pages::NUM_PAGES + 3;

	std::vector<std::string> Descriptions;

	int CurIndex = 0;
	for(int i = 0; i < NumVotesToSend; i++)
	{
		if(i == Pages::ADMIN && Server()->GetAuthedState(ClientId) < AUTHED_ADMIN)
			continue;
		if(IsFlagSet(g_Config.m_SvVoteMenuFlags, i))
			continue;

		if(i == Pages::NUM_PAGES)
			Descriptions.emplace_back(EMPTY_DESC);
		else if(i == Pages::NUM_PAGES + 1)
			Descriptions.emplace_back(FANCY_LINES_DESC);
		else if(i == Pages::NUM_PAGES + 2)
			Descriptions.emplace_back(EMPTY_DESC);
		else
		{
			char pDesc[64];
			str_format(pDesc, sizeof(pDesc), "%s %s", i == GetPage(ClientId) ? "☒" : "☐", m_aPages[i]);
			Descriptions.emplace_back(pDesc); 
		}

		CurIndex++;
	}

	CMsgPacker OptionMsg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
	OptionMsg.AddInt(CurIndex);
	for(const auto &pDesc : Descriptions)
		OptionMsg.AddString(pDesc.c_str(), VOTE_DESC_LENGTH);

	if(!Server()->IsSixup(ClientId))
	{
		while(CurIndex < 15)
		{
			OptionMsg.AddString("", VOTE_DESC_LENGTH);
			CurIndex++;
		}
	}

	Server()->SendMsg(&OptionMsg, MSGFLAG_VITAL, ClientId);
	GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex = 0;
}

bool CVoteMenu::OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId)
{
	if(str_comp_nocase(pMsg->m_pType, "option") != 0)
		return false;

	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	
	const int Page = m_aClientData[ClientId].m_Page;
	const char *Vote = str_skip_voting_menu_prefixes(pMsg->m_pValue);
	const char *pDescription = pMsg->m_pReason;

	for(int i = 0; i < Pages::NUM_PAGES; i++)
	{
		if(!str_comp_nocase(Vote, m_aPages[i]))
		{
			SetPage(ClientId, i);
			return true;
		}
	}

	if(Page != Pages::VOTES)
		return true;
	return false;
}