/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
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
	std::fill(std::begin(m_BroadcastTick), std::end(m_BroadcastTick), 0);
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

	char aCurrentBroadcast[sizeof(m_aBroadcastText[0])];

	float SecondsRemaining = (m_BroadcastTick[g_Config.m_ClDummy] - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
	if(SecondsRemaining <= 0.0f)
	{
		if(SecondsRemaining == 0.0f)
		{
			DeleteBroadcastContainer();
		}

		SecondsRemaining = (m_BroadcastTick[!g_Config.m_ClDummy] - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
		if(SecondsRemaining <= 0.0f)
		{
			DeleteBroadcastContainer();
			return;
		}
		else
		{
			str_copy(aCurrentBroadcast, m_aBroadcastText[!g_Config.m_ClDummy]);
		}
	}
	else
	{
		str_copy(aCurrentBroadcast, m_aBroadcastText[g_Config.m_ClDummy]);
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	if(m_BroadcastRenderOffset < 0.0f)
		m_BroadcastRenderOffset = Width / 2.0f - TextRender()->TextWidth(12.0f, aCurrentBroadcast, -1, Width) / 2.0f;

	if(!m_TextContainerIndex.Valid())
	{
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(m_BroadcastRenderOffset, 40.0f));
		Cursor.m_FontSize = 12.0f;
		Cursor.m_LineWidth = Width;
		TextRender()->CreateTextContainer(m_TextContainerIndex, &Cursor, aCurrentBroadcast);
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
		DoBroadcast(pMsg->m_pMessage);
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
	m_BroadcastTick[Conn] = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;
	str_copy(m_aBroadcastText[Conn], pText);

	if(Dummy)
	{
		const float SecondsRemaining = (m_BroadcastTick[Conn] - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
		if(SecondsRemaining <= 0.0f)
			DeleteBroadcastContainer();
	}
	else
		DeleteBroadcastContainer();

	if(g_Config.m_ClPrintBroadcasts)
	{
		char aLine[sizeof(m_aBroadcastText[0])];
		while((pText = str_next_token(pText, "\n", aLine, sizeof(aLine))))
		{
			if(aLine[0] != '\0')
			{
				if(Dummy)
					GameClient()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dummy broadcast", aLine, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor)));
				else
					GameClient()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "broadcast", aLine, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor)));
			}
		}
	}
}
