#include "shop.h"
#include "../gamecontext.h"
#include "../player.h"
#include "accounts.h"
#include <algorithm>
#include <base/system.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <vector>

IServer *CShop::Server() const { return GameServer()->Server(); }

void CShop::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	if(m_Items.empty())
		AddItems();
}

void CShop::AddItems()
{
	m_Items.push_back(new CItems("Rainbow Feet", TYPE_RAINBOW, 750, 0));
	m_Items.push_back(new CItems("Rainbow Body", TYPE_RAINBOW, 1250, 0));
	m_Items.push_back(new CItems("Rainbow Hook", TYPE_RAINBOW, 1500, 0));

	m_Items.push_back(new CItems("Emoticon Gun", TYPE_GUN, 1550, 0));
	m_Items.push_back(new CItems("Confetti Gun", TYPE_GUN, 2000, 0));
	m_Items.push_back(new CItems("Phase Gun", TYPE_GUN, 1000, 0));

	m_Items.push_back(new CItems("Clockwise Indicator", TYPE_INDICATOR, 1500, 0));
	m_Items.push_back(new CItems("Counter Clockwise Indicator", TYPE_INDICATOR, 1500, 0));
	m_Items.push_back(new CItems("Inward Turning Indicator", TYPE_INDICATOR, 3000, 0));
	m_Items.push_back(new CItems("Outward Turning Indicator", TYPE_INDICATOR, 3000, 0));
	m_Items.push_back(new CItems("Line Indicator", TYPE_INDICATOR, 3250, 0));
	m_Items.push_back(new CItems("Criss Cross Indicator", TYPE_INDICATOR, 2500, 0));

	m_Items.push_back(new CItems("Explosive Death", TYPE_DEATHS, 1250, 0));
	m_Items.push_back(new CItems("Hammer Hit Death", TYPE_DEATHS, 1250, 0));
	m_Items.push_back(new CItems("Indicator Death", TYPE_DEATHS, 1750, 0));
	m_Items.push_back(new CItems("Laser Death", TYPE_DEATHS, 2250, 0));

	m_Items.push_back(new CItems("Star Trail", TYPE_TRAIL, 2500, 0));
	m_Items.push_back(new CItems("Dot Trail", TYPE_TRAIL, 2500, 0));

	m_Items.push_back(new CItems("Sparkle", TYPE_OTHER, 1000, 0));
	m_Items.push_back(new CItems("Heart Hat", TYPE_OTHER, 2250, 0));
	m_Items.push_back(new CItems("Inverse Aim", TYPE_OTHER, 3500, 0));
	m_Items.push_back(new CItems("Lovely", TYPE_OTHER, 2750, 0));
	m_Items.push_back(new CItems("Rotating Ball", TYPE_OTHER, 3500, 0));
	m_Items.push_back(new CItems("Epic Circle", TYPE_OTHER, 3500, 0));
	//m_Items.push_back(new CItems("Bloody", TYPE_OTHER, 1500, 0));
}

int CShop::GetItemPrice(const char *pName)
{
	for(CItems *pItem : m_Items)
	{
		if(!str_comp(pItem->Name(), ""))
			continue;

		if(!str_comp_nocase(pName, pItem->Name()) || !str_comp_nocase(ShortcutToName(pName), pItem->Name()))
		{
			return pItem->Price();
		}
	}

	return -1;
}

int CShop::GetItemMinLevel(const char *pName)
{
	for(CItems *pItem : m_Items)
	{
		if(!str_comp(pItem->Name(), ""))
			continue;

		if(!str_comp_nocase(pName, pItem->Name()) || !str_comp_nocase(ShortcutToName(pName), pItem->Name()))
		{
			return pItem->MinLevel();
		}
	}

	return -1;
}

const char *CShop::NameToShortcut(const char *pName)
{
	int Index = 0;
	for(const char *pItem : Items)
	{
		if(!str_comp_nocase(pItem, pName))
		{
			return ItemShortcuts[Index];
		}
		Index++;
	}

	return "";
}

const char *CShop::ShortcutToName(const char *pShortcut)
{
	int Index = 0;
	for(const char *pItemShortcut : ItemShortcuts)
	{
		if(!str_comp_nocase(pItemShortcut, pShortcut))
		{
			return Items[Index];
		}
		Index++;
	}

	return "";
}

void CShop::BuyItem(int ClientId, const char *pName)
{
	CAccountSession *pAcc = &GameServer()->m_Account[ClientId];
	const int Price = GetItemPrice(pName);
	const int MinLevel = GetItemMinLevel(pName);

	if(!pAcc->m_LoggedIn)
	{
		GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
		GameServer()->SendChatTarget(ClientId, "│ You aren't logged in");
		GameServer()->SendChatTarget(ClientId, "│ 1 - /register <Username> <Pw> <Pw>");
		GameServer()->SendChatTarget(ClientId, "│ 2 - /Login <Username> <Pw>");
		GameServer()->SendChatTarget(ClientId, "╰─────────────────────────────");
		return;
	}
	else if(GameServer()->m_apPlayers[ClientId]->OwnsItem(pName))
	{
		GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
		GameServer()->SendChatTarget(ClientId, "│ You already own that Item!");
		GameServer()->SendChatTarget(ClientId, "│ No refunds");
		GameServer()->SendChatTarget(ClientId, "╰───────────────────────────");
		return;
	}
	else if(Price == -1)
	{
		// This is used to completely disable an Item, also if it has already been bought
		GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
		GameServer()->SendChatTarget(ClientId, "│ Invalid Item!");
		GameServer()->SendChatTarget(ClientId, "│ Check out the Voting Menu");
		GameServer()->SendChatTarget(ClientId, "│ to see all available Items");
		GameServer()->SendChatTarget(ClientId, "╰───────────────────────────");
		return;
	}
	else if(Price == 0)
	{
		// Can be used for seasonal Items, Items can still be toggled
		GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
		GameServer()->SendChatTarget(ClientId, "│ Item is out of stock!");
		GameServer()->SendChatTarget(ClientId, "│ Ask an Admin to add it");
		GameServer()->SendChatTarget(ClientId, "╰───────────────────────");
		return;
	}
	else if(Price < -1)
	{
		GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
		GameServer()->SendChatTarget(ClientId, "│ Our systems are dying");
		GameServer()->SendChatTarget(ClientId, "│ Try again in a sec");
		GameServer()->SendChatTarget(ClientId, "╰───────────────────────");
		return;
	}

	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	if(!pPl)
		return;

	char aBuf[256];

	char ItemName[64];
	str_copy(ItemName, pName);
	if(str_comp(ShortcutToName(ItemName), "") != 0)
		str_copy(ItemName, ShortcutToName(ItemName));

	if(pAcc->m_Money < Price)
	{
		GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
		str_format(aBuf, sizeof(aBuf), "│ You don't have enough %s to buy %s", g_Config.m_SvCurrencyName, ItemName);
		GameServer()->SendChatTarget(ClientId, aBuf);
		str_format(aBuf, sizeof(aBuf), "│ You need atleast %d %s", Price, g_Config.m_SvCurrencyName);
		GameServer()->SendChatTarget(ClientId, aBuf);
		GameServer()->SendChatTarget(ClientId, "╰───────────────────────");
		return;
	}
	if(pAcc->m_Level < MinLevel)
	{
		GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
		str_format(aBuf, sizeof(aBuf), "│ You need atleast Level %d to buy %s", MinLevel, ItemName);
		GameServer()->SendChatTarget(ClientId, aBuf);
		str_format(aBuf, sizeof(aBuf), "│ You are currently Level %d", pAcc->m_Level);
		GameServer()->SendChatTarget(ClientId, aBuf);
		GameServer()->SendChatTarget(ClientId, "│");
		GameServer()->SendChatTarget(ClientId, "│ Level up by playing");
		GameServer()->SendChatTarget(ClientId, "│ or finishing Maps");
		GameServer()->SendChatTarget(ClientId, "╰───────────────────────");
		return;
	}

	pAcc->m_Money -= Price;

	char AddItem[256];
	str_format(AddItem, sizeof(AddItem), "%s ", NameToShortcut(ItemName));
	str_append(pAcc->m_Inventory, AddItem);

	GameServer()->SendChatTarget(ClientId, "╭──────     Sʜᴏᴘ");
	str_format(aBuf, sizeof(aBuf), "│ You bought \"%s\" for %d %s", ItemName, Price, g_Config.m_SvCurrencyName);
	GameServer()->SendChatTarget(ClientId, aBuf);
	str_format(aBuf, sizeof(aBuf), "│ You now have: %lld %s", pAcc->m_Money, g_Config.m_SvCurrencyName);
	GameServer()->SendChatTarget(ClientId, aBuf);
	GameServer()->SendChatTarget(ClientId, "╰───────────────────────");
	GameServer()->m_AccountManager.SaveAccountsInfo(ClientId, GameServer()->m_Account[ClientId]);
}