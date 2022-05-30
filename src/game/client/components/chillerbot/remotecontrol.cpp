// ChillerDragon 2021 - chillerbot ux

#include <game/generated/protocol.h>

#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chathelper.h>
#include <game/client/components/chillerbot/chillerbotux.h>
#include <game/client/gameclient.h>

#include <engine/serverbrowser.h>

#include "remotecontrol.h"

void CRemoteControl::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
	if(!g_Config.m_ClRemoteControl)
		return;

	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
	if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
	{
		m_pClient->m_ChatHelper.Get128Name(pMsg, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		return;
	if(Team != 3) // whisper only
		return;

	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);
	bool IsFDDRace = !str_comp(ServerInfo.m_aGameType, "F-DDrace");

	char aBuf[128];
	int Num = 0;
	char aMsg[3][2048] = {{0}};
	for(int i = 0, k = 0; pMsg[i]; i++, k++)
	{
		char c = pMsg[i];
		if(c == ' ' && Num < (IsFDDRace ? 2 : 1))
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
		m_pClient->m_ChatHelper.SayBuffer(aBuf);
		return;
	}
	else if(Num == 1 && IsFDDRace)
	{
		str_format(aBuf, sizeof(aBuf), "Error: %s missing command (usage: '/whisper name token command')", aName);
		m_pClient->m_ChatHelper.SayBuffer(aBuf);
		return;
	}
	if(!str_comp(aMsg[IsFDDRace ? 1 : 0], g_Config.m_ClRemoteControlTokenAdmin))
		m_pClient->Console()->ExecuteLine(aMsg[IsFDDRace ? 2 : 1]);
	else if(!str_comp(aMsg[IsFDDRace ? 1 : 0], g_Config.m_ClRemoteControlToken))
		ExecuteWhitelisted(aMsg[IsFDDRace ? 2 : 1]);
	else
	{
		dbg_msg("chillerbot", "whisper='%s'", pMsg);
		dbg_msg(
			"chillerbot",
			"Error: %s failed to remote control (invalid token attempt='%s' token='%s' admin='%s')",
			aName, aMsg[IsFDDRace ? 1 : 0], g_Config.m_ClRemoteControlToken, g_Config.m_ClRemoteControlTokenAdmin);
		str_format(
			aBuf,
			sizeof(aBuf),
			"Error: %s failed to remote control (invalid token)",
			aName);
		m_pClient->m_ChatHelper.SayBuffer(aBuf);
		return;
	}
}

void CRemoteControl::ExecuteWhitelisted(const char *pCommand, const char *pWhitelistFile)
{
	if(!Storage())
		return;

	// exec the file
	IOHANDLE File = Storage()->OpenFile(pWhitelistFile, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[128];
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open remote control whitelist file '%s'", pWhitelistFile);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
		return;
	}
	char *pLine;
	CLineReader Reader;

	str_format(aBuf, sizeof(aBuf), "loading remote control whitelist file '%s'", pWhitelistFile);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
	Reader.Init(File);

	while((pLine = Reader.Get()))
	{
		if(!str_comp_nocase(pLine, pCommand))
		{
			str_format(aBuf, sizeof(aBuf), "executing whitelisted command '%s'", pCommand);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
			m_pClient->Console()->ExecuteLine(pCommand);
			io_close(File);
			return;
		}
	}

	str_format(aBuf, sizeof(aBuf), "command '%s' not whitelisted", pCommand);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
	io_close(File);
}

void CRemoteControl::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}
