// ChillerDragon 2021 - chillerbot ux

#include <base/vmath.h>
#include <engine/config.h>
#include <engine/shared/config.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include "chillerbotux.h"

#include "city.h"

void CCityHelper::OnInit()
{
	m_AutoDropMoney[0] = false;
	m_AutoDropMoney[1] = false;
	m_WalletDropDelay[0] = 1;
	m_WalletDropDelay[1] = 1;
	m_NextWalletDrop[0] = 0;
	m_NextWalletDrop[1] = 0;
	m_LastDummy = 0;
}

void CCityHelper::SetAutoDrop(bool Drop, int Delay, int ClientID)
{
	char aBuf[128];
	m_AutoDropMoney[ClientID] = Drop;
	m_WalletDropDelay[ClientID] = Delay;
	str_format(aBuf, sizeof(aBuf), "turn drop %s on %s", Drop ? "on" : "off", ClientID ? "dummy" : "main");
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "city", aBuf);
	str_format(aBuf, sizeof(aBuf), "drop main=%d dummy=%d", m_AutoDropMoney[0], m_AutoDropMoney[1]);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "city", aBuf);
}

void CCityHelper::OnConsoleInit()
{
	Console()->Register("auto_drop_money", "?i[delay]?i[dummy]?s[on|off]", CFGFLAG_CLIENT, ConAutoDropMoney, this, "");

	Console()->Chain("cl_show_wallet", ConchainShowWallet, this);
}

void CCityHelper::ConchainShowWallet(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CWarList *pSelf = (CWarList *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->GameClient()->m_ChillerBotUX.EnableComponent("money");
	else
		pSelf->GameClient()->m_ChillerBotUX.DisableComponent("money");
}

void CCityHelper::PrintWalletToChat(int ClientID)
{
	if(ClientID == -1)
		ClientID = g_Config.m_ClDummy;

	char aWallet[128];
	char aBuf[128];
	str_format(aWallet, sizeof(aWallet), "money: %d", WalletMoney(ClientID));
	if(!ClientID)
	{
		for(auto &Entry : m_vWalletMain)
		{
			str_format(aBuf, sizeof(aBuf), ", \"%s\": %d", Entry.first.c_str(), Entry.second);
			str_append(aWallet, aBuf, sizeof(aWallet));
		}
	}
	else
	{
		for(auto &Entry : m_vWalletDummy)
		{
			str_format(aBuf, sizeof(aBuf), ", \"%s\": %d", Entry.first.c_str(), Entry.second);
			str_append(aWallet, aBuf, sizeof(aWallet));
		}
	}
	m_pClient->m_Chat.Say(0, aWallet);
}

int CCityHelper::WalletMoney(int ClientID)
{
	if(ClientID == -1)
		ClientID = g_Config.m_ClDummy;
	if(m_WalletMoney[ClientID] == 0)
	{
		if(ClientID)
			m_vWalletDummy.clear();
		else
			m_vWalletMain.clear();
	}
	return m_WalletMoney[ClientID];
}

void CCityHelper::SetWalletMoney(int Money, int ClientID)
{
	if(ClientID == -1)
		ClientID = g_Config.m_ClDummy;
	m_WalletMoney[ClientID] = Money;
	if(g_Config.m_ClShowWallet)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%d", m_WalletMoney[ClientID]);
		m_pClient->m_ChillerBotUX.SetComponentNoteLong("money", aBuf);
	}
}

void CCityHelper::AddWalletMoney(int Money, int ClientID)
{
	if(ClientID == -1)
		ClientID = g_Config.m_ClDummy;
	SetWalletMoney(WalletMoney(ClientID) + Money);
}

void CCityHelper::ConAutoDropMoney(IConsole::IResult *pResult, void *pUserData)
{
	CCityHelper *pSelf = (CCityHelper *)pUserData;
	int ClientID = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : g_Config.m_ClDummy;
	bool Drop = !pSelf->m_AutoDropMoney[ClientID];
	int Delay = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 4;
	if(pResult->NumArguments() > 2)
		Drop = !str_comp(pResult->GetString(2), "on");
	pSelf->SetAutoDrop(
		Drop,
		Delay < 1 ? 1 : Delay,
		ClientID);
}

void CCityHelper::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_BROADCAST)
	{
		CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
		const char *pMoney = str_find(pMsg->m_pMessage, "Money [");
		if(!pMoney)
			return;
		char aMoney[128] = {0};
		char aAmount[128] = {0};
		str_copy(aMoney, pMoney + 7, sizeof(aMoney));
		for(int i = 0; i < 128; i++)
		{
			if(aMoney[i] == ']' || aMoney[i] == '\0' || aMoney[i] == '\n')
			{
				aAmount[i] = '\0';
				break;
			}
			aAmount[i] = aMoney[i];
		}
		SetWalletMoney(atoi(aAmount));
		;
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(pMsg->m_ClientID == -1)
			OnServerMsg(pMsg->m_pMessage);
		else
			OnChatMsg(pMsg->m_ClientID, pMsg->m_pMessage);
	}
}

void CCityHelper::AddWalletEntry(std::vector<std::pair<std::string, int>> *pVec, const std::pair<std::string, int> &Entry)
{
	for(auto &E : *pVec)
	{
		if(E.first == Entry.first)
		{
			E.second += Entry.second;
			return;
		}
	}
	pVec->push_back(Entry);
}

void CCityHelper::OnServerMsg(const char *pMsg)
{
	int Money = 0;
	int n = sscanf(pMsg, "Collected %d money", &Money);
	if(n == 1)
	{
		int Owner = ClosestClientIDToPos(
			vec2(m_pClient->m_LocalCharacterPos.x, m_pClient->m_LocalCharacterPos.y),
			g_Config.m_ClDummy);
		if(Owner != -1)
		{
			const char *pName = m_pClient->m_aClients[Owner].m_aName;
			std::pair<std::string, int> Pair;
			Pair.first = std::string(pName);
			Pair.second = Money;
			if(!g_Config.m_ClDummy)
				AddWalletEntry(&m_vWalletMain, Pair);
			else
				AddWalletEntry(&m_vWalletDummy, Pair);
		}
		AddWalletMoney(Money);
		return;
	}
	n = sscanf(pMsg, "You withdrew %d money from your bank account to your wallet.", &Money);
	if(n == 1)
	{
		AddWalletMoney(Money);
		return;
	}
	n = sscanf(pMsg, "You deposited %d money from your wallet to your bank account.", &Money);
	if(n == 1)
	{
		AddWalletMoney(-Money);
		return;
	}
	n = sscanf(pMsg, "Wallet [%d]", &Money);
	if(n == 1)
	{
		SetWalletMoney(Money);
		return;
	}
}

int CCityHelper::ClosestClientIDToPos(vec2 Pos, int Dummy)
{
	float ClosestRange = 0.f;
	int ClosestId = -1;
	vec2 ClosestPos = vec2(-1, -1);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int ClientID = m_pClient->m_LocalIDs[Dummy];
		if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;
		if(ClientID == i)
			continue;

		const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, i);
		const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);

		if(pPrevInfo && pInfo)
		{
			vec2 otherPos = m_pClient->m_aClients[i].m_Predicted.m_Pos;
			float len = distance(otherPos, Pos);
			if(len < ClosestRange || !ClosestRange)
			{
				ClosestRange = len;
				ClosestPos = otherPos;
				ClosestId = i;
			}
		}
	}
	return ClosestId;
}

void CCityHelper::OnChatMsg(int ClientID, const char *pMsg)
{
	// TODO: move this to chat helper? or do I want a new chat command system in each component? -.-
	const char *pName = m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName;
	const char *pDummyName = m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName;
	int NameLen = 0;

	if(str_startswith(pMsg, pName))
		NameLen = str_length(pName);
	else if(m_pClient->Client()->DummyConnected() && str_startswith(pMsg, pDummyName))
		NameLen = str_length(pDummyName);

	if(!NameLen)
		return;

	char aMsg[2048];
	char aCmd[2048] = {0};
	str_copy(aMsg, pMsg + NameLen, sizeof(aMsg));
	for(unsigned int i = 0; i < sizeof(aMsg); i++)
	{
		if(aMsg[i] == ' ' || aMsg[i] == ':')
			continue;

		if(aMsg[i] == '!')
			str_copy(aCmd, &aMsg[i + 1], sizeof(aCmd));
		break;
	}
	if(aCmd[0] == '\0')
		return;
	// char aBuf[128];
	// str_format(aBuf, sizeof(aBuf), "cmd '%s'", aCmd);
	// m_pClient->m_Chat.Say(0, aBuf);
	if(!str_comp(aCmd, "wallet"))
		PrintWalletToChat();
}

void CCityHelper::DropAllMoney(int ClientID)
{
	if(WalletMoney(ClientID) < 1)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "/money drop %d", WalletMoney(ClientID) > 100000 ? 100000 : WalletMoney(ClientID));

	CMsgPacker Msg(NETMSGTYPE_CL_SAY, false);
	Msg.AddInt(0);
	Msg.AddString(aBuf, -1);
	Client()->SendMsg(ClientID, &Msg, MSGFLAG_VITAL);
}

void CCityHelper::OnRender()
{
	if(m_LastDummy != g_Config.m_ClDummy)
	{
		m_LastDummy = g_Config.m_ClDummy;
		SetWalletMoney(WalletMoney());
	}
	for(int i = 0; i < NUM_DUMMIES; i++)
	{
		if(!m_AutoDropMoney[i])
			continue;
		if(time_get() < m_NextWalletDrop[i])
			continue;

		m_NextWalletDrop[i] = time_get() + time_freq() * m_WalletDropDelay[i];
		DropAllMoney(i);
	}
}
