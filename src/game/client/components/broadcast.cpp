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
	m_BroadcastRenderOffset = -1.0f;
	m_ClientBroadcastRenderOffset = -1.0f;
	m_ClientBroadcastTime = 0.0f;
	TextRender()->DeleteTextContainer(m_TextContainerIndex);
	TextRender()->DeleteTextContainer(m_ClientTextContainerIndex);
}

void CBroadcast::OnWindowResize()
{
	m_BroadcastRenderOffset = -1.0f;
	m_ClientBroadcastRenderOffset = -1.0f;
	TextRender()->DeleteTextContainer(m_TextContainerIndex);
	TextRender()->DeleteTextContainer(m_ClientTextContainerIndex);
}

void CBroadcast::OnConsoleInit()
{
	Console()->Register("client_broadcast", "r[text]", CFGFLAG_CLIENT, ConClientBroadcast, this, "Show client side broadcast");
}

void CBroadcast::ConClientBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CBroadcast *pSelf = (CBroadcast *)pUserData;
	pSelf->DoClientBroadcast(pResult->GetString(0));
}

void CBroadcast::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderServerBroadcast();
	RenderClientBroadcast();
}

void CBroadcast::RenderClientBroadcast()
{
	if(Client()->LocalTime() >= m_ClientBroadcastTime)
	{
		TextRender()->DeleteTextContainer(m_ClientTextContainerIndex);
		return;
	}

	const float Height = 340.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	if(m_ClientBroadcastRenderOffset < 0.0f)
		m_ClientBroadcastRenderOffset = Width / 2.0f - TextRender()->TextWidth(12.0f, m_aClientBroadcastText, -1, Width) / 2.0f;

	if(!m_ClientTextContainerIndex.Valid())
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, m_ClientBroadcastRenderOffset, 20.0f, 12.0f, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Width;
		TextRender()->CreateTextContainer(m_ClientTextContainerIndex, &Cursor, m_aClientBroadcastText);
	}
	if(m_ClientTextContainerIndex.Valid())
	{
		ColorRGBA TextColor = ColorRGBA(1, 1, 0.5f, 1);
		ColorRGBA OutlineColor = TextRender()->DefaultTextOutlineColor();
		TextRender()->RenderTextContainer(m_ClientTextContainerIndex, TextColor, OutlineColor);
	}
}

void CBroadcast::DoClientBroadcast(const char *pText)
{
	str_copy(m_aClientBroadcastText, pText);
	TextRender()->DeleteTextContainer(m_ClientTextContainerIndex);
	m_BroadcastRenderOffset = -1.0f;
	m_ClientBroadcastTime = Client()->LocalTime() + 10.0f;
}

void CBroadcast::RenderServerBroadcast()
{
	if(m_pClient->m_Scoreboard.Active() || m_pClient->m_Motd.IsActive() || !g_Config.m_ClShowBroadcasts)
		return;
	const float SecondsRemaining = (m_BroadcastTick - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
	if(SecondsRemaining <= 0.0f)
	{
		TextRender()->DeleteTextContainer(m_TextContainerIndex);
		return;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	if(m_BroadcastRenderOffset < 0.0f)
		m_BroadcastRenderOffset = Width / 2.0f - TextRender()->TextWidth(12.0f, m_aBroadcastText, -1, Width) / 2.0f;

	if(!m_TextContainerIndex.Valid())
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, m_BroadcastRenderOffset, 40.0f, 12.0f, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Width;
		TextRender()->CreateTextContainer(m_TextContainerIndex, &Cursor, m_aBroadcastText);
	}
	if(m_TextContainerIndex.Valid())
	{
		const float Alpha = SecondsRemaining >= 1.0f ? 1.0f : SecondsRemaining;
		ColorRGBA TextColor = TextRender()->DefaultTextColor();
		TextColor.a *= Alpha;
		ColorRGBA OutlineColor = TextRender()->DefaultTextOutlineColor();
		OutlineColor.a *= Alpha;
		TextRender()->RenderTextContainer(m_TextContainerIndex, TextColor, OutlineColor);
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
	str_copy(m_aBroadcastText, pMsg->m_pMessage);
	m_BroadcastTick = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;
	m_BroadcastRenderOffset = -1.0f;
	TextRender()->DeleteTextContainer(m_TextContainerIndex);

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
