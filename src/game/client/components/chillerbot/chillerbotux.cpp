// ChillerDragon 2020 - chillerbot ux

#include <engine/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/race.h>
#include <game/client/render.h>
#include <game/generated/protocol.h>

#include "chillerbotux.h"

void CChillerBotUX::OnRender()
{
	if(m_LastGreet < time_get())
	{
		m_LastGreet = 0;
		m_aGreetName[0] = '\0';
	}
	FinishRenameTick();
}

void CChillerBotUX::FinishRenameTick()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClFinishRename)
		return;
	vec2 Pos = m_pClient->m_PredictedChar.m_Pos;
	if(CRaceHelper::IsNearFinish(m_pClient, Pos))
	{
		if(Client()->State() == IClient::STATE_ONLINE && !m_pClient->m_pMenus->IsActive() && g_Config.m_ClEditor == 0)
		{
			Graphics()->BlendNormal();
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 0, 0.5f);
			RenderTools()->DrawRoundRect(10.0f, 30.0f, 150.0f, 50.0f, 10.0f);
			Graphics()->QuadsEnd();
			TextRender()->Text(0, 20.0f, 30.f, 20.0f, "chillerbot-ux", -1);
			TextRender()->Text(0, 50.0f, 60.f, 10.0f, "finish rename", -1);
		}
		if(!m_IsNearFinish)
		{
			m_IsNearFinish = true;
			m_pClient->SendFinishName();
		}
	}
	else
	{
		m_IsNearFinish = false;
	}
}

bool CChillerBotUX::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_find_nocase(pLine, pName);

	if(pHL)
	{
		int Length = str_length(pName);

		if((pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}

	return false;
}

bool CChillerBotUX::IsGreeting(const char *pMsg)
{
	const char aGreetings[][128] = {
		"hi",
		"hay",
		"hey",
		"heey",
		"heeey",
		"heeeey",
		"haay",
		"haaay",
		"haaaay",
		"haaaaay",
		"henlo",
		"hello",
		"hallo",
		"hellu",
		"hallu",
		"helu",
		"henlu",
		"hemnlo",
		"herro",
		"moin",
		"servus",
		"guten tag",
		"priviet",
		"ola",
		"ay",
		"ayy",
		"ayyy",
		"ayyyy",
		"aayyy",
		"aaay",
		"aaaay",
		"yo",
		"yoo",
		"yooo",
		"sup",
		"selam"};
	for(const auto &aGreeting : aGreetings)
	{
		const char *pHL = str_find_nocase(pMsg, aGreeting);
		while(pHL)
		{
			int Length = str_length(aGreeting);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '1' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, aGreeting);
		}
	}
	return false;
}

void CChillerBotUX::OnInit()
{
	m_aGreetName[0] = '\0';
	m_LastGreet = 0;
}

void CChillerBotUX::OnConsoleInit()
{
	Console()->Register("say_hi", "", CFGFLAG_CLIENT, ConSayHi, this, "Respond to the last greeting in chat");
}

void CChillerBotUX::ConSayHi(IConsole::IResult *pResult, void *pUserData)
{
	((CChillerBotUX *)pUserData)->DoGreet();
}

void CChillerBotUX::DoGreet()
{
	if(m_aGreetName[0])
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "hi %s", m_aGreetName);
		m_pClient->m_pChat->Say(0, aBuf);
		return;
	}
	m_pClient->m_pChat->Say(0, "hi");
}

void CChillerBotUX::Get128Name(const char *pMsg, char *pName)
{
	for(int i = 0; pMsg[i] && i < 17; i++)
	{
		if(pMsg[i] == ':')
		{
			str_copy(pName, pMsg, i + 1);
			return;
		}
	}
	str_copy(pName, " ", 2);
}

void CChillerBotUX::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
	bool Highlighted = false;
	if(LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		Highlighted = true;
	if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		Highlighted = true;
	if(!Highlighted)
		return;

	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
	if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientID].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		return;
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		return;
	if(IsGreeting(pMsg))
	{
		str_copy(m_aGreetName, aName, sizeof(m_aGreetName));
		m_LastGreet = time_get() + time_freq() * 10;
	}
}

void CChillerBotUX::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}
