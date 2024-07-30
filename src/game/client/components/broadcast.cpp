/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/color.h>
#include <base/system.h>
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
	for(auto &TextContainerIndex : m_aTextContainerIndices)
		TextRender()->DeleteTextContainer(TextContainerIndex);
	for(auto &Segment : m_aSegments)
		Segment.m_TextIndex = -1;
}

void CBroadcast::OnWindowResize()
{
	m_BroadcastRenderOffset = -1.0f;
	for(auto &TextContainerIndex : m_aTextContainerIndices)
		TextRender()->DeleteTextContainer(TextContainerIndex);
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
	const float SecondsRemaining = (m_BroadcastTick - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
	if(SecondsRemaining <= 0.0f)
	{
		for(auto &TextContainerIndex : m_aTextContainerIndices)
			TextRender()->DeleteTextContainer(TextContainerIndex);
		return;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	if(m_BroadcastRenderOffset < 0.0f)
		m_BroadcastRenderOffset = Width / 2.0f - TextRender()->TextWidth(12.0f, m_aBroadcastText, -1, Width) / 2.0f;

	CTextCursor Cursor;
	if(!m_aTextContainerIndices[0].Valid())
	{
		TextRender()->SetCursor(&Cursor, m_BroadcastRenderOffset, 40.0f, 12.0f, TEXTFLAG_RENDER);
	}
	for(int SegmentIndex = 0; SegmentIndex <= MAX_BROADCAST_COLOR_SEGMENTS; SegmentIndex++)
	{
		const CBroadcastSegment &Segment = m_aSegments[SegmentIndex];
		auto &TextContainerIndex = m_aTextContainerIndices[SegmentIndex];
		if(Segment.m_TextIndex == -1)
			break;

		if(!TextContainerIndex.Valid())
		{
			Cursor.m_LineWidth = Width;
			int Length = -1;
			if(SegmentIndex < MAX_BROADCAST_COLOR_SEGMENTS)
				Length = m_aSegments[SegmentIndex + 1].m_TextIndex - Segment.m_TextIndex;
			TextRender()->CreateTextContainer(TextContainerIndex, &Cursor, &m_aBroadcastText[Segment.m_TextIndex], Length);
		}
		if(TextContainerIndex.Valid())
		{
			const float Alpha = SecondsRemaining >= 1.0f ? 1.0f : SecondsRemaining;
			ColorRGBA TextColor = Segment.m_Color;
			TextColor.a *= Alpha;
			ColorRGBA OutlineColor = TextRender()->DefaultTextOutlineColor();
			OutlineColor.a *= Alpha;
			TextRender()->RenderTextContainer(TextContainerIndex, TextColor, OutlineColor);
		}
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
	OnReset();
	m_BroadcastTick = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;

	int MsgLength = str_length(pMsg->m_pMessage);
	int ServerMsgLen = 0;

	int NumSegments = 0;
	m_aSegments[NumSegments].m_Color = TextRender()->DefaultTextColor();
	m_aSegments[NumSegments].m_TextIndex = ServerMsgLen;
	NumSegments++;

	// parse colors
	for(int i = 0; i < MsgLength && ServerMsgLen < MAX_BROADCAST_MSG_SIZE - 1; i++)
	{
		const char *c = pMsg->m_pMessage + i;

		if(*c == '\\' && c[1] == '^')
		{
			m_aBroadcastText[ServerMsgLen++] = c[1];
			i++; // skip '^'
			continue;
		}

		if(*c == '^' && i + 3 < MsgLength && str_isnum(c[1]) && str_isnum(c[2]) && str_isnum(c[3]))
		{
			m_aSegments[NumSegments].m_Color = ColorRGBA((c[1] - '0') / 9.0f, (c[2] - '0') / 9.0f, (c[3] - '0') / 9.0f, 1.0f);
			m_aSegments[NumSegments].m_TextIndex = ServerMsgLen;
			NumSegments++;
			i += 3; // skip color code
			continue;
		}

		m_aBroadcastText[ServerMsgLen++] = *c;
	}
	m_aBroadcastText[ServerMsgLen] = '\0';

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
