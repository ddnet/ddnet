#ifndef GAME_SERVER_FOXNET_VOTEMENU_H
#define GAME_SERVER_FOXNET_VOTEMENU_H

#include "accounts.h"
#include <array>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>

class CGameContext;
class IServer;

enum Pages
{
	NONE = -1,

	VOTES = 0,
	SETTINGS,
	ACCOUNT,
	SHOP,
	INVENTORY,
	ADMIN,
	NUM_PAGES
};

enum Prefixes
{
	DOT = 0,
	DASH,
	GREATER_THAN,
	ARROW,
	RECTANGLE_BULLET,
	TRIANGLE_BULLET
};

enum Flags
{
	FLAG_VOTES = 1 << VOTES,
	FLAG_SETTINGS = 1 << SETTINGS,
	FLAG_ACCOUNT = 1 << ACCOUNT,
	FLAG_SHOP = 1 << SHOP,
	FLAG_INVENTORY = 1 << INVENTORY,
};

struct CVoteMenu
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	std::array<char[64], NUM_PAGES> m_aPages;
	struct ClientData
	{
		int m_Page = VOTES;

		// Comparison data for auto updates
		CAccountSession m_Account = CAccountSession();
	};
	ClientData m_aClientData[MAX_CLIENTS];
	std::vector<std::string> m_vDescriptions;

	bool IsPageAllowed(int ClientId, int Page) const;

	void PrepareVoteOptions(int ClientId, int Page);

	void AddDescription(const char *pDesc) { m_vDescriptions.emplace_back(pDesc); }
	void AddDescriptionPrefix(const char *pDesc, int Prefix);
	void AddDescriptionCheckBox(const char *pDesc, bool Checked);

	void SendPageSettings(int ClientId);
	void SendPageAccount(int ClientId);
	void SendPageShop(int ClientId);
	void SendPageInventory(int ClientId);
	void SendPageAdmin(int ClientId);

	void UpdatePages(int ClientId);

public:
	int GetPage(int ClientId) const;
	void SetPage(int ClientId, int Page);

	void AddHeader(int ClientId);

	void Tick();
	void OnClientDrop(int ClientId);
	void Init(CGameContext *pGameServer);
	bool OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId);
};

#endif // GAME_SERVER_FOXNET_VOTEMENU_H