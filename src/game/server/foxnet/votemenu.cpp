#include "votemenu.h"
#include "accounts.h"
#include <algorithm>
#include <base/system.h>
#include <cstring>
#include <engine/message.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/voting.h>
#include <iterator>
#include <string>
#include <vector>
#include <game/gamecore.h>
#include <optional>
#include <engine/console.h>
#include "shop.h"

// Font: https://fsymbols.com/generators/smallcaps/
constexpr const char *EMPTY_DESC = " ";
constexpr const char *FANCY_LINES_DESC = "═─═─═─═─═─═─═─═─═─═─═";

constexpr const char *SETTINGS_AUTO_LOGIN = "Auto Login";
constexpr const char *SETTINGS_HIDE_COSMETICS = "Hide Cosmetics";
constexpr const char *SETTINGS_HIDE_POWERUPS = "Hide PowerUps";

// Admin SubPages
constexpr const char *ADMIN_UTIL = "Util Page";
constexpr const char *ADMIN_COSMETICS = "Cosmetics Page";
constexpr const char *ADMIN_MISC = "Misc Page";

// Admin Util
constexpr const char *ADMIN_UTIL_VANISH = "Vanish";

constexpr const char *ADMIN_UTIL_INVINCIBLE = "Invincible";
constexpr const char *ADMIN_UTIL_SPIDERHOOK = "Spider Hook";
constexpr const char *ADMIN_UTIL_PASSIVE = "Passive";

constexpr const char *ADMIN_UTIL_TELEKINESIS = "Telekinesis";
constexpr const char *ADMIN_UTIL_TELEK_IMMUNITY = "Telekinesis Immunity";

constexpr const char *ADMIN_UTIL_COLLIDABLE = "Collidable";
constexpr const char *ADMIN_UTIL_HITTABLE = "Hittable";
constexpr const char *ADMIN_UTIL_HOOKABLE = "Hookable";

// Admin Misc
constexpr const char *ADMIN_MISC_SNAKE = "Snake";
constexpr const char *ADMIN_MISC_UFO = "Ufo";

constexpr const char *ADMIN_MISC_OBFUSCATED = "Obfuscate Name";
constexpr const char *ADMIN_MISC_IGN_KILL_BORD = "Ignore Kill Border";

constexpr const char *ADMIN_MISC_HEARTGUN = "Heart Gun";
constexpr const char *ADMIN_MISC_LIGHTSABER = "Lightsaber";
constexpr const char *ADMIN_MISC_PORTALGUN = "Portal gun";

// Admin Extra Cosmetics
constexpr const char *ADMIN_COSM_PICKUPPET = "Pickup Pet";
constexpr const char *ADMIN_COSM_STAFFIND = "Staff Indicator";
constexpr const char *ADMIN_COSM_HEARTGUN = "Heart Gun";

// Part of Cosmetics ig /shrug
constexpr const char *ADMIN_ABILITY_HEART = "Heart Ability";
constexpr const char *ADMIN_ABILITY_SHIELD = "Shield Ability";
constexpr const char *ADMIN_ABILITY_FIREWORK = "Firework Ability";
constexpr const char *ADMIN_ABILITY_TELEKINESIS = "Telekinesis Ability";

IServer *CVoteMenu::Server() const { return GameServer()->Server(); }

void CVoteMenu::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	str_copy(m_aPages[PAGE_VOTES], "Vᴏᴛᴇs");
	str_copy(m_aPages[PAGE_SETTINGS], "Sᴇᴛᴛɪɴɢs");
	str_copy(m_aPages[PAGE_ACCOUNT], "Aᴄᴄᴏᴜɴᴛ");
	str_copy(m_aPages[PAGE_SHOP], "Sʜᴏᴘ");
	str_copy(m_aPages[PAGE_INVENTORY], "Iɴᴠᴇɴᴛᴏʀʏ");
	str_copy(m_aPages[PAGE_ADMIN], "Aᴅᴍɪɴ");
}

bool CVoteMenu::OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId)
{
	if(str_comp_nocase(pMsg->m_pType, "option") != 0)
		return false;

	const char *pVote = str_skip_voting_menu_prefixes(pMsg->m_pValue);
	const int Page = m_aClientData[ClientId].m_Page;

	if(!pVote)
		return false;

	for(int i = 0; i < NUM_PAGES; i++)
	{
		if(IsOption(pVote, m_aPages[i]))
		{
			SetPage(ClientId, i);
			return true;
		}
	}

	if(IsOption(pVote, FANCY_LINES_DESC) || IsOption(pVote, EMPTY_DESC))
		return true;

	if(IsCustomVoteOption(pMsg, ClientId))
	{
		PrepareVoteOptions(ClientId, Page);
		return true;
	}
	return false;
}

const char *CVoteMenu::FormatItemVote(CItems *pItem, const CAccountSession *pAcc)
{
	static char aBuf[128];
	char levelBuf[32] = "";

	if(pItem->MinLevel() > 0)
		str_format(levelBuf, sizeof(levelBuf), "Min lvl %d |", pItem->MinLevel());
	str_format(aBuf, sizeof(aBuf), "%s  %s  | %d %s", levelBuf, pItem->Name(), pItem->Price(), g_Config.m_SvCurrencyName);

	return aBuf;
}

bool CVoteMenu::IsCustomVoteOption(const CNetMsg_Cl_CallVote *pMsg, int ClientId)
{
	const int Page = m_aClientData[ClientId].m_Page;
	const char *pVote = str_skip_voting_menu_prefixes(pMsg->m_pValue);
	const char *pReason = pMsg->m_pReason;

	std::optional<int> ReasonInt = std::nullopt;
	if(pReason[0] && str_isallnum(pReason))
		ReasonInt = str_toint(pReason);

	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	CCharacter *pChr = pPl->GetCharacter();

	if(!pVote || !pPl)
		return false;

	if(Page < 0 || Page >= NUM_PAGES)
		return false;

	CAccountSession &Acc = GameServer()->m_aAccounts[ClientId];

	if(!IsPageAllowed(ClientId, Page) && Page != PAGE_VOTES)
	{
		return false;
	}

	if(Page == PAGE_SETTINGS)
	{
		if(Acc.m_LoggedIn && IsOption(pVote, SETTINGS_AUTO_LOGIN))
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
		if(IsOption(pVote, SETTINGS_HIDE_COSMETICS))
		{
			pPl->SetHideCosmetics(!pPl->m_HideCosmetics);
			return true;
		}
		if(IsOption(pVote, SETTINGS_HIDE_POWERUPS))
		{
			pPl->SetHidePowerUps(!pPl->m_HidePowerUps);
			return true;
		}
	}
	else if(Page == PAGE_ACCOUNT)
	{
	}
	else if(Page == PAGE_SHOP)
	{
		for(const auto &pItems : GameServer()->m_Shop.m_Items)
		{
			if(IsOption(pItems->Name(), ""))
				continue;
			if(pItems->Price() == -1)
				continue;
			const char *pVoteName = FormatItemVote(pItems, &Acc);

			if(IsOption(pVote, pVoteName))
			{
				GameServer()->m_Shop.BuyItem(ClientId, pItems->Name());
				return true;
			}
		}
	}
	else if(Page == PAGE_INVENTORY || Page == PAGE_ADMIN)
	{
		if(pPl->m_HideCosmetics && Page == PAGE_INVENTORY)
		{
			GameServer()->SendChatTarget(ClientId, "Turn on Cosmetics to enable them");
			SetPage(ClientId, PAGE_SETTINGS);
			return true;
		}

		// !New Cosmetic
		if(IsOptionWithSuffix(pVote, "Rainbow Speed"))
		{
			if(ReasonInt.has_value())
				pPl->m_Cosmetics.m_RainbowSpeed = ReasonInt.value();
			else
				GameServer()->SendChatTarget(ClientId, "Please specify the rainbow speed using the reason field");
			return true;
		}
		if(IsOptionWithSuffix(pVote, "Emoticon Gun"))
		{
			if(ReasonInt.has_value())
				pPl->ToggleItem("Emoticon Gun", ReasonInt.value(), Page == PAGE_ADMIN);
			else
				GameServer()->SendChatTarget(ClientId, "Please specify the emote type using the reason field");
			return true;
		}

		// Options that use the reason field go above
		for(const auto &pItems : GameServer()->m_Shop.m_Items)
		{
			if(IsOption(pItems->Name(), ""))
				continue;
			if(pItems->Price() == -1)
				continue;

			if(IsOption(pVote, pItems->Name()))
			{
				pPl->ToggleItem(pItems->Name(), -1, Page == PAGE_ADMIN);
				return true;
			}
		}
	}
	if(Page == PAGE_ADMIN)
	{
		if(IsOption(pVote, ADMIN_UTIL))
		{
			SetSubPage(ClientId, SUB_ADMIN_UTIL);
			return true;
		}
		if(IsOption(pVote, ADMIN_MISC))
		{
			SetSubPage(ClientId, SUB_ADMIN_MISC);
			return true;
		}
		if(IsOption(pVote, ADMIN_COSMETICS))
		{
			SetSubPage(ClientId, SUB_ADMIN_COSMETICS);
			return true;
		}

		if(GetSubPage(ClientId) == SUB_ADMIN_UTIL)
		{
			if(IsOption(pVote, ADMIN_UTIL_INVINCIBLE))
			{
				if(pChr)
					pChr->SetInvincible(!pChr->Core()->m_Invincible);
				return true;
			}
			if(IsOption(pVote, ADMIN_UTIL_VANISH))
			{
				pPl->m_Vanish = !pPl->m_Vanish;
				return true;
			}
			if(IsOption(pVote, ADMIN_UTIL_SPIDERHOOK))
			{
				pPl->m_SpiderHook = !pPl->m_SpiderHook;
				return true;
			}
			if(IsOption(pVote, ADMIN_UTIL_PASSIVE))
			{
				if(pChr)
					pChr->SetPassive(!pChr->Core()->m_Passive);
				return true;
			}

			if(IsOption(pVote, ADMIN_UTIL_TELEKINESIS))
			{
				if(pChr)
					pChr->GiveWeapon(WEAPON_TELEKINESIS, pChr->GetWeaponGot(WEAPON_TELEKINESIS));
				return true;
			}
			if(IsOption(pVote, ADMIN_UTIL_TELEK_IMMUNITY))
			{
				pPl->SetTelekinesisImmunity(!pPl->m_TelekinesisImmunity);
				return true;
			}

			if(IsOption(pVote, ADMIN_UTIL_COLLIDABLE))
			{
				if(pChr)
					pChr->SetCollidable(!pChr->Core()->m_Collidable);
				return true;
			}
			if(IsOption(pVote, ADMIN_UTIL_HITTABLE))
			{
				if(pChr)
					pChr->SetHittable(!pChr->Core()->m_Hittable);
				return true;
			}
			if(IsOption(pVote, ADMIN_UTIL_HOOKABLE))
			{
				if(pChr)
					pChr->SetHookable(!pChr->Core()->m_Hookable);
				return true;
			}
		}

		if(GetSubPage(ClientId) == SUB_ADMIN_MISC)
		{
			if(IsOption(pVote, ADMIN_MISC_SNAKE) && pChr)
			{
				pChr->SetSnake(!pChr->m_Snake.Active());
				return true;
			}

			if(IsOption(pVote, ADMIN_MISC_UFO) && pChr)
			{
				pChr->SetUfo(!pChr->m_Ufo.Active());
				return true;
			}

			if(IsOption(pVote, ADMIN_MISC_IGN_KILL_BORD))
			{
				pPl->m_IgnoreGamelayer = !pPl->m_IgnoreGamelayer;
				return true;
			}
			if(IsOption(pVote, ADMIN_MISC_OBFUSCATED))
			{
				pPl->SetObfuscated(!pPl->m_Obfuscated);
				return true;
			}
			if(IsOption(pVote, ADMIN_MISC_HEARTGUN))
			{
				if(pChr)
				{
					pChr->GiveWeapon(WEAPON_HEARTGUN, pChr->GetWeaponGot(WEAPON_HEARTGUN));
					pChr->SetActiveWeapon(WEAPON_HEARTGUN);
				}
				return true;
			}
			if(IsOption(pVote, ADMIN_MISC_LIGHTSABER))
			{
				if(pChr)
				{
					pChr->GiveWeapon(WEAPON_LIGHTSABER, pChr->GetWeaponGot(WEAPON_LIGHTSABER));
					pChr->SetActiveWeapon(WEAPON_LIGHTSABER);
				}
				return true;
			}
			if(IsOption(pVote, ADMIN_MISC_PORTALGUN))
			{
				if(pChr)
				{
					pChr->GiveWeapon(WEAPON_PORTALGUN, pChr->GetWeaponGot(WEAPON_PORTALGUN));
					pChr->SetActiveWeapon(WEAPON_PORTALGUN);
				}
				return true;
			}
		}

		if(GetSubPage(ClientId) == SUB_ADMIN_COSMETICS)
		{
			if(IsOption(pVote, ADMIN_COSM_PICKUPPET))
			{
				pPl->SetPickupPet(!pPl->m_Cosmetics.m_PickupPet);
				return true;
			}
			if(IsOption(pVote, ADMIN_COSM_STAFFIND))
			{
				pPl->SetStaffInd(!pPl->m_Cosmetics.m_StaffInd);
				return true;
			}
			if(IsOption(pVote, ADMIN_COSM_HEARTGUN))
			{
				if(pChr)
					pChr->GiveWeapon(WEAPON_HEARTGUN, pChr->GetWeaponGot(WEAPON_HEARTGUN));
				return true;
			}
			if(IsOption(pVote, ADMIN_ABILITY_HEART))
			{
				pPl->SetAbility(pPl->m_Cosmetics.m_Ability == ABILITY_HEART ? ABILITY_NONE : ABILITY_HEART);
				return true;
			}
			if(IsOption(pVote, ADMIN_ABILITY_SHIELD))
			{
				pPl->SetAbility(pPl->m_Cosmetics.m_Ability == ABILITY_SHIELD ? ABILITY_NONE : ABILITY_SHIELD);
				return true;
			}
			if(IsOption(pVote, ADMIN_ABILITY_FIREWORK))
			{
				pPl->SetAbility(pPl->m_Cosmetics.m_Ability == ABILITY_FIREWORK ? ABILITY_NONE : ABILITY_FIREWORK);
				return true;
			}
			if(IsOption(pVote, ADMIN_ABILITY_TELEKINESIS))
			{
				pPl->SetAbility(pPl->m_Cosmetics.m_Ability == ABILITY_TELEKINESIS ? ABILITY_NONE : ABILITY_TELEKINESIS);
				return true;
			}
		}
	}

	if(Page != PAGE_VOTES)
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
	SetPage(ClientId, PAGE_VOTES);
}

void CVoteMenu::UpdatePages(int ClientId)
{
	const int Page = GetPage(ClientId);

	bool Changes = false;

	if(!IsPageAllowed(ClientId, Page))
	{
		if(Page != PAGE_VOTES)
			SetPage(ClientId, PAGE_VOTES);
		return;
	}
	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const CAccountSession *pAcc = &GameServer()->m_aAccounts[ClientId];
	CAccountSession &OldAcc = m_aClientData[ClientId].m_Account;

	if(pAcc->m_LoggedIn != m_aClientData[ClientId].m_Account.m_LoggedIn) // Check login status change
		Changes = true;

	if(Page == PAGE_VOTES)
		return;

	if(Page == PAGE_SETTINGS)
	{
		if(pAcc->m_Flags != OldAcc.m_Flags)
			Changes = true;
	}
	if(Page == PAGE_ACCOUNT)
	{
		if(pAcc->m_Level != OldAcc.m_Level)
			Changes = true;
		if(pAcc->m_XP != OldAcc.m_XP)
			Changes = true;
		if(pAcc->m_Playtime != OldAcc.m_Playtime)
			Changes = true;
		if(pAcc->m_Money != OldAcc.m_Money)
			Changes = true;
		if(pAcc->m_Deaths != OldAcc.m_Deaths)
			Changes = true;
	}
	if(Page == PAGE_SHOP)
	{
		if(pAcc->m_Money != OldAcc.m_Money)
			Changes = true;
		if(memcmp(pAcc->m_Inventory, OldAcc.m_Inventory, sizeof(pAcc->m_Inventory)) != 0)
			Changes = true;

		// if(mem_comp(&Acc, &m_aClientData[ClientId].m_Account, sizeof(Acc)) != 0)
		//	Changes = true;
	}
	if(Page == PAGE_INVENTORY || Page == PAGE_ADMIN)
	{
		if(memcmp(pAcc->m_Inventory, OldAcc.m_Inventory, sizeof(pAcc->m_Inventory)) != 0)
			Changes = true;
		if(memcmp(&pPl->m_Cosmetics, &m_aClientData[ClientId].m_Cosmetics, sizeof(pPl->m_Cosmetics)) != 0)
			Changes = true;
	}

	if(Changes)
	{
		m_aClientData[ClientId].m_Account = GameServer()->m_aAccounts[ClientId];
		m_aClientData[ClientId].m_Cosmetics = pPl->m_Cosmetics;
		PrepareVoteOptions(ClientId, Page);
	}
}

bool CVoteMenu::IsPageAllowed(int ClientId, int Page) const
{
	if(Page < 0 || Page >= NUM_PAGES)
		return true;

	const CAccountSession *pAcc = &GameServer()->m_aAccounts[ClientId];

	if(IsFlagSet(g_Config.m_SvVoteMenuFlags, Page) && Page != PAGE_ADMIN)
		return false;
	if(Page == PAGE_ADMIN && Server()->GetAuthedState(ClientId) < AUTHED_MOD) // Allow Mod Access
		return false;
	if(!pAcc->m_LoggedIn && (Page == PAGE_SHOP || Page == PAGE_INVENTORY))
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
	case PAGE_VOTES: GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex = 0; return;
	case PAGE_SETTINGS: SendPageSettings(ClientId); break;
	case PAGE_ACCOUNT: SendPageAccount(ClientId); break;
	case PAGE_SHOP: SendPageShop(ClientId); break;
	case PAGE_INVENTORY: SendPageInventory(ClientId); break;
	case PAGE_ADMIN: SendPageAdmin(ClientId); break;
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
		return PAGE_VOTES;
	return m_aClientData[ClientId].m_Page;
}

void CVoteMenu::SetPage(int ClientId, int Page)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(Page < 0 || Page >= NUM_PAGES)
		return;

	m_aClientData[ClientId].m_Page = Page;
	CAccountSession &Acc = GameServer()->m_aAccounts[ClientId];
	if(Acc.m_LoggedIn)
		Acc.m_VoteMenuPage = Page;

	PrepareVoteOptions(ClientId, Page);
}

void CVoteMenu::SendPageSettings(int ClientId)
{
	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const CAccountSession *pAcc = &GameServer()->m_aAccounts[ClientId];

	if(pAcc->m_LoggedIn)
		AddVoteCheckBox(SETTINGS_AUTO_LOGIN, pAcc->m_Flags & ACC_FLAG_AUTOLOGIN);
	AddVoteCheckBox(SETTINGS_HIDE_COSMETICS, pPl->m_HideCosmetics);
	AddVoteCheckBox(SETTINGS_HIDE_POWERUPS, pPl->m_HidePowerUps);
}

void CVoteMenu::SendPageAccount(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const CAccountSession *pAcc = &GameServer()->m_aAccounts[ClientId];

	if(!pAcc->m_LoggedIn)
	{
		AddVoteText("You are not logged in.");
		AddVoteSeperator();
		AddVoteText("1 - use /register <Name> <Password> <Password>");
		AddVoteText("2 - login using /login <Name> <Password>");
		return;
	}

	char aBuf[VOTE_DESC_LENGTH];

	AddVoteText("╭─────────       Pʀᴏғɪʟᴇ");
	str_format(aBuf, sizeof(aBuf), "│ Account Name: %s", pAcc->m_Username);
	AddVoteText(aBuf);
	str_format(aBuf, sizeof(aBuf), "│ Last Player Name: %s", pAcc->m_LastName);
	AddVoteText(aBuf);

	char TimeBuf[64];

	// Register Date
	if(FormatUnixTime(pAcc->m_RegisterDate, TimeBuf, sizeof(TimeBuf), "%Y-%m-%d %H:%M:%S"))
	{
		str_format(aBuf, sizeof(aBuf), "│ Register Date: %s", TimeBuf);
		AddVoteText(aBuf);
	}
	else
	{
		AddVoteText("│ Register Date: n/a");
	}

	AddVoteText("├──────      Sᴛᴀᴛs");

	str_format(aBuf, sizeof(aBuf), "│ Level [%d]", pAcc->m_Level);
	AddVoteText(aBuf);

	int CurXp = pAcc->m_XP;
	int NeededXp = GameServer()->m_AccountManager.NeededXP(pAcc->m_Level);

	str_format(aBuf, sizeof(aBuf), "│ XP [%d/%d]", CurXp, NeededXp);
	AddVoteText(aBuf);

	float PlayTimeHours = pAcc->m_Playtime / 60.0f;
	str_format(aBuf, sizeof(aBuf), "│ Playtime: %.1f Hour%s", PlayTimeHours, PlayTimeHours == 1 ? "" : "s");
	if(pAcc->m_Playtime < 100)
		str_format(aBuf, sizeof(aBuf), "│ Playtime: %lld Minute%s", pAcc->m_Playtime, pAcc->m_Playtime == 1 ? "" : "s");
	AddVoteText(aBuf);

	str_format(aBuf, sizeof(aBuf), "│ %s: %lld", g_Config.m_SvCurrencyName, pAcc->m_Money);
	AddVoteText(aBuf);

	str_format(aBuf, sizeof(aBuf), "│ Deaths: %d", pAcc->m_Deaths);
	AddVoteText(aBuf);

	AddVoteText("╰──────────────────────────");
}

void CVoteMenu::SendPageShop(int ClientId)
{
	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const CAccountSession *pAcc = &GameServer()->m_aAccounts[ClientId];

	if(!pAcc->m_LoggedIn)
	{
		AddVoteText("You are not logged in.");
		AddVoteSeperator();
		AddVoteText("1 - use /register <Name> <Password> <Password>");
		AddVoteText("2 - login using /login <Name> <Password>");
		return;
	}
	std::vector<std::string> RainbowItems;
	std::vector<std::string> GunItems;
	std::vector<std::string> IndicatorItems;
	std::vector<std::string> KillEffectItems;
	std::vector<std::string> TrailItems;
	std::vector<std::string> OtherItems;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s: %lld", g_Config.m_SvCurrencyName, pAcc->m_Money);
	AddVoteText(aBuf);
	AddVoteSeperator();

	for(const auto &pItems : GameServer()->m_Shop.m_Items)
	{
		if(!str_comp(pItems->Name(), ""))
			continue;
		if(pItems->Price() == -1)
			continue;
		if(pPl->OwnsItem(pItems->Name()))
			continue;

		const char *pVoteName = FormatItemVote(pItems, pAcc);

		if(pItems->Type() == TYPE_RAINBOW)
			RainbowItems.push_back(std::string(pVoteName));
		else if(pItems->Type() == TYPE_GUN)
			GunItems.push_back(std::string(pVoteName));
		else if(pItems->Type() == TYPE_INDICATOR)
			IndicatorItems.push_back(std::string(pVoteName));
		else if(pItems->Type() == TYPE_DEATHS)
			KillEffectItems.push_back(std::string(pVoteName));
		else if(pItems->Type() == TYPE_TRAIL)
			TrailItems.push_back(std::string(pVoteName));
		else
			OtherItems.push_back(std::string(pVoteName));
	}
	if(!RainbowItems.empty())
	{
		AddVoteSubheader("Rᴀɪɴʙᴏᴡ");
		for(const auto &Item : RainbowItems)
		{
			AddVoteText(Item.c_str());
		}
		AddVoteSeperator();
	}
	if(!GunItems.empty())
	{
		AddVoteSubheader("Gᴜɴs");
		for(const auto &Item : GunItems)
		{
			AddVoteText(Item.c_str());
		}
		AddVoteSeperator();
	}
	if(!IndicatorItems.empty())
	{
		AddVoteSubheader("Gᴜɴ Hɪᴛ Eғғᴇᴄᴛs");
		for(const auto &Item : IndicatorItems)
		{
			AddVoteText(Item.c_str());
		}
		AddVoteSeperator();
	}
	if(!KillEffectItems.empty())
	{
		AddVoteSubheader("Dᴇᴀᴛʜ Eғғᴇᴄᴛs");
		for(const auto &Item : KillEffectItems)
		{
			AddVoteText(Item.c_str());
		}
		AddVoteSeperator();
	}
	if(!TrailItems.empty())
	{
		AddVoteSubheader("Tʀᴀɪʟs");
		for(const auto &Item : TrailItems)
		{
			AddVoteText(Item.c_str());
		}
		AddVoteSeperator();
	}
	if(!OtherItems.empty())
	{
		AddVoteSubheader("Oᴛʜᴇʀ");
		for(const auto &Item : OtherItems)
		{
			AddVoteText(Item.c_str());
		}
	}
}

void CVoteMenu::SendPageInventory(int ClientId)
{
	// CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	const CAccountSession *pAcc = &GameServer()->m_aAccounts[ClientId];
	if(!pAcc->m_LoggedIn)
	{
		AddVoteText("You are not logged in.");
		AddVoteSeperator();
		AddVoteText("1 - use /register <Name> <Password> <Password>");
		AddVoteText("2 - login using /login <Name> <Password>");
		return;
	}

	AddVoteSubheader("Cᴏsᴍᴇᴛɪᴄs");
	AddVoteSeperator();

	DoCosmeticVotes(ClientId, false);
}

void CVoteMenu::DoCosmeticVotes(int ClientId, bool Authed)
{
	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];

	// !New Cosmetic
	std::vector<std::string> RainbowItems;
	std::vector<std::string> GunItems;
	std::vector<std::string> IndicatorItems;
	std::vector<std::string> KillEffectItems;
	std::vector<std::string> TrailItems;
	std::vector<std::string> OtherItems;

	for(const auto &pItems : GameServer()->m_Shop.m_Items)
	{
		if(!Authed)
		{
			if(!str_comp(pItems->Name(), ""))
				continue;
			if(pItems->Price() == -1)
				continue;
			if(!(pPl->OwnsItem(pItems->Name())))
				continue;
		}

		if(pItems->Type() == TYPE_RAINBOW)
			RainbowItems.push_back(std::string(pItems->Name()));
		else if(pItems->Type() == TYPE_GUN)
			GunItems.push_back(std::string(pItems->Name()));
		else if(pItems->Type() == TYPE_INDICATOR)
			IndicatorItems.push_back(std::string(pItems->Name()));
		else if(pItems->Type() == TYPE_DEATHS)
			KillEffectItems.push_back(std::string(pItems->Name()));
		else if(pItems->Type() == TYPE_TRAIL)
			TrailItems.push_back(std::string(pItems->Name()));
		else
			OtherItems.push_back(std::string(pItems->Name()));
	}

	if(Authed)
	{
		AddVoteSubheader("Uɴᴀᴠᴀɪʟᴀʙʟᴇ");
		AddVoteCheckBox(ADMIN_COSM_PICKUPPET, pPl->m_Cosmetics.m_PickupPet);
		AddVoteCheckBox(ADMIN_COSM_STAFFIND, pPl->m_Cosmetics.m_StaffInd);
		if(pPl->GetCharacter())
			AddVoteCheckBox(ADMIN_COSM_HEARTGUN, pPl->GetCharacter()->GetWeaponGot(WEAPON_HEARTGUN));
		AddVoteSeperator();

		AddVoteSubheader("Aʙɪʟɪᴛɪᴇs");
		AddVoteCheckBox(ADMIN_ABILITY_HEART, pPl->m_Cosmetics.m_Ability == ABILITY_HEART);
		AddVoteCheckBox(ADMIN_ABILITY_SHIELD, pPl->m_Cosmetics.m_Ability == ABILITY_SHIELD);
		AddVoteCheckBox(ADMIN_ABILITY_FIREWORK, pPl->m_Cosmetics.m_Ability == ABILITY_FIREWORK);
		AddVoteCheckBox(ADMIN_ABILITY_TELEKINESIS, pPl->m_Cosmetics.m_Ability == ABILITY_TELEKINESIS);
		AddVoteSeperator();
	}

	if(!RainbowItems.empty())
	{
		AddVoteSubheader("Rᴀɪɴʙᴏᴡ");
		AddVoteValueOption("Rainbow Speed", pPl->m_Cosmetics.m_RainbowSpeed, 20, BULLET_ARROW);
		for(const auto &Item : RainbowItems)
		{
			AddVoteCheckBox(Item.c_str(), pPl->ItemEnabled(Item.c_str()));
		}
		AddVoteSeperator();
	}
	if(!GunItems.empty())
	{
		AddVoteSubheader("Gᴜɴs");
		for(const auto &Item : GunItems)
		{
			if(!str_comp(Item.c_str(), "Emoticon Gun"))
				AddVoteValueOption(Item.c_str(), pPl->m_Cosmetics.m_EmoticonGun, 16, BULLET_NONE);
			else
				AddVoteCheckBox(Item.c_str(), pPl->ItemEnabled(Item.c_str()));
		}
		AddVoteSeperator();
	}
	if(!IndicatorItems.empty())
	{
		AddVoteSubheader("Gᴜɴ Hɪᴛ Eғғᴇᴄᴛs");
		for(const auto &Item : IndicatorItems)
		{
			AddVoteCheckBox(Item.c_str(), pPl->ItemEnabled(Item.c_str()));
		}
		AddVoteSeperator();
	}
	if(!KillEffectItems.empty())
	{
		AddVoteSubheader("Dᴇᴀᴛʜ Eғғᴇᴄᴛs");
		for(const auto &Item : KillEffectItems)
		{
			AddVoteCheckBox(Item.c_str(), pPl->ItemEnabled(Item.c_str()));
		}
		AddVoteSeperator();
	}
	if(!TrailItems.empty())
	{
		AddVoteSubheader("Tʀᴀɪʟs");
		for(const auto &Item : TrailItems)
		{
			AddVoteCheckBox(Item.c_str(), pPl->ItemEnabled(Item.c_str()));
		}
		AddVoteSeperator();
	}
	if(!OtherItems.empty())
	{
		AddVoteSubheader("Oᴛʜᴇʀ");
		for(const auto &Item : OtherItems)
		{
			AddVoteCheckBox(Item.c_str(), pPl->ItemEnabled(Item.c_str()));
		}
	}
}

int CVoteMenu::GetSubPage(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return 0;
	const int Page = m_aClientData[ClientId].m_Page;
	return m_aClientData[ClientId].m_SubPage[Page];
}

void CVoteMenu::SetSubPage(int ClientId, int SubPage)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	const int Page = m_aClientData[ClientId].m_Page;
	if(Page < 0 || Page >= NUM_PAGES)
		return;

	m_aClientData[ClientId].m_SubPage[Page] = SubPage;
	PrepareVoteOptions(ClientId, Page);
}

bool CVoteMenu::CanUseCmd(int ClientId, const char *pCmd) const
{
	const IConsole::CCommandInfo *pInfo = GameServer()->Console()->GetCommandInfo(pCmd, CFGFLAG_SERVER, false);
	if(!pInfo)
		return false;

	int Required = pInfo->GetAccessLevel();
	int ClientLevel = IConsole::ACCESS_LEVEL_USER;
	switch(Server()->GetAuthedState(ClientId))
	{
	case AUTHED_ADMIN: ClientLevel = IConsole::ACCESS_LEVEL_ADMIN; break;
	case AUTHED_MOD: ClientLevel = IConsole::ACCESS_LEVEL_MOD; break;
	case AUTHED_HELPER: ClientLevel = IConsole::ACCESS_LEVEL_HELPER; break;
	default: ClientLevel = IConsole::ACCESS_LEVEL_USER; break;
	}
	return Required >= ClientLevel;
}

void CVoteMenu::SendPageAdmin(int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);

	AddVoteSubheader("Aᴅᴍɪɴ Pᴀɢᴇs");
	AddVotePrefix(ADMIN_UTIL, GetSubPage(ClientId) == SUB_ADMIN_UTIL ? BULLET_BLACK_DIAMOND : BULLET_WHITE_DIAMOND);
	AddVotePrefix(ADMIN_COSMETICS, GetSubPage(ClientId) == SUB_ADMIN_COSMETICS ? BULLET_BLACK_DIAMOND : BULLET_WHITE_DIAMOND);
	AddVotePrefix(ADMIN_MISC, GetSubPage(ClientId) == SUB_ADMIN_MISC ? BULLET_BLACK_DIAMOND : BULLET_WHITE_DIAMOND);
	AddVoteSeperator();
	if(GetSubPage(ClientId) == SUB_ADMIN_UTIL)
	{
		if(pChr && CanUseCmd(ClientId, "invincible"))
			AddVoteCheckBox(ADMIN_UTIL_INVINCIBLE, pChr->Core()->m_Invincible);
		if(CanUseCmd(ClientId, "spider_hook"))
			AddVoteCheckBox(ADMIN_UTIL_SPIDERHOOK, pPlayer->m_SpiderHook);
		if(CanUseCmd(ClientId, "vanish"))
			AddVoteCheckBox(ADMIN_UTIL_VANISH, pPlayer->m_Vanish);
		if(pChr)
		{
			if(CanUseCmd(ClientId, "telekinesis"))
				AddVoteCheckBox(ADMIN_UTIL_TELEKINESIS, pChr->GetWeaponGot(WEAPON_TELEKINESIS));
			if(CanUseCmd(ClientId, "telekinesis_immunity"))
				AddVoteCheckBox(ADMIN_UTIL_TELEK_IMMUNITY, pPlayer->m_TelekinesisImmunity);
			AddVoteSeperator();

			if(CanUseCmd(ClientId, "passive"))
			{
				AddVoteCheckBox(ADMIN_UTIL_PASSIVE, pChr->Core()->m_Passive);
				AddVoteSeperator();
			}

			if(CanUseCmd(ClientId, "collidable") && CanUseCmd(ClientId, "hittable") && CanUseCmd(ClientId, "hookable"))
			{
				AddVoteText("Should your own character be:");
				AddVoteCheckBox(ADMIN_UTIL_COLLIDABLE, pChr->Core()->m_Collidable);
				AddVoteCheckBox(ADMIN_UTIL_HITTABLE, pChr->Core()->m_Hittable);
				AddVoteCheckBox(ADMIN_UTIL_HOOKABLE, pChr->Core()->m_Hookable);
			}
		}
	}
	if(GetSubPage(ClientId) == SUB_ADMIN_MISC)
	{
		AddVoteSubheader("Mɪsᴄᴇʟʟᴀɴᴇᴏᴜs");
		if(pChr)
		{
			if(CanUseCmd(ClientId, "snake"))
				AddVoteCheckBox(ADMIN_MISC_SNAKE, pChr->m_Snake.Active());
			if(CanUseCmd(ClientId, "ufo"))
				AddVoteCheckBox(ADMIN_MISC_UFO, pChr->m_Ufo.Active());
		}

		AddVoteSeperator();
		if(CanUseCmd(ClientId, "obfuscate"))
			AddVoteCheckBox(ADMIN_MISC_OBFUSCATED, pPlayer->m_Obfuscated);

		if(CanUseCmd(ClientId, "ignore_gamelayer"))
			AddVoteCheckBox(ADMIN_MISC_IGN_KILL_BORD, pPlayer->m_IgnoreGamelayer);

		AddVoteSeperator();
		if(pChr)
		{
			if(CanUseCmd(ClientId, "heartgun"))
				AddVoteCheckBox(ADMIN_MISC_HEARTGUN, pChr->GetWeaponGot(WEAPON_HEARTGUN));
			if(CanUseCmd(ClientId, "lightsaber"))
				AddVoteCheckBox(ADMIN_MISC_LIGHTSABER, pChr->GetWeaponGot(WEAPON_LIGHTSABER));
			if(CanUseCmd(ClientId, "Portalgun"))
				AddVoteCheckBox(ADMIN_MISC_PORTALGUN, pChr->GetWeaponGot(WEAPON_PORTALGUN));
		}

	}
	if(GetSubPage(ClientId) == SUB_ADMIN_COSMETICS)
	{
		DoCosmeticVotes(ClientId, true);
	}
}

void CVoteMenu::AddHeader(int ClientId)
{
	const int NumVotesToSend = NUM_PAGES + 3;

	std::vector<std::string> Descriptions;

	int CurIndex = 0;
	for(int i = 0; i < NumVotesToSend; i++)
	{
		if(!IsPageAllowed(ClientId, i))
			continue;

		if(i == NUM_PAGES)
			Descriptions.emplace_back(EMPTY_DESC);
		else if(i == NUM_PAGES + 1)
			Descriptions.emplace_back(FANCY_LINES_DESC);
		else if(i == NUM_PAGES + 2)
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

void CVoteMenu::AddVotePrefix(const char *pDesc, int Prefix)
{
	const char *pPrefixes[] = {"", "•", "─", ">", "⇨", "⁃", "‣", "◆", "◇"};

	if(Prefix < 0 || Prefix >= (int)std::size(pPrefixes))
	{
		AddVoteText(pDesc);
		return;
	}
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %s", pPrefixes[Prefix], pDesc);
	AddVoteText(aBuf);
}

void CVoteMenu::AddVoteValueOption(const char *pDescription, int Value, int Max, int BulletPoint)
{
	char aBuf[VOTE_DESC_LENGTH];
	if(Max == -1)
	{
		str_format(aBuf, sizeof(aBuf), "%s: %d", pDescription, Value);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%s: %d/%d", pDescription, Value, Max);
	}
	AddVotePrefix(aBuf, BulletPoint);
}

void CVoteMenu::AddVoteSubheader(const char *pDesc)
{
	char aBuf[128];
	// str_format(aBuf, sizeof(aBuf), "═─═ %s ═─═", pDesc);
	str_format(aBuf, sizeof(aBuf), "─── %s ───", pDesc);
	AddVoteText(aBuf);
}

void CVoteMenu::AddVoteCheckBox(const char *pDesc, bool Checked)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %s", Checked ? "☒" : "☐", pDesc);
	AddVoteText(aBuf);
}