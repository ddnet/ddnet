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
}

int CCityHelper::WalletMoney(int ClientID)
{
	if(ClientID == -1)
		ClientID = g_Config.m_ClDummy;
	return m_WalletMoney[ClientID];
};

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
		m_WalletMoney[g_Config.m_ClDummy] = atoi(aAmount);
	}
}

void CCityHelper::DropAllMoney(int ClientID)
{
	if(!WalletMoney(ClientID))
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "/money drop %d", WalletMoney(ClientID) > 100000 ? 100000 : WalletMoney(ClientID));

	CMsgPacker Msg(NETMSGTYPE_CL_SAY, false);
	Msg.AddInt(0);
	Msg.AddString(aBuf, -1);
	Client()->SendMsgY(&Msg, MSGFLAG_VITAL, ClientID);
}

void CCityHelper::OnRender()
{
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
