#include "votemenu.h"
#include "../gamecontext.h"
#include "../player.h"
#include "accounts.h"
#include <algorithm>
#include <base/system.h>
#include <ctime>
#include <engine/message.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/voting.h>
#include <iterator>
#include <string>
#include <vector>

// Font: https://fsymbols.com/generators/smallcaps/
constexpr const char *EMPTY_DESC = " ";
constexpr const char *FANCY_LINES_DESC = "═─═─═─═─═─═─═─═─═─═─═";
constexpr const char *SETTINGS_AUTO_LOGIN = "Auto Login";

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

bool CVoteMenu::OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId)
{
	if(str_comp_nocase(pMsg->m_pType, "option") != 0)
		return false;

	const int Page = m_aClientData[ClientId].m_Page;
	const char *Vote = str_skip_voting_menu_prefixes(pMsg->m_pValue);
	const char *pDescription = pMsg->m_pReason;

	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];

	if(!Vote || !pDescription || !pPl)
		return false;

	if(Page < 0 || Page >= NUM_PAGES)
		return false;

	CAccountSession &Acc = GameServer()->m_Account[ClientId];

	for(int i = 0; i < Pages::NUM_PAGES; i++)
	{
		if(!str_comp(Vote, m_aPages[i]))
		{
			SetPage(ClientId, i);
			return true;
		}
	}

	if(IsPageAllowed(ClientId, Page))
	{
		if(Page == Pages::SETTINGS)
		{
			if(Acc.m_LoggedIn && !str_comp(Vote, SETTINGS_AUTO_LOGIN))
			{
				if(Acc.m_Flags & ACC_FLAG_AUTOLOGIN)
				{
					Acc.m_Flags &= ~ACC_FLAG_AUTOLOGIN;
					GameServer()->SendChatTarget(ClientId, "Auto Login has been disabled");
				}
				else
				{
					Acc.m_Flags |= ACC_FLAG_AUTOLOGIN;
					GameServer()->SendChatTarget(ClientId, "Auto Login has been enabled");
				}
				return true;
			}
		}
		else if(Page == Pages::ACCOUNT)
		{
		}
		else if(Page == Pages::SHOP)
		{
		}
		else if(Page == Pages::INVENTORY)
		{
		}
		else if(Page == Pages::ADMIN)
		{
		}
	}

	if(Page != Pages::VOTES)
		return true;
	return false;
}

void CVoteMenu::Tick()
{
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			return;
		if(!GameServer()->m_apPlayers[ClientId] || Server()->ClientSlotEmpty(ClientId))
			continue;

		if(GameServer()->m_apPlayers[ClientId]->m_PlayerFlags & PLAYERFLAG_IN_MENU)
			UpdatePages(ClientId);
	}
}

void CVoteMenu::OnClientDrop(int ClientId)
{
	SetPage(ClientId, Pages::VOTES);
}

void CVoteMenu::UpdatePages(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const int Page = GetPage(ClientId);

	bool Changes = false;

	if(!IsPageAllowed(ClientId, Page))
	{
		if(Page != Pages::VOTES)
			SetPage(ClientId, Pages::VOTES);
		return;
	}
	CAccountSession Acc = GameServer()->m_Account[ClientId];

	if(Acc.m_LoggedIn != m_aClientData[ClientId].m_Account.m_LoggedIn) // Check login status change
		Changes = true;

	if(Page == Pages::VOTES)
		return;

	if(Page == Pages::SETTINGS)
	{
		if(Acc.m_Flags != m_aClientData[ClientId].m_Account.m_Flags)
			Changes = true;
	}
	else if(Page == Pages::ACCOUNT)
	{
		if(mem_comp(&Acc, &m_aClientData[ClientId].m_Account, sizeof(Acc)) != 0)
			Changes = true;
	}

	if(Changes)
	{
		m_aClientData[ClientId].m_Account = GameServer()->m_Account[ClientId];
		PrepareVoteOptions(ClientId, Page);
	}
}

bool CVoteMenu::IsPageAllowed(int ClientId, int Page) const
{
	if(Page < 0 || Page >= NUM_PAGES)
		return true;

	const CAccountSession Acc = GameServer()->m_Account[ClientId];

	if(IsFlagSet(g_Config.m_SvVoteMenuFlags, Page) && Page != Pages::ADMIN)
		return false;
	if(Page == Pages::ADMIN && Server()->GetAuthedState(ClientId) < AUTHED_ADMIN)
		return false;
	if(!Acc.m_LoggedIn && (Page == Pages::SHOP || Page == Pages::INVENTORY))
		return false;
	return true;
}

void CVoteMenu::PrepareVoteOptions(int ClientId, int Page)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(Page < 0 || Page >= NUM_PAGES)
		return;

	GameServer()->ClearVotes(ClientId);
	m_vDescriptions.clear();

	switch(Page)
	{
	case Pages::VOTES: GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex = 0; return;
	case Pages::SETTINGS: SendPageSettings(ClientId); break;
	case Pages::ACCOUNT: SendPageAccount(ClientId); break;
	case Pages::SHOP: SendPageShop(ClientId); break;
	case Pages::INVENTORY: SendPageInventory(ClientId); break;
	case Pages::ADMIN: SendPageAdmin(ClientId); break;
	}

	const int NumVotesToSend = m_vDescriptions.size();
	int TotalVotesSent = 0;

	CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
	while(TotalVotesSent < NumVotesToSend)
	{
		const int VotesLeft = std::min(NumVotesToSend - TotalVotesSent, g_Config.m_SvSendVotesPerTick);
		Msg.AddInt(VotesLeft);

		int CurIndex = 0;

		while(CurIndex < VotesLeft)
		{
			Msg.AddString(m_vDescriptions.at(TotalVotesSent).c_str(), VOTE_DESC_LENGTH);

			CurIndex++;
			TotalVotesSent++;
		}

		if(!Server()->IsSixup(ClientId))
		{
			while(CurIndex < 15)
			{
				Msg.AddString("", VOTE_DESC_LENGTH);
				CurIndex++;
			}
		}
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
		Msg.Reset();
	}
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
	CAccountSession &Acc = GameServer()->m_Account[ClientId];
	if(Acc.m_LoggedIn)
		Acc.m_VoteMenuPage = Page;

	PrepareVoteOptions(ClientId, Page);
}

void CVoteMenu::SendPageSettings(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const CAccountSession Acc = GameServer()->m_Account[ClientId];

	if(Acc.m_LoggedIn)
		AddDescriptionCheckBox(SETTINGS_AUTO_LOGIN, Acc.m_Flags & ACC_FLAG_AUTOLOGIN);
}

void CVoteMenu::SendPageAccount(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const CAccountSession Acc = GameServer()->m_Account[ClientId];

	if(!Acc.m_LoggedIn)
	{
		AddDescription("You are not logged in.");
		AddDescription(EMPTY_DESC);
		AddDescription("1 - use /register <Name> <Password> <Password>");
		AddDescription("2 - login using /login <Name> <Password>");
		return;
	}

	char aBuf[VOTE_DESC_LENGTH];

	AddDescription("╭─────────       Pʀᴏғɪʟᴇ");
	str_format(aBuf, sizeof(aBuf), "│ Account Name: %s", Acc.m_Username);
	AddDescription(aBuf);
	str_format(aBuf, sizeof(aBuf), "│ Last Player Name: %s", Acc.m_LastName);
	AddDescription(aBuf);

	char TimeBuf[64];

	// Register Date
	if(FormatUnixTime(Acc.m_RegisterDate, TimeBuf, sizeof(TimeBuf), "%Y-%m-%d %H:%M:%S"))
	{
		str_format(aBuf, sizeof(aBuf), "│ Register Date: %s", TimeBuf);
		AddDescription(aBuf);
	}
	else
	{
		AddDescription("│ Register Date: n/a");
	}

	AddDescription("├──────      Sᴛᴀᴛs");

	str_format(aBuf, sizeof(aBuf), "│ Level [%d]", Acc.m_Level);
	AddDescription(aBuf);

	int CurXp = Acc.m_XP;
	int ClampedLevel = std::clamp((int)Acc.m_Level, 0, 4);
	int NeededXp = GameServer()->m_AccountManager.m_NeededXp[ClampedLevel];

	str_format(aBuf, sizeof(aBuf), "│ XP [%d/%d]", CurXp, NeededXp);
	AddDescription(aBuf);

	float PlayTimeHours = Acc.m_Playtime / 60.0f;
	str_format(aBuf, sizeof(aBuf), "│ Playtime: %.1f Hour%s", PlayTimeHours, PlayTimeHours == 1 ? "" : "s");
	if(Acc.m_Playtime < 100)
		str_format(aBuf, sizeof(aBuf), "│ Playtime: %lld Minute%s", (int)Acc.m_Playtime, Acc.m_Playtime == 1 ? "" : "s");
	AddDescription(aBuf);

	str_format(aBuf, sizeof(aBuf), "│ %s: %lld", g_Config.m_SvCurrencyName, Acc.m_Money);
	AddDescription(aBuf);

	str_format(aBuf, sizeof(aBuf), "│ Deaths: %d", Acc.m_Deaths);
	AddDescription(aBuf);

	AddDescription("╰──────────────────────────");
}

void CVoteMenu::SendPageShop(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	AddDescription("ToDo");
}

void CVoteMenu::SendPageInventory(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	AddDescription("ToDo");
}

void CVoteMenu::SendPageAdmin(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	AddDescription("ToDo");
}

void CVoteMenu::AddHeader(int ClientId)
{
	const int NumVotesToSend = Pages::NUM_PAGES + 3;

	std::vector<std::string> Descriptions;

	int CurIndex = 0;
	for(int i = 0; i < NumVotesToSend; i++)
	{
		if(!IsPageAllowed(ClientId, i))
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

void CVoteMenu::AddDescriptionPrefix(const char *pDesc, int Prefix)
{
	const char *pPrefixes[] = {"•", "─", ">", "⇨", "⁃", "‣", "◆", "◇"};

	if(Prefix < 0 || Prefix >= (int)std::size(pPrefixes))
	{
		AddDescription(pDesc);
		return;
	}
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %s", pPrefixes[Prefix], pDesc);
	AddDescription(aBuf);
}

void CVoteMenu::AddDescriptionCheckBox(const char *pDesc, bool Checked)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %s", Checked ? "☒" : "☐", pDesc);
	AddDescription(aBuf);
}