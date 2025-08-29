#ifndef GAME_SERVER_FOXNET_VOTEMENU_H
#define GAME_SERVER_FOXNET_VOTEMENU_H

#include "accounts.h"
#include <array>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/server/player.h>

class CGameContext;
class IServer;

enum Pages
{
	PAGE_NONE = -1,

	PAGE_VOTES = 0,
	PAGE_SETTINGS,
	PAGE_ACCOUNT,
	PAGE_SHOP,
	PAGE_INVENTORY,
	PAGE_ADMIN,
	NUM_PAGES,
};

enum AdminSubPages
{
	SUB_ADMIN_UTIL = 0,
	SUB_ADMIN_MISC,
	SUB_ADMIN_COSMETICS,
	NUM_SUB_PAGES,
};

enum BulletPoints
{
	BULLET_NONE = 0,
	BULLET_POINT,
	BULLET_DASH,
	BULLET_GREATER_THAN,
	BULLET_ARROW,
	BULLET_HYPHEN,
	BULLET_TRIANGLE,
	BULLET_BLACK_DIAMOND,
	BULLET_WHITE_DIAMOND
};

enum Flags
{
	FLAG_VOTES = 1 << PAGE_VOTES,
	FLAG_SETTINGS = 1 << PAGE_SETTINGS,
	FLAG_ACCOUNT = 1 << PAGE_ACCOUNT,
	FLAG_SHOP = 1 << PAGE_SHOP,
	FLAG_INVENTORY = 1 << PAGE_INVENTORY,
};

class CVoteMenu
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	std::array<char[64], NUM_PAGES> m_aPages;
	struct ClientData
	{
		int m_Page = PAGE_VOTES;
		int m_SubPage[NUM_PAGES] = {0};

		// Comparison data for auto updates
		CAccountSession m_Account = CAccountSession();
		CCosmetics m_Cosmetics = CCosmetics();
	};
	ClientData m_aClientData[MAX_CLIENTS];
	std::vector<std::string> m_vDescriptions;

	bool IsPageAllowed(int ClientId, int Page) const;

	bool IsOptionWithSuffix(const char *pDesc, const char *pWantedOption) { return str_startswith(pDesc, pWantedOption) != 0; }
	bool IsOption(const char *pDesc, const char *pWantedOption) { return !str_comp(pDesc, pWantedOption); }

	void AddVoteText(const char *pDesc) { m_vDescriptions.emplace_back(pDesc); }
	void AddVoteSeperator() { m_vDescriptions.emplace_back(" "); }
	void AddVoteSubheader(const char *pDesc);
	void AddVotePrefix(const char *pDesc, int Prefix);
	void AddVoteCheckBox(const char *pDesc, bool Checked);
	void AddVoteValueOption(const char *pDescription, int Value, int Max, int BulletPoint);

	void SendPageSettings(int ClientId);
	void SendPageAccount(int ClientId);
	void SendPageShop(int ClientId);
	void SendPageInventory(int ClientId);
	void SendPageAdmin(int ClientId);

	void DoCosmeticVotes(int ClientId, bool Authed);

	void UpdatePages(int ClientId);

	int GetSubPage(int ClientId) const;
	void SetSubPage(int ClientId, int Page);

public:
	void PrepareVoteOptions(int ClientId, int Page);

	int GetPage(int ClientId) const;
	void SetPage(int ClientId, int Page);

	void AddHeader(int ClientId);

	void Tick();
	void OnClientDrop(int ClientId);
	void Init(CGameContext *pGameServer);
	bool OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId);
	bool IsCustomVoteOption(const CNetMsg_Cl_CallVote *pMsg, int ClientId);
};

#endif // GAME_SERVER_FOXNET_VOTEMENU_H