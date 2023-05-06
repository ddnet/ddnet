/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include <game/client/components/motd.h>
#include <game/client/components/scoreboard.h>

#include "broadcast.h"

void CBroadcast::OnReset()
{
	m_BroadcastTick = 0;
}

void CBroadcast::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderServerBroadcast();
}

void CBroadcast::RenderServerBroadcast()
{
	if(m_pClient->m_Scoreboard.Active() || m_pClient->m_Motd.IsActive() || !g_Config.m_ClShowBroadcasts)
		return;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	if(Client()->GameTick(g_Config.m_ClDummy) < m_BroadcastTick)
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, m_BroadcastRenderOffset, 40.0f, 12.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Width - m_BroadcastRenderOffset;
		TextRender()->TextEx(&Cursor, m_aBroadcastText, -1);
	}
}

void CBroadcast::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_BROADCAST)
	{
		OnBroadcastMessage((CNetMsg_Sv_Broadcast *)pRawMsg);
	}
}

void CBroadcast::OnBroadcastMessage(const CNetMsg_Sv_Broadcast *pMsg)
{
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	str_copy(m_aBroadcastText, pMsg->m_pMessage);
	m_BroadcastRenderOffset = Width / 2.0f - TextRender()->TextWidth(12.0f, m_aBroadcastText, -1, Width, TEXTFLAG_STOP_AT_END) / 2.0f;
	m_BroadcastTick = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;

	if(g_Config.m_ClPrintBroadcasts)
	{
		const char *pText = m_aBroadcastText;
		char aLine[sizeof(m_aBroadcastText)];
		while((pText = str_next_token(pText, "\n", aLine, sizeof(aLine))))
		{
			if(aLine[0] != '\0')
			{
				m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "broadcast", aLine, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor)));
			}
		}
	}
}
