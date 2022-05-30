// chillerbot-ux
// The stresser component
// used to stress test servers and find crashbugs

#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <game/client/gameclient.h>

#include "stresser.h"

void CStresser::OnInit()
{
	m_RequestCmdlist = 0;
	m_PenDelay = 0;
	for(auto &SendData : m_SendData)
		SendData = false;
}

void CStresser::OnMapLoad()
{
	m_RequestCmdlist = -(time_get() + time_freq() * 5); // wait a few seconds before requesting it
}

void CStresser::OnConsoleInit()
{
	Console()->Chain("cl_pentest", ConchainPenTest, this);
}

void CStresser::ConchainPenTest(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CStresser *pSelf = (CStresser *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	for(auto &SendData : pSelf->m_SendData)
		SendData = false;
}

void CStresser::RandomMovements()
{
	float t = Client()->LocalTime();
	mem_zero(&m_InputData[g_Config.m_ClDummy], sizeof(m_InputData[0]));

	m_InputData[g_Config.m_ClDummy].m_Direction = rand() % 3 - 1;
	m_InputData[g_Config.m_ClDummy].m_Jump = ((int)t);
	m_InputData[g_Config.m_ClDummy].m_Fire = ((int)(t * 10));
	m_InputData[g_Config.m_ClDummy].m_Hook = ((int)(t * 2)) & 1;
	m_InputData[g_Config.m_ClDummy].m_WantedWeapon = ((int)t) % NUM_WEAPONS;
	m_InputData[g_Config.m_ClDummy].m_TargetX = (int)(sinf(t * 3) * 100.0f);
	m_InputData[g_Config.m_ClDummy].m_TargetY = (int)(cosf(t * 3) * 100.0f);

	m_SendData[g_Config.m_ClDummy] = true;
}

void CStresser::OnRender()
{
	if(!Config()->m_ClPenTest)
		return;

	RandomMovements();

	if(m_RequestCmdlist < 0) // waiting to send
	{
		if(time_get() >= -m_RequestCmdlist)
		{
			m_pClient->m_Chat.SayChat("/cmdlist");
			m_RequestCmdlist = time_get();
		}
		// dbg_msg("pentest", "time=%lld req=%lld diff=%lld", time_get(), -m_RequestCmdlist, (-m_RequestCmdlist - time_get()) / time_freq());
		return;
	}
	else if(m_RequestCmdlist)
	{
		int64_t SecsPassed = -(m_RequestCmdlist - time_get()) / time_freq();
		// str_format(aBuf, sizeof(aBuf), "sent message %lld secs ago", SecsPassed);
		// m_pChat->SayChat(aBuf);
		if(SecsPassed > 1)
		{
			char aBuf[2048];
			str_copy(aBuf, "", sizeof(aBuf));
			for(const auto &ChatCmd : m_vChatCmds)
			{
				str_append(aBuf, ChatCmd, sizeof(aBuf));
				// dbg_msg("pentest", "append chat cmd=%s", ChatCmd);
			}
			// m_pChat->SayChat(aBuf);
			dbg_msg("pentest", "found chat cmds=%s", aBuf);
			// m_pChat->SayChat("stopped waiting.");
			m_RequestCmdlist = 0; // finished waiting for response
		}
		return;
	}

	m_PenDelay--;
	if(m_PenDelay > 0)
		return;
	m_PenDelay = 100 + rand() % 50;

	// chat messages
	if(rand() % 2) // parsed chat cmds
	{
		char aChatCmd[128];
		char aArg[64];
		static const char *pCharset = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"§$%&/()=?{[]}\\<>|-.,;:+#*'~'@_/";
		str_copy(aArg, "", sizeof(aArg));
		int len = rand() % 64;
		for(int i = 0; i < len; i++)
		{
			char buf[2];
			str_format(buf, sizeof(buf), "%c", pCharset[rand() % str_length(pCharset)]);
			str_append(aArg, buf, sizeof(aArg));
		}
		str_format(aChatCmd, sizeof(aChatCmd), "/%s %s", GetRandomChatCommand(), aArg);
		m_pClient->m_Chat.SayChat(aChatCmd);
	}
	else // file messages
	{
		const char *pMessage = GetPentestCommand(g_Config.m_ClPenTestFile);
		if(pMessage)
		{
			m_pClient->m_Chat.SayChat(pMessage);
		}
		else
		{
			const int NUM_CMDS = 3;
			char aaCmds[NUM_CMDS][512] = {
				"/register xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx 12831237189237189231982371938712893798",
				"todo: configure me ( pentest file not found)",
				"/login bang baz baz"};
			m_pClient->m_Chat.SayChat(aaCmds[rand() % NUM_CMDS]);
		}
	}
}

const char *CStresser::GetRandomChatCommand()
{
	if(m_vChatCmds.empty())
		return 0;
	return m_vChatCmds[rand() % m_vChatCmds.size()];
}

const char *CStresser::GetPentestCommand(char const *pFileName)
{
	IOHANDLE File = Storage()->OpenFile(pFileName, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
		return 0;

	std::vector<char *> v;
	char *pLine;
	CLineReader *lr = new CLineReader();
	lr->Init(File);
	while((pLine = lr->Get()))
		if(str_length(pLine))
			if(pLine[0] != '#')
				v.push_back(pLine);
	io_close(File);
	static const char *pCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"§$%&/()=?{[]}\\<>|-.,;:+#*'~'@_/";
	char *pMessage = v[rand() % v.size()];
	for(int i = 0; pMessage[i] != 0; i++)
	{
		if(pMessage[i] == '?')
			pMessage[i] = pCharset[rand() % str_length(pCharset)];
	}
	return pMessage;
}

void CStresser::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}

void CStresser::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
	char aBuf[2048];
	if(ClientID == -1) // server message
	{
		if(m_RequestCmdlist > 0)
		{
			// int64 SecsPassed = -(m_pClient->m_RequestCmdlist - time_get()) / time_freq();
			// str_format(aBuf, sizeof(aBuf), "sent message %lld secs ago", SecsPassed);
			// Say(0, aBuf);
			// dbg_msg("pentest", "cmdlist: %s", pMsg);
			str_copy(aBuf, pMsg, sizeof(aBuf));
			char aToken[64];
			for(const char *tok = aBuf; (tok = str_next_token(tok, ",", aToken, sizeof(aToken)));)
			{
				char *pBuf = (char *)malloc(64 * sizeof(char));
				str_copy(pBuf, str_skip_whitespaces(aToken), 64);
				m_vChatCmds.push_back(pBuf);
				// dbg_msg("pentest", "found chat command: %s", pBuf);
			}
		}
	}
}
