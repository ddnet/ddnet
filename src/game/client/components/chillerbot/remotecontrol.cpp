// ChillerDragon 2021 - chillerbot ux

#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chathelper.h>
#include <game/client/components/chillerbot/chillerbotux.h>

#include "remotecontrol.h"

void CRemoteControl::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
	if(!g_Config.m_ClRemoteControl)
		return;

	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
	if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
	{
		m_pClient->m_pChatHelper->Get128Name(pMsg, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		return;
	if(Team != 3) // whisper only
		return;

	char aBuf[128];
	int Num = 0;
	char aMsg[3][2048] = {{0}};
	for(int i = 0, k = 0; pMsg[i]; i++, k++)
	{
		char c = pMsg[i];
		if(c == ' ' && Num < 2)
		{
			Num++;
			k = -1;
			continue;
		}
		aMsg[Num][k] = c;
	}
	if(Num == 0)
	{
		str_format(aBuf, sizeof(aBuf), "Error: %s missing token (usage: '/whisper name token command')", aName);
		m_pClient->m_pChatHelper->SayBuffer(aBuf);
		return;
	}
	else if(Num == 1)
	{
		str_format(aBuf, sizeof(aBuf), "Error: %s missing command (usage: '/whisper name token command')", aName);
		m_pClient->m_pChatHelper->SayBuffer(aBuf);
		return;
	}
	if(str_comp(aMsg[1], g_Config.m_ClRemoteControlToken))
	{
		str_format(aBuf, sizeof(aBuf), "Error: %s failed to remote control (invalid token)", aName);
		m_pClient->m_pChatHelper->SayBuffer(aBuf);
		return;
	}
	m_pClient->Console()->ExecuteLine(aMsg[2]);
}

void CRemoteControl::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}
