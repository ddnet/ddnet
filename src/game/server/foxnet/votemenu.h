#ifndef GAME_SERVER_FOXNET_VOTEMENU_H
#define GAME_SERVER_FOXNET_VOTEMENU_H

#include <array>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>

class CGameContext;
class IServer;

enum Pages
{
	VOTES = 0,
	SETTINGS,
	ACCOUNT,
	SHOP,
	INVENTORY,
	ADMIN,
	NUM_PAGES
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
	} m_aClientData[MAX_CLIENTS];

public:

	int GetPage(int ClientId) const;
	void SetPage(int ClientId, int Page);

	void AddHeader(int ClientId);

	void Init(CGameContext *pGameServer);
	bool OnCallVote(const CNetMsg_Cl_CallVote *pMsg, int ClientId);
};

#endif // GAME_SERVER_FOXNET_VOTEMENU_H