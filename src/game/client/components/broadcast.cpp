/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>

#include <game/client/components/motd.h>
#include <game/client/components/scoreboard.h>

#include "broadcast.h"

void CBroadcast::OnReset()
{
	std::fill(std::begin(m_aBroadcastTick), std::end(m_aBroadcastTick), 0);
	DeleteBroadcastContainer();
}

void CBroadcast::OnWindowResize()
{
	DeleteBroadcastContainer();
}

void CBroadcast::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderServerBroadcast();
}

void CBroadcast::RenderServerBroadcast()
{
	if(GameClient()->m_Scoreboard.IsActive() || GameClient()->m_Motd.IsActive() || !g_Config.m_ClShowBroadcasts)
		return;

	int ActiveBroadcastIndex;
	float SecondsRemaining = (m_aBroadcastTick[g_Config.m_ClDummy] - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
	if(SecondsRemaining <= 0.0f)
	{
		if(SecondsRemaining == 0.0f)
		{
			DeleteBroadcastContainer();
		}

		SecondsRemaining = (m_aBroadcastTick[!g_Config.m_ClDummy] - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
		if(SecondsRemaining <= 0.0f)
		{
			DeleteBroadcastContainer();
			return;
		}
		else
		{
			ActiveBroadcastIndex = (int)!g_Config.m_ClDummy;
		}
	}
	else
	{
		ActiveBroadcastIndex = g_Config.m_ClDummy;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	if(m_BroadcastRenderOffset < 0.0f)
		m_BroadcastRenderOffset = Width / 2.0f - TextRender()->TextWidth(12.0f, m_aaBroadcastText[ActiveBroadcastIndex], -1, Width) / 2.0f;

	if(!m_TextContainerIndex.Valid())
	{
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(m_BroadcastRenderOffset, 40.0f));
		Cursor.m_FontSize = 12.0f;
		Cursor.m_LineWidth = Width;
		TextRender()->CreateTextContainer(m_TextContainerIndex, &Cursor, m_aaBroadcastText[ActiveBroadcastIndex]);
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
		const CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
		DoBroadcast(pMsg->m_pMessage, false);
	}
}

void CBroadcast::DeleteBroadcastContainer()
{
	m_BroadcastRenderOffset = -1.0f;
	TextRender()->DeleteTextContainer(m_TextContainerIndex);
}

void CBroadcast::DoDummyBroadcast(const char *pText)
{
	DoBroadcast(pText, true);
}

void CBroadcast::DoBroadcast(const char *pText, bool Dummy)
{
	int Conn = (Dummy != (bool)g_Config.m_ClDummy);
	m_aBroadcastTick[Conn] = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;
	str_copy(m_aaBroadcastText[Conn], pText);
	DeleteBroadcastContainer();

	if(g_Config.m_ClPrintBroadcasts)
	{
		char aLine[sizeof(m_aaBroadcastText[0])];
		while((pText = str_next_token(pText, "\n", aLine, sizeof(aLine))))
		{
			if(aLine[0] != '\0')
			{
				log_info_color(IConsole::ColorToLogColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor))),
					Dummy ? "dummy broadcast" : "broadcast", "%s", aLine);
			}
		}
	}
}
